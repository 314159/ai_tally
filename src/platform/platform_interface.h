#pragma once

#include <string>

namespace platform {

/**
 * Initialize platform-specific resources
 * @return true if initialization succeeded, false otherwise
 */
bool initialize();

/**
 * Clean up platform-specific resources
 */
void cleanup();

/**
 * Get the name of the current platform
 * @return platform name string
 */
std::string get_platform_name();

/**
 * Get platform-specific network initialization requirements
 * @return true if network subsystem was initialized successfully
 */
bool initialize_network();

/**
 * Clean up platform-specific network resources
 */
void cleanup_network();

/**
 * Get the last platform-specific error message
 * @return error message string
 */
std::string get_last_error();

} // namespace platform
