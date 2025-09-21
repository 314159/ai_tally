#ifdef _WIN32

#include "platform_interface.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iostream>
#include <sstream>

#pragma comment(lib, "ws2_32.lib")

namespace platform
{

	namespace
	{
		bool winsock_initialized = false;
		std::string last_error_message;

		void set_last_error(const std::string &message)
		{
			DWORD error_code = GetLastError();
			std::ostringstream oss;
			oss << message;
			if (error_code != 0)
			{
				oss << " (Error code: " << error_code << ")";
			}
			last_error_message = oss.str();
		}

		void set_winsock_error(const std::string &message)
		{
			int error_code = WSAGetLastError();
			std::ostringstream oss;
			oss << message << " (WSA Error: " << error_code << ")";
			last_error_message = oss.str();
		}
	}

	bool initialize()
	{
		std::cout << "Initializing Windows platform...\n";

		if (!initialize_network())
		{
			return false;
		}

		// Set console UTF-8 code page for better text output
		if (!SetConsoleOutputCP(CP_UTF8))
		{
			set_last_error("Failed to set console output code page to UTF-8");
			// This is not critical, so we continue
		}

		// Enable ANSI escape sequences for colored output
		HANDLE console_handle = GetStdHandle(STD_OUTPUT_HANDLE);
		if (console_handle != INVALID_HANDLE_VALUE)
		{
			DWORD console_mode;
			if (GetConsoleMode(console_handle, &console_mode))
			{
				console_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				SetConsoleMode(console_handle, console_mode);
			}
		}

		return true;
	}

	void cleanup()
	{
		std::cout << "Cleaning up Windows platform...\n";
		cleanup_network();
	}

	std::string get_platform_name()
	{
		OSVERSIONINFOEX version_info{};
		version_info.dwOSVersionInfoSize = sizeof(version_info);

// Note: GetVersionEx is deprecated, but we'll use it for simplicity
// In production, you'd want to use Version Helper functions or RtlGetVersion
#pragma warning(push)
#pragma warning(disable : 4996)
		if (GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&version_info)))
		{
			std::ostringstream oss;
			oss << "Windows " << version_info.dwMajorVersion << "." << version_info.dwMinorVersion;
			if (version_info.wServicePackMajor > 0)
			{
				oss << " SP" << version_info.wServicePackMajor;
			}
			return oss.str();
		}
#pragma warning(pop)

		return "Windows (unknown version)";
	}

	bool initialize_network()
	{
		if (winsock_initialized)
		{
			return true;
		}

		WSADATA wsa_data;
		int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);

		if (result != 0)
		{
			set_winsock_error("WSAStartup failed");
			return false;
		}

		// Verify that we got the requested Winsock version
		if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2)
		{
			cleanup_network();
			last_error_message = "Requested Winsock version not available";
			return false;
		}

		winsock_initialized = true;
		std::cout << "Winsock 2.2 initialized successfully\n";
		return true;
	}

	void cleanup_network()
	{
		if (winsock_initialized)
		{
			WSACleanup();
			winsock_initialized = false;
			std::cout << "Winsock cleaned up\n";
		}
	}

	std::string get_last_error()
	{
		return last_error_message;
	}

} // namespace platform

#endif // _WIN32