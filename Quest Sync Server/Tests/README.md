# Quest Sync Server Tests

This directory contains unit tests for the Quest Sync Server project. The tests are designed to achieve 100% code coverage and ensure the reliability of the server implementation.

## Test Structure

The tests are organized by component:

- **LoggerTests.cpp**: Tests for the logging system
- **ConfigTests.cpp**: Tests for the configuration management
- **MessageTests.cpp**: Tests for message serialization/deserialization
- **TCPServerTests.cpp**: Tests for the TCP server implementation
- **CommandProcessorTests.cpp**: Tests for the command-line interface
- **SessionManagerTests.cpp**: Tests for session token management
- **RateLimiterTests.cpp**: Tests for authentication rate limiting
- **IntegrationTests.cpp**: Integration tests for component interactions

## Integration Tests

The integration tests validate that server components work correctly together in real-world scenarios:

### Authentication Flow Integration
- **AuthenticationFlowIntegration**: Tests complete authentication workflow with TCPServer + SessionManager + RateLimiter
- **SessionTokenRefreshIntegration**: Tests session token refresh mechanism
- **MultipleClientSessionManagement**: Tests concurrent client session handling

### Message Processing Integration
- **MessageProcessingIntegration**: Tests end-to-end message handling and routing
- **ConfigurationIntegration**: Tests how configuration changes affect server behavior
- **SessionExpiryIntegration**: Tests session expiration and cleanup

### Stress and Concurrency Testing
- **ConcurrentOperationsIntegration**: Tests thread safety with multiple concurrent operations
- **StressTestIntegration**: Tests server performance under high load (100 clients, 1000 messages)
- **ErrorHandlingIntegration**: Tests error recovery and graceful handling of invalid inputs

These tests ensure that:
- Authentication security works correctly across all components
- Session management integrates properly with rate limiting
- Message processing handles concurrent clients safely
- Configuration changes are applied consistently
- The server maintains stability under stress conditions

## Running the Tests

### Prerequisites

- Visual Studio 2019 or later
- Google Test framework (automatically installed via NuGet)

### Steps to Run Tests

1. Open the `QuestSyncServerTests.sln` solution in Visual Studio
2. Build the solution (this will automatically restore the Google Test NuGet package)
3. Run the tests using the Test Explorer in Visual Studio:
   - Open Test Explorer: Test > Test Explorer
   - Click "Run All Tests" to execute all tests

### Command Line

You can also run the tests from the command line:

```
cd "Quest Sync Server\Tests"
QuestSyncServerTests.exe
```

### Saving Test Results

The test runner supports saving test results to XML or JSON files. You can use the provided batch files to run tests and automatically save the results:

```
cd "Quest Sync Server\Tests"
run-tests.bat                # Saves results in XML format
run-tests-json.bat           # Saves results in JSON format
run-tests-from-any-dir.bat   # Interactive script that can be run from any directory
```

The batch files will:
1. Automatically find the test executable in common build directories
2. Create a timestamped filename for the results
3. Save the results to the `results` directory

You can also run the test executable directly with output options:

```
QuestSyncServerTests.exe --output-xml=results/test-results.xml
QuestSyncServerTests.exe --output-json=results/test-results.json
```

Test results are saved in the `results` directory with timestamps in the filename.

## Test Coverage

These tests aim to achieve 100% code coverage for the Quest Sync Server implementation. The tests cover:

- Normal operation paths
- Error handling and edge cases
- Thread safety
- Performance considerations

## Adding New Tests

When adding new functionality to the server, please add corresponding tests to maintain 100% coverage. Follow these guidelines:

1. Create test cases for both normal operation and error conditions
2. Use mock objects where appropriate to isolate the component being tested
3. Ensure thread safety is tested for components that may be accessed concurrently
4. Add performance tests for critical paths

## Troubleshooting

If you encounter issues running the tests:

1. **NuGet Package Issues**:
   - Run the included `download-packages.bat` script to download the Google Test package directly
   - This will create a packages directory and download the Google Test package into it
   - Alternatively, in Visual Studio, right-click on the solution and select "Restore NuGet Packages"
   - If packages still fail to restore, open the Package Manager Console (Tools > NuGet Package Manager > Package Manager Console) and run:
     ```
     Install-Package Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn -Version 1.8.1.7 -ProjectName QuestSyncServerTests
     ```
   - If you see errors about missing gmock/gmock.h, you can also try installing the GMock package:
     ```
     Install-Package gmock -Version 1.11.0 -ProjectName QuestSyncServerTests
     ```

2. **Project Reference Issues**:
   - Ensure the test project correctly references the main Quest Sync Server project
   - If needed, remove and re-add the reference to the main project

3. **Build Configuration**:
   - Make sure you're building for the same platform (x86/x64) as the main project
   - Check that the C++ language standard is set to C++17 in project properties

4. **Google Test Framework**:
   - If Google Test headers are not found, check that the NuGet package is properly installed
   - Verify that the include paths in the project settings are correct
