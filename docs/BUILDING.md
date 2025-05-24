# Quest Sync Build Guide

This document provides detailed instructions for building Quest Sync from source code, including environment setup, dependencies, and build configurations.

## Prerequisites

### Required Software

#### Visual Studio 2022
- **Edition**: Community, Professional, or Enterprise
- **Workloads**: Desktop development with C++
- **Components**:
  - MSVC v143 compiler toolset
  - Windows 10/11 SDK (latest version)
  - CMake tools for C++ (optional)

#### Git
- **Version**: 2.30 or later
- **Purpose**: Source code management and dependency retrieval

### System Requirements

#### Development Machine
- **OS**: Windows 10 (1903+) or Windows 11
- **RAM**: 8GB minimum, 16GB recommended
- **Storage**: 10GB free space for source code and build outputs
- **CPU**: x64 processor with SSE2 support

#### Target Environment
- **Game**: Fallout: New Vegas (Steam, GOG, or retail)
- **Framework**: xNVSE 6.0 or later
- **OS**: Windows 7 SP1 or later

## Source Code Setup

### Repository Structure

```
Quest-Sync/
├── docs/                          # Documentation
├── nvse_plugin_example/           # Client plugin source
│   ├── *.cpp, *.h                # Plugin source files
│   ├── nvse_plugin_example.sln   # Visual Studio solution
│   └── nvse_plugin_example.vcxproj # Project file
├── Quest Sync Server/             # Server application source
│   ├── *.cpp, *.h                # Server source files
│   ├── Quest Sync Server.sln     # Visual Studio solution
│   └── Quest Sync Server.vcxproj # Project file
├── Quest Sync Server/Tests/       # Server unit tests
│   ├── *.cpp, *.h                # Test source files
│   └── QuestSyncServerTests.vcxproj # Test project
├── nvse/                          # xNVSE framework
├── common/                        # Shared utilities
└── README.md
```

### Environment Variables

Set up the following environment variables for automatic deployment:

#### FalloutNVPath
**Purpose**: Automatic plugin deployment to game directory
**Setup**:
1. Open "Edit the system environment variables" from Windows Search
2. Click "Environment Variables..."
3. Add new system variable:
   - **Name**: `FalloutNVPath`
   - **Value**: Path to Fallout New Vegas installation

**Examples**:
- Steam: `C:\Program Files (x86)\Steam\steamapps\common\Fallout New Vegas`
- GOG: `C:\GOG Games\Fallout New Vegas`
- Custom: Your installation path

## Building the Client Plugin

### Project Configuration

#### Opening the Project
1. Navigate to `nvse_plugin_example/`
2. Open `nvse_plugin_example.sln` in Visual Studio
3. Wait for project to load and dependencies to resolve

#### Build Configurations

**Debug Configuration:**
- **Purpose**: Development and debugging
- **Optimizations**: Disabled
- **Debug Info**: Full debug information included
- **Runtime**: Debug runtime libraries
- **Output**: `Debug/QuestSync NVSE Plugin.dll`

**Release Configuration:**
- **Purpose**: Production deployment
- **Optimizations**: Maximum optimization for speed
- **Debug Info**: Minimal debug information
- **Runtime**: Release runtime libraries
- **Output**: `Release/QuestSync NVSE Plugin.dll`

### Build Process

#### Using Visual Studio IDE

1. **Select Configuration**:
   - Debug: For development and testing
   - Release: For distribution

2. **Select Platform**:
   - x86: For 32-bit Fallout New Vegas (standard)
   - x64: Not supported (game is 32-bit)

3. **Build Solution**:
   - Menu: Build → Build Solution
   - Shortcut: Ctrl+Shift+B
   - Or: F7

#### Using Command Line

```cmd
# Navigate to project directory
cd "nvse_plugin_example"

# Build Debug configuration
msbuild nvse_plugin_example.sln /p:Configuration=Debug /p:Platform=x86

# Build Release configuration  
msbuild nvse_plugin_example.sln /p:Configuration=Release /p:Platform=x86
```

