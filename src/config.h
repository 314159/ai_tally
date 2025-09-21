#pragma once

#include <string>
#include <fstream>
#include <cstdint>
#include <boost/json.hpp>
#include <iostream>

namespace atem
{

	struct Config
	{
		// WebSocket settings
		std::string ws_address = "0.0.0.0";
		unsigned short ws_port = 8080;

		// ATEM settings
		std::string atem_ip = "192.168.1.100";
		uint16_t mock_inputs = 8;

		// Load configuration from a JSON file
		void load_from_file(const std::string &filename)
		{
			std::ifstream file(filename);
			if (!file)
			{
				std::cout << "Info: Configuration file '" << filename << "' not found. Using defaults.\n";
				return;
			}

			std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			boost::system::error_code ec;
			boost::json::value jv = boost::json::parse(content, ec);

			if (ec)
			{
				std::cerr << "Warning: Failed to parse config file '" << filename << "': " << ec.message() << "\n";
				return;
			}

			try
			{
				ws_address = jv.at("websocket").at("address").as_string().c_str();
				ws_port = static_cast<unsigned short>(jv.at("websocket").at("port").as_int64());
				atem_ip = jv.at("atem").at("ip_address").as_string().c_str();
				mock_inputs = static_cast<uint16_t>(jv.at("atem").at("mock_inputs").as_int64());
			}
			catch (const std::exception &e)
			{
				std::cerr << "Warning: Error reading values from config file: " << e.what() << "\n";
			}
		}
	};

} // namespace atem
