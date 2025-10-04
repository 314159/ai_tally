#include "config.h"
#include "platform_interface.h"
#include "tally_monitor.h"
#include "websocket_server.h"
#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <future> // Required for promise and future
#include <gsl/gsl>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <thread>

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    // Display application and SDK version info at startup
    std::cout << "ATEM Tally WebSocket Server\n"
              << "Using Blackmagic ATEM SDK Version: " << ATEM_SDK_VERSION << std::endl;

    // Ensure platform cleanup is always called on exit
    auto cleanup_guard = gsl::finally([] { platform::cleanup(); });

    try {
        // Initialize platform-specific code
        if (!platform::initialize()) {
            std::cerr << "Failed to initialize platform\n";
            return 1;
        }

        // --- Configuration ---
        atem::Config config;
        std::string config_file = "config/server_config.json";

        // Load from file first
        config.load_from_file(config_file.c_str());

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
            "mock", po::bool_switch(&config.mock_enabled)->default_value(config.mock_enabled), "Enable mock mode")(
            "mock-inputs", po::value<uint16_t>(&config.mock_inputs)->default_value(config.mock_inputs),
            "Number of inputs to show in mock mode");

        po::variables_map vm;
        const std::vector<std::string> args(argv, argv + argc);
        po::store(po::command_line_parser(args).options(desc).run(), vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << "\n"
                      << desc << "\n";
            return 0;
        }

        // Re-load from file if a different one was specified on the command line
        if (vm.count("config") && vm["config"].as<std::string>() != "config/server_config.json") {
            config.load_from_file(config_file.c_str());
            // Re-apply command line arguments to override the new file's settings
            po::store(po::command_line_parser(args).options(desc).run(), vm);
            po::notify(vm);
        }

        // --- Service Setup ---
        boost::asio::io_context io_context;
        const auto address = boost::asio::ip::make_address(config.ws_address);
        const unsigned short port = config.ws_port;

        // Create tally monitor
        auto monitor = std::make_unique<atem::TallyMonitor>(io_context, config);

        // Create server
        auto server = std::make_unique<atem::HttpAndWebSocketServer>(io_context, address, port, config, gsl::make_not_null(monitor.get()));

        // Connect tally updates to websocket broadcasts and TUI
        monitor->on_tally_change([&server](const atem::TallyUpdate& update) {
            server->broadcast_tally_update(update);
        });

        // Connect mode changes to websocket broadcasts
        monitor->on_mode_change([&server](bool is_mock) {
            server->broadcast_mode_change(is_mock);
        });

        // Keep the io_context running until it's explicitly stopped.
        auto work_guard = boost::asio::make_work_guard(io_context);

        // Run the io_context in its own thread for server operations
        std::thread server_thread([&io_context]() {
            io_context.run();
            std::cout << "Server thread finished." << std::endl;
        });

        std::promise<void> server_ready_promise;
        std::future<void> server_ready_future = server_ready_promise.get_future();
        monitor->on_ready([&server_ready_promise]() {
            server_ready_promise.set_value();
        });
        // Start the monitor first and wait for it to be ready.

        // Setup signal handling for graceful shutdown
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](const boost::system::error_code&, int) {
            // Stop the io_context. This will cause io_context.run() to return.
            io_context.stop();
        });

        monitor->start();
        server_ready_future.wait();

        // Start the server
        server->start();

        // The main thread will now block here until a signal is received.
        server_thread.join();
        // --- Shutdown ---
        std::cout << "Shutting down server..." << std::endl;
        if (server)
            server->stop();
        if (monitor)
            monitor->stop();

        // Allow the io_context to stop by resetting the work guard.
        work_guard.reset();
        if (server_thread.joinable()) {
            server_thread.join();
        }

        std::cout << "Application stopped." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
