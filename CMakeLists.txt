cmake_minimum_required(VERSION 3.20)
project(ATEMTallyServer VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add this to handle COM interface definitions properly
if(MSVC)
    add_definitions(-D_WIN32_DCOM)
endif()

set(CMAKE_CXX_EXTENSIONS OFF)

# Include CPM for dependency management
include(cmake/CPM.cmake)

# --- Blackmagic ATEM SDK ---
# Find the SDK directory automatically to avoid hardcoding the version.
file(GLOB BMD_SDK_DIRS LIST_DIRECTORIES true "${CMAKE_SOURCE_DIR}/Blackmagic ATEM Switchers SDK *")

if(NOT BMD_SDK_DIRS)
    message(FATAL_ERROR "Blackmagic ATEM Switchers SDK directory not found in project root. Expected directory pattern: 'Blackmagic ATEM Switchers SDK <version>'")
endif()

# In case multiple versions are present, use the first one found.
list(GET BMD_SDK_DIRS 0 BMD_SDK_DIR)
message(STATUS "Found ATEM SDK at: ${BMD_SDK_DIR}")

# Extract the version from the directory name to pass to the application
if(BMD_SDK_DIR MATCHES "Blackmagic ATEM Switchers SDK ([0-9.]+.*)$")
    set(ATEM_SDK_VERSION "${CMAKE_MATCH_1}")
    message(STATUS "Found ATEM SDK Version: ${ATEM_SDK_VERSION}")
else()
    set(ATEM_SDK_VERSION "unknown")
    message(WARNING "Could not determine ATEM SDK version from directory name: ${BMD_SDK_DIR}")
endif()

# Platform detection
if(WIN32)
    set(PLATFORM_SOURCES src/platform/windows_platform.cpp)
    # Add Ole32.lib for COM
    set(PLATFORM_LIBS ws2_32 wsock32 ole32 oleaut32)
    set(ATEM_SDK_INCLUDE_DIR "${BMD_SDK_DIR}/Windows/include")
    set(ATEM_SDK_DISPATCH_SRC "")
elseif(APPLE)
    set(PLATFORM_SOURCES src/platform/macos_platform.cpp)
    # Add CoreFoundation for CFStringRef etc.
    set(PLATFORM_LIBS "-framework CoreFoundation")
    set(ATEM_SDK_INCLUDE_DIR "${BMD_SDK_DIR}/Mac OS X/include")
    set(ATEM_SDK_DISPATCH_SRC "${BMD_SDK_DIR}/Mac OS X/include/BMDSwitcherAPIDispatch.cpp")
    # Set the macOS deployment target to ensure modern APIs are available.
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Find or fetch Boost
CPMAddPackage(
    NAME Boost
    VERSION 1.89.0
    GITHUB_REPOSITORY boostorg/boost
    GIT_TAG boost-1.89.0
    OPTIONS
        "BOOST_ENABLE_CMAKE ON"
        "BOOST_INCLUDE_LIBRARIES asio\\\;system\\\;thread\\\;chrono\\\;json\\\;program_options"
)

# Find or fetch restbed and apply patches using a CMake script
CPMAddPackage(
    NAME restbed
    GITHUB_REPOSITORY Corvusoft/restbed
    GIT_TAG 4.8
    PATCHES
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/restbed-cmake-version.patch"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/restbed-boost-1.81.patch"
    OPTIONS
        "BUILD_TESTS OFF"
        "BUILD_EXAMPLES OFF"
        "BUILD_SHARED OFF"
        "BUILD_SSL=ON"
)

# Find or fetch Microsoft's GSL (Guideline Support Library)
CPMAddPackage(
    NAME GSL
    GITHUB_REPOSITORY microsoft/GSL
    GIT_TAG v4.2.0 # A recent stable tag
    OPTIONS "GSL_INSTALL ON"
)

# Add ATEM SDK (mock implementation for this example)
set(ATEM_SDK_SOURCES
    src/atem/atem_connection.cpp
    src/atem/tally_state.cpp
    src/atem/atem_sdk_wrapper.cpp
)

# Main executable
add_executable(${PROJECT_NAME}
    src/config.cpp
    src/main.cpp
    src/sse_server.cpp
    src/tally_monitor.cpp
    ${ATEM_SDK_SOURCES}
    ${ATEM_SDK_DISPATCH_SRC}
    ${PLATFORM_SOURCES}
)

# Find Git executable for use in the custom command
find_package(Git REQUIRED)

# --- Git Versioning ---
# This custom command generates version.h by running a CMake script.
add_custom_command(
    OUTPUT "${CMAKE_BINARY_DIR}/version.h"
    # The COMMAND runs cmake in script mode (-P) on our generator script.
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -DBINARY_DIR=${CMAKE_BINARY_DIR}
        -P "${CMAKE_SOURCE_DIR}/cmake/generate_version.cmake"
    # DEPENDS tells CMake when to re-run this command.
    DEPENDS
        "${CMAKE_SOURCE_DIR}/cmake/generate_version.cmake"
        "${CMAKE_SOURCE_DIR}/src/version.h.in"
        "${CMAKE_SOURCE_DIR}/.git/logs/HEAD" # Detects commits, checkouts
        "${CMAKE_SOURCE_DIR}/.git/index"     # Detects staged changes
    COMMENT "Generating version.h from git"
)

# Create a custom target that depends on the generated version.h file.
add_custom_target(version_generator ALL DEPENDS "${CMAKE_BINARY_DIR}/version.h")

# Add the build directory to the include path so the generated version.h can be found
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR})

# Pass the ATEM SDK version to the source code as a preprocessor definition
target_compile_definitions(${PROJECT_NAME} PRIVATE ATEM_SDK_VERSION="${ATEM_SDK_VERSION}")

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/platform
    src/atem
    ${ATEM_SDK_INCLUDE_DIR}
    ${restbed_SOURCE_DIR}/source
)

# Make sure the version_generator target runs before the main executable is built.
add_dependencies(${PROJECT_NAME} version_generator)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::asio
    Boost::system
    Boost::thread
    Boost::chrono
    Boost::json
    Boost::program_options
    GSL
    restbed-static # Use the target name defined by restbed's CMakeLists.txt
    ${PLATFORM_LIBS}
)

# Compiler-specific options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Enable Clang-Tidy for static analysis
find_program(CLANG_TIDY_EXE clang-tidy)
if(CLANG_TIDY_EXE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXE}")
    # The presence of a .clang-tidy file in the source tree root will be
    # automatically detected by clang-tidy.
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXE})
else()
    message(WARNING "clang-tidy not found. Static analysis will be skipped.")
endif()

# Copy resources
file(COPY ${CMAKE_SOURCE_DIR}/config DESTINATION ${CMAKE_BINARY_DIR})

# Install targets
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)
