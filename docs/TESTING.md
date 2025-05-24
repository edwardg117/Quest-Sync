# Quest Sync Testing Guide

This document covers testing procedures and guidelines for the Quest Sync project, including both server unit tests and client integration testing.

## Overview

Quest Sync uses a comprehensive testing approach:

- **Server Unit Tests**: Automated tests with 100% code coverage using Google Test framework
- **Client Integration Tests**: Manual testing procedures for the NVSE plugin
- **End-to-End Tests**: Full system testing with client-server communication
- **Performance Tests**: Load testing and performance validation

## Server Unit Tests

### Test Structure

The server tests are located in `Quest Sync Server/Tests/` and organized by component:

- **LoggerTests.cpp**: Logging system functionality
- **ConfigTests.cpp**: Configuration file parsing and validation
- **MessageTests.cpp**: Message serialization and deserialization
- **TCPServerTests.cpp**: Network server implementation
- **CommandProcessorTests.cpp**: Command-line interface

### Prerequisites

- Visual Studio 2019 or later
- Google Test framework (automatically installed via NuGet)
- Google Mock framework (gmock)

### Running Server Tests

#### Using Visual Studio

1. Open `Quest Sync Server/Tests/QuestSyncServerTests.sln`
2. Build the solution (this automatically restores NuGet packages)
3. Open Test Explorer: **Test > Test Explorer**
4. Click **Run All Tests** to execute all tests

#### Command Line

```bash
cd "Quest Sync Server\Tests"
QuestSyncServerTests.exe
```

#### Automated Test Execution

Use the provided batch files for automated testing:

```bash
# Run tests and save XML results
run-tests.bat

# Run tests and save JSON results  
run-tests-json.bat

# Interactive script that can be run from any directory
run-tests-from-any-dir.bat
```

### Test Results

Test results are automatically saved to the `results/` directory with timestamps:

- **XML Format**: `test-results-YYYYMMDD-HHMMSS.xml`
- **JSON Format**: `test-results-YYYYMMDD-HHMMSS.json`

### Test Coverage Goals

The server tests aim for **100% code coverage** and include:

- **Normal Operation**: All standard functionality paths
- **Error Handling**: Exception cases and error conditions
- **Edge Cases**: Boundary conditions and unusual inputs
- **Thread Safety**: Concurrent access scenarios
- **Performance**: Critical path performance validation

## Client Testing

### Manual Testing Procedures

Since the client is an NVSE plugin that integrates with Fallout: New Vegas, testing requires manual procedures:

#### Prerequisites

- Fallout: New Vegas with xNVSE installed
- Quest Sync Server running
- Test save files with various quest states

#### Basic Functionality Tests

1. **Plugin Loading**
   - Start Fallout NV with xNVSE
   - Verify plugin loads without errors
   - Check for initialization messages in logs

2. **Server Connection**
   - Configure `quest_sync.ini` with server details
   - Start game and verify connection notification
   - Check connection status in logs

3. **Quest Synchronization**
   - Start or load a game with active quests
   - Complete quest objectives
   - Verify quest updates are sent to server
   - Test with multiple clients connected

4. **Configuration Testing**
   - Test various configuration options
   - Verify debug logging levels
   - Test reconnection behavior

#### Error Condition Tests

1. **Network Failures**
   - Test behavior when server is unavailable
   - Verify reconnection attempts
   - Test network interruption scenarios

2. **Invalid Configurations**
   - Test with malformed config files
   - Verify graceful handling of invalid settings
   - Test missing configuration files

3. **Version Compatibility**
   - Test with incompatible server versions
   - Verify proper error messages
   - Test version negotiation

### Client Test Scenarios

#### Scenario 1: New Game Synchronization

1. Start new game on Client A
2. Connect Client B to same server
3. Start new game on Client B
4. Complete quest objectives on Client A
5. Verify quest updates appear on Client B
6. Complete different objectives on Client B
7. Verify updates appear on Client A

#### Scenario 2: Save Game Compatibility

1. Load existing save with quest progress on Client A
2. Connect to server
3. Connect Client B with different save
4. Verify quest states synchronize correctly
5. Test quest completion from both clients

#### Scenario 3: Connection Recovery

1. Start both clients connected to server
2. Disconnect server temporarily
3. Verify clients attempt reconnection
4. Restart server
5. Verify clients reconnect automatically
6. Test quest synchronization after reconnection

## End-to-End Testing

### Full System Tests

#### Test Environment Setup