### Build Output

#### Successful Build
```
Build succeeded.
    0 Warning(s)
    0 Error(s)

Time Elapsed 00:00:15.23
```

#### Output Files
- **DLL**: `QuestSync NVSE Plugin.dll` (main plugin)
- **PDB**: Debug symbols (Debug builds only)
- **LIB**: Import library
- **EXP**: Export file

#### Automatic Deployment
If `FalloutNVPath` environment variable is set:
- Plugin DLL automatically copied to game directory
- Configuration file template created if needed

### Common Build Issues

#### Missing Dependencies
**Error**: Cannot find nvse headers
**Solution**: Ensure nvse/ directory contains required header files

#### Environment Variable Issues
**Error**: Cannot copy to $(FalloutNVPath)
**Solution**: 
1. Verify environment variable is set correctly
2. Restart Visual Studio after setting variable
3. Check path exists and is writable

#### Compiler Errors
**Error**: Various C++ compilation errors
**Solution**:
1. Ensure Visual Studio 2022 is installed with C++ workload
2. Check Windows SDK is installed
3. Verify project targets correct platform (x86)

## Building the Server

### Project Configuration

#### Opening the Project
1. Navigate to `Quest Sync Server/`
2. Open `Quest Sync Server.sln` in Visual Studio
3. Wait for project to load

#### Build Configurations

**Debug Configuration:**
- **Console Application**: Shows debug output
- **Optimizations**: Disabled for debugging
- **Runtime Checks**: Enabled
- **Output**: `Debug/Quest Sync Server.exe`

**Release Configuration:**
- **Console Application**: Optimized for performance
- **Optimizations**: Maximum speed optimization
- **Runtime Checks**: Disabled
- **Output**: `Release/Quest Sync Server.exe`

### Build Process

#### Using Visual Studio IDE

1. Select configuration (Debug/Release)
2. Select platform (x86 or x64)
3. Build solution (Ctrl+Shift+B)

#### Using Command Line

```cmd
# Navigate to server directory
cd "Quest Sync Server"

# Build Debug configuration
msbuild "Quest Sync Server.sln" /p:Configuration=Debug /p:Platform=x64

# Build Release configuration
msbuild "Quest Sync Server.sln" /p:Configuration=Release /p:Platform=x64
```

### Build Output

#### Output Files
- **EXE**: `Quest Sync Server.exe` (main executable)
- **PDB**: Debug symbols
- **CFG**: Default configuration file (auto-generated)

#### Dependencies
The server executable includes all required dependencies statically linked.

## Building Unit Tests

### Test Project Setup

#### Opening Test Project
1. Navigate to `Quest Sync Server/Tests/`
2. Open `QuestSyncServerTests.sln` in Visual Studio
3. Allow NuGet package restoration

#### Dependencies

**Google Test Framework:**
- **Package**: Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn
- **Version**: 1.8.1.7
- **Installation**: Automatic via NuGet

**Google Mock Framework:**
- **Package**: gmock
- **Version**: 1.11.0
- **Installation**: Manual via Package Manager Console

### Building Tests

#### Using Visual Studio

1. Build the test solution
2. Open Test Explorer (Test → Test Explorer)
3. Run all tests or specific test suites

#### Using Command Line

```cmd
# Navigate to test directory
cd "Quest Sync Server\Tests"

# Restore NuGet packages
nuget restore

# Build test project
msbuild QuestSyncServerTests.sln /p:Configuration=Debug

# Run tests
QuestSyncServerTests.exe
```

### Test Dependencies

#### Manual Package Installation

If automatic NuGet restoration fails:

```cmd
# Install Google Test
Install-Package Microsoft.googletest.v140.windesktop.msvcstl.static.rt-dyn -Version 1.8.1.7

# Install Google Mock  
Install-Package gmock -Version 1.11.0
```

