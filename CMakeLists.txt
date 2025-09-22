cmake_minimum_required(VERSION 3.20)
project(ATEMTallyServer VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BMD_SDK_VERSION "10.0")

# Add this to handle COM interface definitions properly
if(MSVC)
    add_definitions(-D_WIN32_DCOM)
endif()


set(CMAKE_CXX_EXTENSIONS OFF)

# Include CPM for dependency management
include(cmake/CPM.cmake)

# Platform detection
if(WIN32)
    set(PLATFORM_SOURCES src/platform/windows_platform.cpp)
    # Add Ole32.lib for COM
    set(PLATFORM_LIBS ws2_32 wsock32 ole32)
    set(ATEM_SDK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Blackmagic ATEM Switchers SDK ${BMD_SDK_VERSION}/Windows/include")
    set(ATEM_SDK_DISPATCH_SRC "")
elseif(APPLE)
    set(PLATFORM_SOURCES src/platform/macos_platform.cpp)
    # Add CoreFoundation for CFStringRef etc.
    set(PLATFORM_LIBS "-framework CoreFoundation")
    set(ATEM_SDK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/Blackmagic ATEM Switchers SDK ${BMD_SDK_VERSION}/Mac OS X/include")
    set(ATEM_SDK_DISPATCH_SRC "${CMAKE_SOURCE_DIR}/Blackmagic ATEM Switchers SDK ${BMD_SDK_VERSION}/Mac OS X/include/BMDSwitcherAPIDispatch.cpp")
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
    GIT_TAG boost-1.89.0 # Or the specific tag for 1.89.0
    OPTIONS
        "BOOST_ENABLE_CMAKE ON"
        "BOOST_INCLUDE_LIBRARIES beast\\\;asio\\\;system\\\;thread\\\;chrono\\\;json\\\;program_options"
)

# Add ATEM SDK (mock implementation for this example)
set(ATEM_SDK_SOURCES
    src/atem/atem_connection.cpp
    src/atem/tally_state.cpp
    src/atem/atem_sdk_wrapper.cpp
)

# Main executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/websocket_server.cpp
    src/tally_monitor.cpp
    ${ATEM_SDK_SOURCES}
    ${ATEM_SDK_DISPATCH_SRC}
    ${PLATFORM_SOURCES}
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/platform
    src/atem
    ${ATEM_SDK_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::beast
    Boost::asio
    Boost::system
    Boost::thread
    Boost::chrono
    Boost::json
    Boost::program_options
    ${PLATFORM_LIBS}
)

# Compiler-specific options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# Copy resources
file(COPY ${CMAKE_SOURCE_DIR}/config DESTINATION ${CMAKE_BINARY_DIR})

# Install targets
install(TARGETS ${PROJECT_NAME} 
    RUNTIME DESTINATION bin
)