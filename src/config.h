#pragma once

#include <cstdint>
#include <gsl/gsl>
#include <string>

namespace atem {

struct Config {
    // WebSocket settings
    std::string ws_address = "0.0.0.0";
    unsigned short ws_port = 8080;
    int ws_connection_limit = 100;

    // ATEM settings
    std::string atem_ip = "192.168.1.100";

    // Mock mode settings
    bool mock_enabled = false;
    bool use_mock_automatically = true; // Fallback to mock if real connection fails
    unsigned int mock_update_interval_ms = 2000;
    uint16_t mock_inputs = 8;

    // Load configuration from a JSON file
    void load_from_file(gsl::czstring filename);
};

} // namespace atem
