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

# Find or fetch Microsoft's GSL (Guideline Support Library)
CPMAddPackage(
    NAME GSL
    GITHUB_REPOSITORY microsoft/GSL
    GIT_TAG v4.2.0 # A recent stable tag
    OPTIONS "GSL_INSTALL ON"
)

# --- wxWidgets Configuration ---
# This section sets options for the wxWidgets subproject build. We set these as
# CACHE variables BEFORE calling CPMAddPackage to ensure they are correctly
# picked up by the wxWidgets build system.

# Core build settings: Build as individual component libraries.
set(wxBUILD_SHARED OFF CACHE BOOL "Build wxWidgets as a static library")
set(wxBUILD_SAMPLES OFF CACHE BOOL "Do not build wxWidgets samples")
set(wxBUILD_MONOLITHIC OFF CACHE BOOL "Build wxWidgets as individual libraries")
set(wxUSE_GUI ON CACHE BOOL "Enable wxWidgets GUI components")

# Disable precompiled headers to ensure wxBUILD_LIBS is respected.
set(wxBUILD_PRECOMP OFF CACHE BOOL "Do not use precompiled headers")

# Suppress the -Wdeprecated-copy warning from wxWidgets headers, which causes
# build failures when -Werror is enabled. This flag is appended only to the
# wxWidgets build, not the main project.
set(wxCXX_FLAGS "-Wno-deprecated-copy" CACHE STRING "Extra flags for wxWidgets build")

# Platform-specific settings
if(APPLE)
    # On macOS, we must disable memset_s to prevent a link error with v3.2.8
    # and explicitly enable iconv for character set conversion.
    set(HAVE_MEMSET_S OFF CACHE BOOL "Disable buggy memset_s on macOS")
    set(wxUSE_ICONV ON CACHE BOOL "Enable iconv for character set conversion")
    CPMAddPackage(NAME wxWidgets
        GITHUB_REPOSITORY wxWidgets/wxWidgets
        GIT_TAG v3.2.8
        PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/wxwidgets-v3.2.8-macos-memset_s.patch"
    )
else()
    CPMAddPackage(NAME wxWidgets
        GITHUB_REPOSITORY wxWidgets/wxWidgets
        GIT_TAG v3.2.8
    )
endif()

# Add ATEM SDK (mock implementation for this example)
set(ATEM_SDK_SOURCES
    ${CMAKE_SOURCE_DIR}/src/atem/atem_connection.cpp
    ${CMAKE_SOURCE_DIR}/src/atem/tally_state.cpp
    ${CMAKE_SOURCE_DIR}/src/atem/atem_sdk_wrapper.cpp
)

set(GUI_SOURCES
    ${CMAKE_SOURCE_DIR}/src/gui/App.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/MainFrame.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/PreferencesDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/TallyPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/BackendService.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/mock_tally_provider.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/atem_tally_provider.cpp
)

# Consolidate all source files into a single list
set(APP_SOURCES
    ${CMAKE_SOURCE_DIR}/src/config.cpp
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/websocket_server.cpp
    ${CMAKE_SOURCE_DIR}/src/tally_monitor.cpp
    ${ATEM_SDK_SOURCES}
    ${GUI_SOURCES}
    ${ATEM_SDK_DISPATCH_SRC}
    ${PLATFORM_SOURCES}
)

# Main executable
add_executable(${PROJECT_NAME} ${APP_SOURCES})

# By default, CMake doesn't treat include directories from imported targets
# as SYSTEM includes. Setting this property to OFF changes that behavior,
# which will suppress compiler warnings from third-party libraries like wxWidgets.
set_property(TARGET ${PROJECT_NAME} PROPERTY NO_SYSTEM_FROM_IMPORTED OFF)

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

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    src/platform
    src/atem
    src/gui
    ${ATEM_SDK_INCLUDE_DIR}
    ${CMAKE_BINARY_DIR} # For the generated version.h
)

# Make sure the version_generator target runs before the main executable is built.
add_dependencies(${PROJECT_NAME} version_generator)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Boost::beast
    Boost::asio
    Boost::system
    Boost::thread
    Boost::chrono
    Boost::json
    Boost::program_options
    GSL # GSL might need to be linked before wxWidgets on some platforms
    # Link against the individual wxWidgets component targets. This automatically
    # handles include directories, compile definitions, and library files.
    # wx::core and wx::base are automatically included as dependencies of wx::adv
    wx::adv  # For advanced controls like wxSpinCtrl
)

# On Windows, create a windowed application instead of a console one
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON)
    # Define for static linking of wxWidgets on Windows
    # This is crucial for the wx headers to correctly include setup.h
    # and see definitions like wxUSE_VALIDATORS.
    target_compile_definitions(${PROJECT_NAME} PRIVATE wxMSW_STATIC_LIB)
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# For non-MSVC compilers, specifically disable the -Wdeprecated-copy warning
# for the GUI source files. This prevents build failures caused by wxWidgets
# headers when -Werror is active, without affecting the rest of the project.
if(NOT MSVC)
    set_source_files_properties(${GUI_SOURCES} PROPERTIES
        COMPILE_FLAGS "-Wno-deprecated-copy")
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
