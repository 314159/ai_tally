#pragma once

#include <boost/json.hpp>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

namespace atem {

struct Config {
    // WebSocket settings
    std::string ws_address = "0.0.0.0";
    unsigned short ws_port = 8080;

    // ATEM settings
    std::string atem_ip = "192.168.1.100";

    // Mock mode settings
    bool mock_enabled = false;
    unsigned int mock_update_interval_ms = 2000;
    uint16_t mock_inputs = 8;

    // Load configuration from a JSON file
    void load_from_file(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file) {
            std::cout << "Info: Configuration file '" << filename << "' not found. Using defaults.\n";
            return;
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        boost::system::error_code ec;
        boost::json::value jv = boost::json::parse(content, ec);

        if (ec) {
            std::cerr << "Warning: Failed to parse config file '" << filename << "': " << ec.message() << "\n";
            return;
        }

        try {
            const auto& root = jv.as_object();

            if (root.if_contains("websocket") && jv.at("websocket").is_object()) {
                const auto& ws = jv.at("websocket").as_object();
                if (ws.if_contains("address")) {
                    ws_address = ws.at("address").as_string().c_str();
                }
                if (ws.if_contains("port")) {
                    ws_port = static_cast<unsigned short>(ws.at("port").as_int64());
                }
            }

            if (root.if_contains("atem") && jv.at("atem").is_object()) {
                const auto& a = jv.at("atem").as_object();
                if (a.if_contains("ip_address")) {
                    atem_ip = a.at("ip_address").as_string().c_str();
                }
            }

            if (root.if_contains("mock_mode") && jv.at("mock_mode").is_object()) {
                const auto& mm = jv.at("mock_mode").as_object();
                if (mm.if_contains("enabled")) {
                    mock_enabled = mm.at("enabled").as_bool();
                }
                if (mm.if_contains("update_interval_ms")) {
                    mock_update_interval_ms = static_cast<unsigned int>(mm.at("update_interval_ms").as_int64());
                }
                if (mm.if_contains("num_inputs")) {
                    mock_inputs = static_cast<uint16_t>(mm.at("num_inputs").as_int64());
                }
            }

        } catch (const std::exception& e) {
            std::cerr << "Warning: Error reading values from config file: " << e.what() << "\n";
        }

        // Validate mock inputs
        if (mock_inputs == 0) {
            std::cerr << "Warning: mock_mode.num_inputs is 0; defaulting to 8\n";
            mock_inputs = 8;
        }
    }
};

} // namespace atem
