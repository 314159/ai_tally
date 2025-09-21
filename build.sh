#!/bin/bash

# Cross-platform build script for ATEM Tally Server
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
CLEAN_BUILD=false
VERBOSE=false
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Function to print colored output
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to show usage
show_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Build script for ATEM Tally Server

OPTIONS:
    -h, --help          Show this help message
    -c, --clean         Clean build directory before building
    -d, --debug         Build in Debug mode (default: Release)
    -v, --verbose       Enable verbose build output
    -j, --jobs N        Number of parallel jobs (default: auto-detected)

EXAMPLES:
    $0                  # Build in Release mode
    $0 -d               # Build in Debug mode  
    $0 -c -v            # Clean build with verbose output
    $0 --debug --jobs 8 # Debug build with 8 parallel jobs

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Detect platform
PLATFORM="unknown"
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macOS"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    PLATFORM="Windows"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    PLATFORM="Linux"
    print_warning "Linux support not implemented in this project"
fi

print_status "Building ATEM Tally Server"
print_status "Platform: $PLATFORM"
print_status "Build Type: $BUILD_TYPE"
print_status "Jobs: $JOBS"

# Check for required tools
check_tool() {
    if ! command -v $1 &> /dev/null; then
        print_error "$1 is required but not installed"
        exit 1
    fi
}

print_status "Checking required tools..."
check_tool cmake
check_tool git

# Check CMake version
CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d'.' -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d'.' -f2)

if [[ $CMAKE_MAJOR -lt 3 ]] || [[ $CMAKE_MAJOR -eq 3 && $CMAKE_MINOR -lt 20 ]]; then
    print_error "CMake 3.20 or higher is required (found $CMAKE_VERSION)"
    exit 1
fi

print_status "CMake version $CMAKE_VERSION OK"

# Create and enter build directory
BUILD_DIR="build"

if [[ $CLEAN_BUILD == true && -d "$BUILD_DIR" ]]; then
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure build
print_status "Configuring build..."
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=$BUILD_TYPE"

if [[ $VERBOSE == true ]]; then
    CMAKE_ARGS="$CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=ON"
fi

# Platform-specific configuration
if [[ "$PLATFORM" == "Windows" ]]; then
    # Use Visual Studio generator if available, otherwise MinGW
    if command -v cl &> /dev/null; then
        print_status "Using Visual Studio compiler"
        CMAKE_ARGS="$CMAKE_ARGS -G \"Visual Studio 17 2022\" -A x64"
    else
        print_status "Using MinGW compiler"
        CMAKE_ARGS="$CMAKE_ARGS -G \"MinGW Makefiles\""
    fi
fi

eval cmake .. $CMAKE_ARGS

if [[ $? -ne 0 ]]; then
    print_error "CMake configuration failed"
    exit 1
fi

# Build
print_status "Building project..."
BUILD_ARGS="--build . --config $BUILD_TYPE"

if [[ $VERBOSE == true ]]; then
    BUILD_ARGS="$BUILD_ARGS --verbose"
fi

BUILD_ARGS="$BUILD_ARGS --parallel $JOBS"

eval cmake $BUILD_ARGS

if [[ $? -ne 0 ]]; then
    print_error "Build failed"
    exit 1
fi

print_status "Build completed successfully!"

# Show output location
if [[ "$PLATFORM" == "Windows" ]]; then
    EXECUTABLE_PATH="$BUILD_TYPE/ATEMTallyServer.exe"
else
    EXECUTABLE_PATH="ATEMTallyServer"
fi

if [[ -f "$EXECUTABLE_PATH" ]]; then
    print_status "Executable created: $PWD/$EXECUTABLE_PATH"
    
    # Make executable on Unix systems
    if [[ "$PLATFORM" != "Windows" ]]; then
        chmod +x "$EXECUTABLE_PATH"
    fi
    
    print_status "To run the server: cd build && ./$EXECUTABLE_PATH"
else
    print_warning "Executable not found at expected location: $EXECUTABLE_PATH"
fi

# Show next steps
print_status "Next steps:"
echo "  1. cd build"
echo "  2. ./$EXECUTABLE_PATH"
echo "  3. Open WebSocket client to ws://localhost:8080"