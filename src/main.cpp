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

        // Setup signal handling with Boost.Asio
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait(
            [&](const boost::system::error_code& error, int signal_number) {
                if (!error) {
                    std::cout << "\nReceived signal " << signal_number
                              << ", shutting down gracefully...\n";
                    if (gui)
                        gui->stop();
                    if (monitor)
                        monitor->stop();
                    if (server)
                        server->stop();
                    // The io_context will stop naturally when all work is done.
                    if (!io_context.stopped()) {
                        io_context.stop();
                    }
                }
            });

        // Create tally monitor
        monitor = std::make_unique<atem::TallyMonitor>(io_context, config.atem_ip, config.mock_inputs);

        // Create server
        server = std::make_unique<atem::HttpAndWebSocketServer>(io_context, address, port, config, *monitor);

        // Create GUI
        gui = std::make_unique<atem::GuiManager>();
        if (!gui->init()) {
            std::cerr << "Failed to initialize GUI\n";
            return 1;
        }

        // Connect tally updates to websocket broadcasts and GUI
        monitor->on_tally_change([&](const atem::TallyUpdate& update) {
            if (server)
                server->broadcast_tally_update(update);
            if (gui)
                gui->update_tally_state(update);
        });

        // Start services
        server->start();
        monitor->start();

        // Run the IO context in a separate thread
        std::thread server_thread([&io_context]() {
            try {
                io_context.run();
            } catch (const std::exception& e) {
                std::cerr << "Server thread error: " << e.what() << std::endl;
            }
        });

        // Run the GUI loop on the main thread
        gui->run_loop();

        // Clean up
        io_context.stop();
        if (server_thread.joinable()) {
            server_thread.join();
        }

        std::cout << "Application stopped.\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    platform::cleanup();
    return 0;
}