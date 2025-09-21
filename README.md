# ATEM Tally WebSocket Server

A cross-platform C++20 WebSocket server that monitors ATEM switcher tally states and broadcasts updates to connected clients in real-time.

## Features

- **Cross-Platform**: Supports Windows and macOS with isolated platform-specific code
- **Modern C++20**: Uses RAII, Rule of 7, smart pointers, and modern C++ best practices
- **Real-Time Updates**: WebSocket server broadcasts tally changes immediately
- **ATEM Integration**: Connects to Blackmagic ATEM switchers via UDP protocol
- **Mock Mode**: Built-in simulation for testing without physical hardware
- **CMake Build System**: Uses CPM for dependency management
- **Thread-Safe**: Proper synchronization for multi-threaded operation

## Architecture

### Core Components

- **WebSocketServer**: Manages WebSocket connections and broadcasts tally updates
- **TallyMonitor**: Monitors ATEM connection and processes tally state changes  
- **ATEMConnection**: Handles communication with ATEM switcher hardware
- **Platform Layer**: Isolates Windows/macOS specific networking code

### Design Patterns

- **RAII**: All resources are properly managed with automatic cleanup
- **Rule of 7**: Classes properly implement copy/move semantics or delete them
- **Observer Pattern**: Callback-based notifications for tally state changes
- **Dependency Injection**: Components receive dependencies through constructor

## Building

### Prerequisites

- CMake 3.20 or higher
- C++20 compatible compiler:
  - Windows: Visual Studio 2019/2022 or MinGW
  - macOS: Xcode 12+ or Clang 10+

### Build Steps

```bash
# Clone repository
git clone <repository-url>
cd atem-tally-server

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build .

# Run
./ATEMTallyServer
```

### Dependencies

Dependencies are automatically downloaded by CPM:

- Boost.Beast (WebSocket and HTTP)
- Boost.Asio (Networking)
- Boost.Json (JSON serialization)
- Boost.System (System utilities)

## Usage

### Basic Operation

1. Start the server:

   ```bash
   ./ATEMTallyServer
   ```

2. Connect WebSocket clients to `ws://localhost:8080`

3. The server will attempt to connect to an ATEM switcher at `192.168.1.100`

4. If no ATEM is found, mock mode automatically enables for testing

### WebSocket Protocol

#### Messages Sent to Clients

**Welcome Message:**

```json
{
  "type": "welcome",
  "message": "Connected to ATEM Tally Server"
}
```

**Tally Updates:**

```json
{
  "type": "tally_update",
  "input": 1,
  "program": true,
  "preview": false,
  "timestamp": 1640995200000
}
```

#### Client Testing

You can test with a simple WebSocket client:

```javascript
const ws = new WebSocket('ws://localhost:8080');

ws.onmessage = (event) => {
  const data = JSON.parse(event.data);
  console.log('Tally Update:', data);
  
  if (data.type === 'tally_update') {
    console.log(`Input ${data.input}: Program=${data.program}, Preview=${data.preview}`);
  }
};
```

## Configuration

Edit `config/server_config.json` to customize:

- **WebSocket settings**: Port, bind address, connection limits
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

## ATEM Protocol

The server implements a subset of the Blackmagic ATEM protocol:

- **Connection**: UDP socket on port 9910
- **Packet Format**: Binary protocol with command parsing
- **Tally Commands**: Processes `TlIn` (Tally Input) commands
- **Real Implementation**: Currently uses mock data (see `atem_connection.cpp`)

## Development

### Adding New Platforms

1. Create `src/platform/newplatform_platform.cpp`
2. Implement the `platform_interface.h` functions
3. Add platform detection to `CMakeLists.txt`
4. Add platform-specific libraries if needed

### Extending Functionality

- **New Message Types**: Add to WebSocket protocol in `websocket_server.cpp`
- **Additional ATEM Data**: Extend `TallyState` structure
- **Configuration**: Add options to `server_config.json`
- **Logging**: Implement file-based logging system

## Troubleshooting

### Common Issues

**"Failed to bind acceptor"**

- Port 8080 might be in use
- Try a different port in configuration
- Check firewall settings

**"Could not connect to ATEM switcher"**

- Verify ATEM IP address is correct
- Ensure ATEM is on same network
- Check that ATEM allows external connections
- Server will automatically use mock mode

**Build Errors**

- Ensure C++20 compiler support
- Check CMake version (3.20+ required)  
- Verify internet connection for dependency downloads

### Debug Mode

Build with debug symbols for troubleshooting:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build .
```

## License

This project demonstrates modern C++ techniques and cross-platform development patterns. The ATEM protocol implementation is simplified for educational purposes.

For production use with real ATEM switchers, you would need to implement the complete Blackmagic ATEM SDK protocol or use their official SDK.
