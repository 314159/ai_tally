# ATEM Tally SSE Server

A cross-platform C++20 Server-Sent Events (SSE) server that monitors ATEM switcher tally states and broadcasts updates to connected clients in real-time.

## Features

- **Cross-Platform**: Supports Windows and macOS with isolated platform-specific code
- **Modern C++20**: Uses RAII, smart pointers, and modern C++ best practices
- **Real-Time Updates**: SSE server broadcasts tally changes immediately over HTTP
- **ATEM Integration**: Connects to Blackmagic ATEM switchers via their SDK
- **Mock Mode**: Built-in simulation for testing without physical hardware
- **CMake Build System**: Uses CPM for dependency management
- **Thread-Safe**: Proper synchronization for multi-threaded operation

## Architecture

### Core Components

- **SseServer**: Manages HTTP connections and broadcasts tally updates via Server-Sent Events
- **TallyMonitor**: Monitors ATEM connection and processes tally state changes
- **ATEMConnection**: Handles communication with ATEM switcher hardware
- **Platform Layer**: Isolates Windows/macOS specific networking code

## Building

### Prerequisites

- Blackmagic ATEM switcher SDK
- CMake 3.20 or higher
- C++20 compatible compiler:
  - Windows: Visual Studio 2019/2022 or MinGW
  - macOS: Xcode 12+ or Clang 10+

### Build Scripts

The project includes convenient build scripts for Windows (`build.bat`) and macOS/Linux (`build.sh`).

```bash
# On macOS or Linux
./build.sh

# On Windows
.\build.bat
```

For more options (like clean or debug builds), run the scripts with the `--help` flag.

### Manual Build Steps

If you prefer to build manually:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Dependencies

Most dependencies are automatically downloaded by CPM:

- **restbed**: For the HTTP/SSE server.
- **Boost**: Specifically `Asio`, `System`, `Program_Options`, `JSON`, `Thread`, and `Chrono`.
- **Microsoft GSL**: Guideline Support Library for utilities like `gsl::finally`.

However, the Blackmagic ATEM Switcher SDK must be downloaded from
[Blackmagic's developer website](https://www.blackmagicdesign.com/developer/products/atem).
Do **NOT** add their SDK into this git archive as the license is
incompatible.

## Usage

### Command-Line Options

The server can be configured via command-line arguments, which will override any settings from the config file.

```bash
./build/ATEMTallyServer --help
```

### Basic Operation

1. Start the server from the project root directory:

    ```bash
    ./build/ATEMTallyServer
    ```

2. Connect an SSE client to the event stream at `http://localhost:8080/events`.

3. The server will attempt to connect to an ATEM switcher (IP configured in `config/server_config.json` or via `--atem-ip`).

4. If no ATEM is found, mock mode automatically enables for testing.

### SSE Protocol

The server pushes events with the name `tally_update` or `mode_change`.

**Tally Update Event:**

```YAML
event: tally_update
data: {"input_id":1,"is_preview":false,"is_program":true}
```

**Mode Change Event (real vs. mock):**

```YAML
event: mode_change
data: {"is_mock":true}
```

### Client Testing

You can test with a simple HTML/JavaScript client:

```html
<script>
  const evtSource = new EventSource("http://localhost:8080/events");

  evtSource.addEventListener("tally_update", (event) => {
    const data = JSON.parse(event.data);
    console.log('Tally Update:', data);
  });

  evtSource.onerror = (err) => {
    console.error("EventSource failed:", err);
  };
</script>
```

- Boost.Asio (Networking)
- Boost.Json (JSON serialization)
- Boost.System (System utilities)

## Configuration

Edit `config/server_config.json` to customize:

- **Web server settings**: Port, bind address, connection limits
- **ATEM connection**: IP address, port, timeouts
- **Mock mode**: Enable simulation, update intervals
- **Logging**: Output levels and destinations

## Platform-Specific Notes

### Windows

- Requires Winsock initialization
- Links against `ws2_32.lib` and `wsock32.lib`
- Supports Windows 10+ with Visual Studio 2019+

### macOS

- Uses standard BSD sockets
- Requires macOS 10.15+ with Xcode 12+
- Links against CoreFoundation framework

## Mock Mode

When no ATEM switcher is available, the server automatically enables mock mode:

- Simulates 8 input channels
- Randomly changes tally states every 2-5 seconds
- Perfect for development and testing
- Can be manually enabled in configuration

## Development

## Troubleshooting

### Common Issues

#### "Failed to bind acceptor"

- Port 8080 might be in use
- Try a different port in configuration
- Check firewall settings

#### "Could not connect to ATEM switcher"

- Verify ATEM IP address is correct
- Ensure ATEM is on same network
- Check that ATEM allows external connections
- Server will automatically use mock mode

#### Build Errors

- Ensure C++20 compiler support
- Check CMake version (3.20+ required)
- Verify internet connection for dependency downloads

### Debug Mode

Build with debug symbols for troubleshooting:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

