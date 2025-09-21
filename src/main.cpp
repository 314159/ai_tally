#include "config.h"
#include "gui_manager.h"
#include "platform_interface.h"
#include "tally_monitor.h"
#include "websocket_server.h"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <memory>
#include <thread>

namespace {
std::unique_ptr<atem::HttpAndWebSocketServer> server;
std::unique_ptr<atem::TallyMonitor> monitor;
std::unique_ptr<atem::GuiManager> gui;
} // namespace
namespace po = boost::program_options;

int main(int argc, char* argv[])
{
    try {
        // Initialize platform-specific code
        if (!platform::initialize()) {
            std::cerr << "Failed to initialize platform\n";
            return 1;
        }

        boost::asio::io_context io_context;

        // --- Configuration ---
        atem::Config config;
        std::string config_file = "config/server_config.json";

        // Load from file first
        config.load_from_file(config_file);

        // Then, define and parse command-line options.
        // These will override the file settings.
        po::options_description desc("Allowed options");
        desc.add_options()("help,h", "produce help message")(
            "config,c",
            po::value<std::string>(&config_file)->default_value(config_file),
            "Path to configuration file")(
            "listen-address", po::value<std::string>(&config.ws_address),
            "WebSocket server listen address")(
            "listen-port", po::value<unsigned short>(&config.ws_port),
            "WebSocket server listen port")(
            "atem-ip", po::value<std::string>(&config.atem_ip),
            "ATEM switcher IP address")(
            "mock-inputs", po::value<uint16_t>(&config.mock_inputs),
            "Number of inputs to show in mock mode");
        bool no_gui = false;
        desc.add_options()("no-gui", po::bool_switch(&no_gui), "Disable the GUI (useful for headless/server runs)");

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << "ATEM Tally WebSocket Server\n"
                      << desc << "\n";
            return 0;
        }

        bool enable_gui = !no_gui;

        const auto address = boost::asio::ip::make_address(config.ws_address);
        const unsigned short port = config.ws_port;

        std::cout << "Starting ATEM Tally WebSocket Server...\n";
        std::cout << "Platform: " << platform::get_platform_name() << "\n";
        std::cout << "Using configuration file: " << config_file << "\n";
        std::cout << "Listening on " << address << ":" << port << "\n";
        std::cout << "Connecting to ATEM at " << config.atem_ip << "\n";

        // Setup signal handling with Boost.Asio
        // Setup signal handling with Boost.Asio. When a signal is received,
        // post a shutdown task to the io_context so shutdown happens in a
        // well-defined order on the main thread.
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait(
            [&](const boost::system::error_code& error, int signal_number) {
                if (!error) {
                    std::cout << "\nReceived signal " << signal_number
                              << ", scheduling shutdown...\n";

                    // Post orderly shutdown to the io_context so it runs
                    // in the IO thread (or the main thread if IO is run
                    // on main). This avoids tearing down objects from the
                    // signal handler or arbitrary threads.
                    boost::asio::post(io_context, [&]() {
                        std::cout << "[main] posted shutdown task to io_context\n";

                        // Stop accepting/processing new work
                        try {
                            if (server) {
                                std::cout << "[main] calling server->stop()\n";
                                server->stop();
                                std::cout << "[main] server->stop() returned\n";
                            }
                            if (monitor) {
                                std::cout << "[main] calling monitor->stop()\n";
                                monitor->stop();
                                std::cout << "[main] monitor->stop() returned\n";
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "Error during posted shutdown: " << e.what() << std::endl;
                        }

                        // If GUI was running, request it to stop. GUI loop runs
                        // on the main thread, so this is safe to call here and
                        // the GUI will exit its loop, allowing main to continue
                        // and destruct objects in the right order.
                        if (gui) {
                            std::cerr << "[main] requesting gui->stop()\n";
                            gui->stop();
                        }

                        // Ask the io_context to stop after posted shutdown tasks
                        if (!io_context.stopped()) {
                            std::cerr << "[main] calling io_context.stop()\n";
                            io_context.stop();
                        }
                    });
                }
            });

        // Create tally monitor
        monitor = std::make_unique<atem::TallyMonitor>(io_context, config.atem_ip, config.mock_inputs);

        // Create server
        server = std::make_unique<atem::HttpAndWebSocketServer>(io_context, address, port, config, *monitor);

        // Create GUI (optional)
        if (enable_gui) {
            gui = std::make_unique<atem::GuiManager>();
            if (!gui->init()) {
                std::cerr << "Failed to initialize GUI\n";
                return 1;
            }
        } else {
            std::cout << "GUI disabled (--no-gui); running headless server.\n";
        }

        // Connect tally updates to websocket broadcasts and GUI
        monitor->on_tally_change([&](const atem::TallyUpdate& update) {
            std::ostringstream oss;
            oss << "[main] tally callback on thread " << std::this_thread::get_id() << " for input " << update.input_id << "\n";
            std::cout << oss.str();
            if (server)
                server->broadcast_tally_update(update);
            if (gui)
                gui->update_tally_state(update);
        });

        // Start services
        server->start();
        monitor->start();

        std::thread server_thread;
        if (enable_gui) {
            // Run the IO context in a separate thread while GUI runs on main
            server_thread = std::thread([&io_context]() {
                try {
                    io_context.run();
                } catch (const std::exception& e) {
                    std::cerr << "Server thread error: " << e.what() << std::endl;
                }
            });

            // Run the GUI loop on the main thread. When GUI->stop() is
            // called (via the posted shutdown above), run_loop() will exit
            // and main continues to orderly cleanup below.
            gui->run_loop();

            // Ensure the io_context is stopped and the server thread finishes.
            if (!io_context.stopped()) {
                io_context.stop();
            }
            if (server_thread.joinable()) {
                server_thread.join();
            }

            // Deterministically tear down objects in main thread order.
            // Destroy server, then monitor, then GUI to avoid races where
            // background IO threads try to access objects while they are
            // being destroyed.
            try {
                server.reset();
            } catch (...) {
            }
            try {
                monitor.reset();
            } catch (...) {
            }
            try {
                gui.reset();
            } catch (...) {
            }
        } else {
            // No GUI: run the IO context on the main thread so the process stays alive
            io_context.run();
            // io_context.run() returned -> shutdown initiated
            // Ensure resources are torn down in main thread order.
            try {
                server.reset();
            } catch (...) {
            }
            try {
                monitor.reset();
            } catch (...) {
            }
            try {
                gui.reset();
            } catch (...) {
            }
        }

        std::cout << "Application stopped.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    platform::cleanup();
    return 0;
}