#### Alternative Package Download

Use provided batch files:
```cmd
# Download packages manually
download-packages.bat

# Restore packages
restore-packages.bat
```

## Build Optimization

### Performance Builds

#### Release Configuration Settings
- **Optimization**: Maximum Speed (/O2)
- **Inline Function Expansion**: Any Suitable (/Ob2)
- **Enable Intrinsic Functions**: Yes (/Oi)
- **Favor Size or Speed**: Favor Speed (/Ot)
- **Whole Program Optimization**: Yes (/GL)

#### Link-Time Code Generation
- **Enable**: Yes (/LTCG)
- **Benefits**: Cross-module optimization
- **Trade-off**: Longer build times

### Debug Builds

#### Debug Configuration Settings
- **Optimization**: Disabled (/Od)
- **Debug Information**: Full (/Zi)
- **Runtime Checks**: Both (/RTC1)
- **Runtime Library**: Debug DLL (/MDd)

## Continuous Integration

### Automated Builds

#### Build Scripts

**Build All Configurations:**
```cmd
@echo off
echo Building Quest Sync Client...
cd "nvse_plugin_example"
msbuild nvse_plugin_example.sln /p:Configuration=Release /p:Platform=x86
if errorlevel 1 goto error

echo Building Quest Sync Server...
cd "..\Quest Sync Server"
msbuild "Quest Sync Server.sln" /p:Configuration=Release /p:Platform=x64
if errorlevel 1 goto error

echo Building Tests...
cd "Tests"
msbuild QuestSyncServerTests.sln /p:Configuration=Release
if errorlevel 1 goto error

echo Build completed successfully!
goto end

:error
echo Build failed!
exit /b 1

:end
```

#### Validation Steps

1. **Build Verification**: Ensure all projects build without errors
2. **Test Execution**: Run unit tests and verify pass rate
3. **Deployment Test**: Verify automatic deployment works
4. **Integration Test**: Basic client-server connectivity

### Build Artifacts

#### Distribution Package

**Client Package:**
- `QuestSync NVSE Plugin.dll`
- `quest_sync.ini` (template)
- Installation instructions

**Server Package:**
- `Quest Sync Server.exe`
- `Quest Sync Server.cfg` (default)
- Server documentation

## Troubleshooting Build Issues

### Common Problems

#### NuGet Package Restoration Failures
**Symptoms**: Missing Google Test headers
**Solutions**:
1. Enable NuGet package restoration in Visual Studio
2. Manually restore packages using Package Manager Console
3. Use provided batch files for manual download

#### Platform Target Mismatches
**Symptoms**: Linker errors about architecture mismatch
**Solutions**:
1. Ensure client builds for x86 (32-bit)
2. Server can build for x86 or x64
3. Check project platform settings

#### Missing Windows SDK
**Symptoms**: Cannot find Windows.h or other system headers
**Solutions**:
1. Install Windows 10/11 SDK through Visual Studio Installer
2. Update project to use installed SDK version
3. Verify SDK installation path

#### Environment Variable Issues
**Symptoms**: Post-build deployment failures
**Solutions**:
1. Verify `FalloutNVPath` is set correctly
2. Restart Visual Studio after setting variables
3. Check target directory permissions

### Build Performance

#### Improving Build Times

**Parallel Builds:**
- Enable multi-processor compilation (/MP)
- Use all available CPU cores
- Configure in project properties

**Incremental Builds:**
- Enable incremental linking
- Use precompiled headers where appropriate
- Minimize unnecessary rebuilds

**SSD Storage:**
- Use SSD for source code and build outputs
- Significantly reduces I/O bottlenecks
- Improves overall build performance

For additional build support, see the [Development Guide](DEVELOPMENT.md) and [Troubleshooting Guide](TROUBLESHOOTING.md).
