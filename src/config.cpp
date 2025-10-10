#include "config.h"
#include <boost/json.hpp>
#include <fstream>
#include <iostream>
#include <optional>

namespace atem {

void Config::load_from_file(gsl::czstring filename)
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

    // Helper to safely get a value from a JSON object
    auto get_value = [](const boost::json::object& obj, const char* key) -> std::optional<boost::json::value> {
        if (obj.contains(key)) {
            return obj.at(key);
        }
        return std::nullopt;
    };

    try {
        const auto& root = jv.as_object();

        if (auto ws_val = get_value(root, "websocket"); ws_val && ws_val->is_object()) {
            const auto& ws = ws_val->as_object();
            if (auto addr = get_value(ws, "address"))
                ws_address = boost::json::value_to<std::string>(*addr);
            if (auto port = get_value(ws, "port"))
                ws_port = static_cast<unsigned short>(port->as_int64());
        }

        if (auto atem_val = get_value(root, "atem"); atem_val && atem_val->is_object()) {
            const auto& a = atem_val->as_object();
            if (auto ip = get_value(a, "ip_address"))
                atem_ip = boost::json::value_to<std::string>(*ip);
        }

        if (auto mock_val = get_value(root, "mock_mode"); mock_val && mock_val->is_object()) {
            const auto& mm = mock_val->as_object();
            if (auto enabled = get_value(mm, "enabled"))
                mock_enabled = enabled->as_bool();
            if (auto interval = get_value(mm, "update_interval_ms"))
                mock_update_interval_ms = static_cast<unsigned int>(interval->as_int64());
            if (auto num = get_value(mm, "num_inputs"))
                mock_inputs = static_cast<uint16_t>(num->as_int64());
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

} // namespace atem
