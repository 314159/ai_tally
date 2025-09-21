@echo off
REM Cross-platform build script for ATEM Tally Server (Windows)
setlocal EnableDelayedExpansion

REM Default values
set BUILD_TYPE=Release
set CLEAN_BUILD=false
set VERBOSE=false
set JOBS=%NUMBER_OF_PROCESSORS%

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :end_parse
if "%~1"=="-h" goto :show_help
if "%~1"=="--help" goto :show_help
if "%~1"=="-c" (
    set CLEAN_BUILD=true
    shift
    goto :parse_args
)
if "%~1"=="--clean" (
    set CLEAN_BUILD=true
    shift
    goto :parse_args
)
if "%~1"=="-d" (
    set BUILD_TYPE=Debug
    shift
    goto :parse_args
)
if "%~1"=="--debug" (
    set BUILD_TYPE=Debug
    shift
    goto :parse_args
)
if "%~1"=="-v" (
    set VERBOSE=true
    shift
    goto :parse_args
)
if "%~1"=="--verbose" (
    set VERBOSE=true
    shift
    goto :parse_args
)
if "%~1"=="-j" (
    set JOBS=%~2
    shift
    shift
    goto :parse_args
)
if "%~1"=="--jobs" (
    set JOBS=%~2
    shift
    shift
    goto :parse_args
)
echo ERROR: Unknown option: %~1
goto :show_help

:end_parse

echo [INFO] Building ATEM Tally Server
echo [INFO] Platform: Windows
echo [INFO] Build Type: %BUILD_TYPE%
echo [INFO] Jobs: %JOBS%

REM Check for required tools
echo [INFO] Checking required tools...

where cmake >nul 2>nul
if errorlevel 1 (
    echo [ERROR] CMake is required but not found in PATH
    exit /b 1
)

where git >nul 2>nul
if errorlevel 1 (
    echo [ERROR] Git is required but not found in PATH
    exit /b 1
)

REM Check CMake version
for /f "tokens=3" %%i in ('cmake --version ^| findstr /R "cmake version"') do set CMAKE_VERSION=%%i
echo [INFO] CMake version %CMAKE_VERSION% found

REM Create and enter build directory
set BUILD_DIR=build

if "%CLEAN_BUILD%"=="true" (
    if exist "%BUILD_DIR%" (
        echo [INFO] Cleaning build directory...
        rmdir /s /q "%BUILD_DIR%"
    )
)

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Configure build
echo [INFO] Configuring build...
set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE%

if "%VERBOSE%"=="true" (
    set CMAKE_ARGS=%CMAKE_ARGS% -DCMAKE_VERBOSE_MAKEFILE=ON
)

REM Check for Visual Studio
where cl >nul 2>nul
if errorlevel 1 (
    echo [INFO] Using MinGW compiler
    set CMAKE_ARGS=%CMAKE_ARGS% -G "MinGW Makefiles"
) else (
    echo [INFO] Using Visual Studio compiler
    set CMAKE_ARGS=%CMAKE_ARGS% -G "Visual Studio 17 2022" -A x64
)

cmake .. %CMAKE_ARGS%
if errorlevel 1 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

REM Build
echo [INFO] Building project...
set BUILD_ARGS=--build . --config %BUILD_TYPE%

if "%VERBOSE%"=="true" (
    set BUILD_ARGS=%BUILD_ARGS% --verbose
)

set BUILD_ARGS=%BUILD_ARGS% --parallel %JOBS%

cmake %BUILD_ARGS%
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)

echo [INFO] Build completed successfully!

REM Show output location
set EXECUTABLE_PATH=%BUILD_TYPE%\ATEMTallyServer.exe
if exist "%EXECUTABLE_PATH%" (
    echo [INFO] Executable created: %CD%\%EXECUTABLE_PATH%
    echo [INFO] To run the server: cd build ^&^& %EXECUTABLE_PATH%
) else (
    echo [WARN] Executable not found at expected location: %EXECUTABLE_PATH%
)

REM Show next steps
echo [INFO] Next steps:
echo   1. cd build
echo   2. %EXECUTABLE_PATH%
echo   3. Open WebSocket client to ws://localhost:8080

goto :eof

:show_help
echo Usage: %0 [OPTIONS]
echo.
echo Build script for ATEM Tally Server
echo.
echo OPTIONS:
echo     -h, --help          Show this help message
echo     -c, --clean         Clean build directory before building
echo     -d, --debug         Build in Debug mode (default: Release^)
echo     -v, --verbose       Enable verbose build output
echo     -j, --jobs N        Number of parallel jobs (default: auto-detected^)
echo.
echo EXAMPLES:
echo     %0                  # Build in Release mode
echo     %0 -d               # Build in Debug mode  
echo     %0 -c -v            # Clean build with verbose output
echo     %0 --debug --jobs 8 # Debug build with 8 parallel jobs
echo.
exit /b 0