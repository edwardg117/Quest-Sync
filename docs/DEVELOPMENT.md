# Quest Sync Development Guide

This guide covers development setup and contribution guidelines for the Quest Sync project.

## Welcome

Quest Sync is an xNVSE plugin that synchronizes quest progress between multiple players in Fallout: New Vegas. The project consists of two main components:

- **Quest Sync NVSE Plugin** - Client-side plugin that integrates with Fallout: New Vegas
- **Quest Sync Server** - Standalone server application that coordinates quest synchronization

If you have questions about development, you can join the xNVSE Discord server in the #plugin-development channel: https://discord.gg/NXqSEcHjg2

## Prerequisites

### Technical Requirements

- **C++ Knowledge**: A solid grasp of C++ fundamentals is required for most development work
- **Visual Studio 2022**: Primary development environment (VS 2019+ may work with additional setup)
- **xNVSE Framework**: Understanding of NVSE plugin development patterns
- **Fallout: New Vegas**: For testing and development

### Development Skills

- C++ programming and debugging
- Network programming (TCP sockets)
- Multi-threaded programming
- Game modding concepts
- Version control (Git)

## Environment Setup

### 1. Visual Studio Installation

Install Visual Studio 2022 with the following components:
- **Desktop development with C++** workload
- **MSVC v143 compiler toolset**
- **Windows 10/11 SDK**

### 2. Environment Variables

Set up the following environment variable for automatic deployment:

1. Open "Edit the system environment variables" from Windows Search
2. Click "Environment Variables..."
3. Add a new system variable:
   - **Name**: `FalloutNVPath`
   - **Value**: Path to your Fallout New Vegas installation (e.g., `C:\Steam\steamapps\common\Fallout New Vegas`)

### 3. Project Structure

```
Quest-Sync/
├── docs/                          # Documentation
├── nvse_plugin_example/           # Client plugin source
├── Quest Sync Server/             # Server application source
├── Quest Sync Server/Tests/       # Server unit tests
├── nvse/                          # xNVSE framework
└── common/                        # Shared utilities
```

## Building the Project

### Client Plugin (NVSE Plugin)

1. Open `nvse_plugin_example/nvse_plugin_example.sln`
2. Select build configuration:
   - **Debug**: For development and debugging
   - **Release**: For production builds
3. Build the solution (F7)
4. The compiled DLL will be automatically copied to your Fallout NV directory if `FalloutNVPath` is set

### Server Application

1. Open `Quest Sync Server/Quest Sync Server.sln`
2. Select build configuration (Debug/Release)
3. Build the solution
4. The executable will be in the respective Debug/Release folder

### Running Tests

See [Testing Documentation](TESTING.md) for comprehensive testing instructions.

## Development Workflow

### 1. Code Organization

- **Client code**: Located in `nvse_plugin_example/`
- **Server code**: Located in `Quest Sync Server/`
- **Shared components**: Use consistent patterns between client and server
- **Version management**: Centralized in `Version.h` files

### 2. Coding Standards

- Follow existing code style and naming conventions
- Use meaningful variable and function names
- Add comments for complex logic
- Implement proper error handling
- Write unit tests for new functionality

### 3. Testing

- Write unit tests for all new server functionality
- Test client functionality in-game
- Verify network communication between client and server
- Test version compatibility scenarios

### 4. Documentation

- Update relevant documentation for new features
- Add inline code comments for complex algorithms
- Update configuration documentation for new settings

## Component-Specific Development

### NVSE Plugin Development

The client plugin integrates with Fallout: New Vegas through the xNVSE framework:

- **Game Integration**: Uses NVSE hooks and callbacks
- **Quest Tracking**: Monitors quest state changes
- **Network Client**: Communicates with Quest Sync Server
- **Configuration**: Reads settings from `quest_sync.ini`

Key files:
- `main.cpp` - Plugin initialization and NVSE integration
- `NetworkClient.cpp` - TCP client implementation
- `QuestStateTracker.cpp` - Quest monitoring logic
- `Config.cpp` - Configuration management

### Server Development

The server is a standalone C++ application:

- **TCP Server**: Handles multiple client connections
- **Message Processing**: Serializes/deserializes quest data
- **Command Interface**: Provides server management commands
- **Configuration**: Reads settings from `Quest Sync Server.cfg`

Key files:
- `Main.cpp` - Server startup and main loop
- `TCPServer.cpp` - Network server implementation
- `CommandProcessor.cpp` - Command-line interface
- `Message.cpp` - Message serialization

## Debugging

### Client Plugin Debugging

1. Build in Debug configuration
2. Attach Visual Studio debugger to `FalloutNV.exe`
3. Set breakpoints in plugin code
4. Launch game with xNVSE

### Server Debugging

1. Build in Debug configuration
2. Run server executable in Visual Studio debugger
3. Set breakpoints as needed
4. Connect client to test functionality

### Network Debugging

- Enable debug logging in both client and server
- Use network monitoring tools (Wireshark) if needed
- Check firewall and port configuration
- Verify version compatibility

## Contributing

### Before Contributing

1. Read the [Architecture Documentation](ARCHITECTURE.md)
2. Understand the [Version Compatibility System](VERSION_COMPATIBILITY.md)
3. Review existing code patterns and conventions
4. Set up your development environment

### Contribution Process

1. Fork the repository
2. Create a feature branch
3. Implement your changes
4. Write/update tests
5. Update documentation
6. Submit a pull request

### Pull Request Guidelines

- Provide clear description of changes
- Include test coverage for new functionality
- Update relevant documentation
- Follow existing code style
- Ensure all tests pass

## Common Development Tasks

### Adding New Configuration Options

1. Add setting to appropriate config file (`quest_sync.ini` or `Quest Sync Server.cfg`)
2. Update `Config.cpp` to read the new setting
3. Add validation and default values
4. Update [Configuration Documentation](CONFIGURATION.md)

### Adding New Network Messages

1. Define message structure in `Message.h`
2. Implement serialization/deserialization
3. Add message handling in client and server
4. Write unit tests for message processing
5. Update [Networking Documentation](NETWORKING.md)

### Version Updates

1. Update version constants in `Version.h` files
2. Adjust compatibility ranges if needed
3. Update [Version Compatibility Documentation](VERSION_COMPATIBILITY.md)
4. Test compatibility scenarios

## Troubleshooting Development Issues

### Build Errors

- Verify Visual Studio configuration
- Check that all dependencies are installed
- Ensure environment variables are set correctly
- Clean and rebuild solution

### Runtime Issues

- Check log files for error messages
- Verify configuration settings
- Test with minimal setup
- Use debugger to trace execution

### Network Issues

- Verify firewall settings
- Check port availability
- Test with local connections first
- Enable debug logging

## Resources

- [xNVSE Documentation](https://github.com/xNVSE/NVSE)
- [Fallout NV Modding Wiki](https://geckwiki.com/)
- [Visual Studio Documentation](https://docs.microsoft.com/en-us/visualstudio/)
- [C++ Reference](https://en.cppreference.com/)

For additional help, join the xNVSE Discord community or create an issue on the project repository.
