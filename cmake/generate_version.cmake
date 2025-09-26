# This script is executed by `cmake -P` from the main CMakeLists.txt
# It receives SOURCE_DIR and BINARY_DIR as command-line definitions.

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR)
    message(FATAL_ERROR "SOURCE_DIR and BINARY_DIR must be defined when running this script.")
endif()

# Find Git
find_package(Git REQUIRED)

# Get version string
execute_process(
    COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Fallback if not in a git repo
if(NOT GIT_VERSION)
    set(GIT_VERSION "0.0.0-unknown")
endif()

# Configure the header file using the fetched version
configure_file(
    "${SOURCE_DIR}/src/version.h.in"
    "${BINARY_DIR}/version.h"
    @ONLY
)