1. **Server Setup**
   - Start Quest Sync Server with test configuration
   - Enable debug logging
   - Configure test-specific settings

2. **Client Setup**
   - Install plugin on test Fallout NV installations
   - Configure clients with server connection details
   - Prepare test save files

#### Multi-Client Testing

1. **Two-Client Scenario**
   - Connect two clients to server
   - Test quest synchronization between clients
   - Verify both directions of communication

2. **Stress Testing**
   - Connect maximum supported clients
   - Generate high-frequency quest updates
   - Monitor server performance and stability

3. **Concurrent Operations**
   - Multiple clients completing quests simultaneously
   - Verify proper synchronization order
   - Test conflict resolution

## Performance Testing

### Server Performance

#### Load Testing

1. **Connection Load**
   - Test maximum concurrent connections
   - Measure connection establishment time
   - Monitor memory usage with multiple clients

2. **Message Throughput**
   - Generate high-frequency quest updates
   - Measure message processing latency
   - Test message queue performance

3. **Resource Usage**
   - Monitor CPU usage under load
   - Track memory consumption over time
   - Test for memory leaks

#### Performance Benchmarks

- **Connection Time**: < 1 second for local connections
- **Message Latency**: < 100ms for quest updates
- **Memory Usage**: Stable over extended operation
- **CPU Usage**: < 10% under normal load

### Client Performance

#### Game Performance Impact

1. **Frame Rate Impact**
   - Measure FPS with and without plugin
   - Test during heavy quest activity
   - Verify minimal performance impact

2. **Memory Usage**
   - Monitor game memory consumption
   - Test for memory leaks during extended play
   - Verify proper cleanup on game exit

3. **Loading Times**
   - Test save game loading with plugin active
   - Measure any impact on game startup time
   - Verify no impact on game stability

## Automated Testing

### Continuous Integration

#### Server Tests

- Automated test execution on code changes
- Test result reporting and notifications
- Code coverage tracking and reporting

#### Integration Tests

- Automated server startup and configuration
- Basic client connection testing
- Smoke tests for critical functionality

### Test Data Management

#### Test Configurations

- Standardized test configuration files
- Version-controlled test data
- Automated test environment setup

#### Test Save Files

- Curated save files for different test scenarios
- Quest states at various completion levels
- Edge case save files for stress testing

## Troubleshooting Tests

### Common Test Issues

#### Server Test Failures

1. **NuGet Package Issues**
   - Run `download-packages.bat` to manually download packages
   - Verify Google Test installation
   - Check project references

2. **Build Configuration Issues**
   - Ensure matching platform (x86/x64)
   - Verify C++17 language standard
   - Check include paths and dependencies

3. **Test Environment Issues**
   - Verify test data files are present
   - Check file permissions
   - Ensure no conflicting processes

#### Client Test Issues

1. **Plugin Loading Failures**
   - Verify xNVSE installation
   - Check plugin file placement
   - Review game compatibility

2. **Connection Issues**
   - Verify server is running
   - Check firewall settings
   - Test network connectivity

3. **Quest Synchronization Problems**
   - Enable debug logging
   - Verify quest mod compatibility
   - Check save file integrity

### Debug Procedures

#### Server Debugging

1. Enable detailed logging in server configuration
2. Use Visual Studio debugger for step-through debugging
3. Monitor network traffic with packet capture tools
4. Review test output files for detailed error information

#### Client Debugging

1. Enable debug logging in `quest_sync.ini`
2. Use NVSE console commands for debugging
3. Monitor game logs for error messages
4. Test with minimal mod configurations

## Test Reporting

### Test Documentation

- Document test procedures and expected results
- Maintain test case database
- Track test coverage metrics
- Report performance benchmarks

### Issue Reporting

When reporting test failures:

1. Include detailed steps to reproduce
2. Provide relevant log files
3. Specify test environment details
4. Include expected vs. actual behavior

### Test Metrics

Track the following metrics:

- **Test Coverage**: Percentage of code covered by tests
- **Pass Rate**: Percentage of tests passing
- **Performance**: Benchmark results over time
- **Stability**: Crash rates and error frequencies

## Contributing to Testing

### Adding New Tests

1. Follow existing test patterns and conventions
2. Write both positive and negative test cases
3. Include performance tests for critical paths
4. Document test procedures and expected results

### Test Review Process

1. All new features must include tests
2. Test code is reviewed alongside feature code
3. Performance impact is evaluated
4. Documentation is updated for new test procedures

For questions about testing procedures or to report test issues, please create an issue on the project repository.
