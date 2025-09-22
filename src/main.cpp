#include "config.h"
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

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << "ATEM Tally WebSocket Server\n"
                      << desc << "\n";
            return 0;
        }

        const auto address = boost::asio::ip::make_address(config.ws_address);
        const unsigned short port = config.ws_port;

        std::cout << "Starting ATEM Tally WebSocket Server...\n";
        std::cout << "Platform: " << platform::get_platform_name() << "\n";
        std::cout << "Using configuration file: " << config_file << "\n";
        std::cout << "Listening on " << address << ":" << port << "\n";
        std::cout << "Connecting to ATEM at " << config.atem_ip << "\n";
        std::cout << "Running in headless server mode.\n";

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

        // Connect tally updates to websocket broadcasts and GUI
        monitor->on_tally_change([&](const atem::TallyUpdate& update) {
            if (server)
                server->broadcast_tally_update(update);
        });

        // Start services
        server->start();
        monitor->start();

        // Run the IO context on the main thread. This will block until a
        // shutdown is initiated (e.g., by a signal).
        io_context.run();
        
        // io_context.run() returned, so shutdown was initiated.
        // The unique_ptrs will now be destroyed in reverse order of declaration,
        // ensuring an orderly teardown.
        try {
            server.reset();
            monitor.reset();
        } catch (const std::exception& e) {
            std::cerr << "Error during final cleanup: " << e.what() << std::endl;
        }

        std::cout << "Application stopped.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    platform::cleanup();
    return 0;
}