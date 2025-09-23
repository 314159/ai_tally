#ifdef __APPLE__

#include "platform_interface.h"
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/utsname.h>

namespace platform {

namespace {
    std::string last_error_message;

    [[maybe_unused]] void set_last_error(const std::string& message)
    {
        last_error_message = message + " (errno: " + std::to_string(errno) + ")";
    }
}

bool initialize()
{
    std::cout << "Initializing macOS platform...\n";

    if (!initialize_network()) {
        return false;
    }

    // Set locale for proper UTF-8 handling
    if (setlocale(LC_ALL, "en_US.UTF-8") == nullptr) {
        std::cout << "Warning: Could not set UTF-8 locale\n";
        // This is not critical, continue anyway
    }

    return true;
}

void cleanup()
{
    std::cout << "Cleaning up macOS platform...\n";
    cleanup_network();
}

std::string get_platform_name()
{
    struct utsname system_info {};

    if (uname(&system_info) != 0) {
        return "macOS (unknown version)";
    }

    std::string platform_name = "macOS ";
    platform_name += system_info.release;
    platform_name += " (";
    platform_name += system_info.machine;
    platform_name += ")";

    return platform_name;
}

bool initialize_network()
{
    // On macOS/Unix systems, the network stack is always available
    // No special initialization is required like Windows Winsock
    std::cout << "Network stack ready (Unix sockets)\n";
    return true;
}

void cleanup_network()
{
    // No special cleanup required for Unix network stack
    std::cout << "Network cleanup complete\n";
}

std::string get_last_error()
{
    return last_error_message;
}

} // namespace platform

#endif // __APPLE__
