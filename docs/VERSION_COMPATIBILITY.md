# Quest Sync Version Compatibility System

This document describes the version compatibility system implemented in Quest Sync to ensure proper communication between the client (NVSE plugin) and server components.

## Overview

Quest Sync uses a robust version checking system that prevents incompatible clients from connecting to the server, ensuring protocol stability and providing clear error messages when version mismatches occur.

## Architecture

### Version Management Structure

The system uses **centralized version constants** in dedicated header files:

- **Server**: `Quest Sync Server/Version.h` - Defines server version and client compatibility ranges
- **Client**: `nvse_plugin_example/Version.h` - Defines client version (immutable, not user-configurable)

### Semantic Versioning (Major.Minor)

Quest Sync uses a two-component versioning scheme:

- **Major version**: Breaking changes that require both client and server updates
- **Minor version**: Backward-compatible improvements within major versions

Example: `1.0`, `1.1`, `2.0`

## Compatibility Rules

### Server-Controlled Compatibility

The server defines which client versions it accepts:

```cpp
// Current server version (major, minor)
constexpr std::array<int, 2> ServerVersion = { 1, 0 };

// Minimum compatible client version
constexpr std::array<int, 2> MinClientVersion = { 1, 0 };

// Maximum compatible client version
constexpr std::array<int, 2> MaxClientVersion = { 1, 9 };
```

### Compatibility Logic

1. **Major versions must match exactly** - Prevents protocol incompatibilities
2. **Minor versions have ranges** - Allows backward compatibility within major versions
3. **Server controls compatibility** - Server defines min/max client versions it supports

## Version Checking Process

### 1. Client-Side Validation

Before attempting connection, the client performs basic sanity checks:

- Version numbers must be non-negative
- Version numbers must be within reasonable bounds (< 100 for major, < 1000 for minor)

### 2. Handshake Protocol

1. **Client connects** and sends its version during handshake
2. **Server analyzes** client version using detailed compatibility checking
3. **Server responds** with accept/reject and specific error message
4. **Connection established** only if versions are compatible

### 3. Enhanced Error Messages

The server provides specific error messages for different compatibility issues:

- **Compatible**: "Connection accepted. Server version: 1.0"
- **Major version mismatch**: "Major version mismatch. Client: 2.0, Server: 1.0. Please update both client and server to matching versions."
- **Client too old**: "Client version too old. Client: 1.0, Server supports: 1.2 to 1.9. Please update your client."
- **Client too new**: "Client version too new. Client: 1.10, Server supports: 1.0 to 1.9. Please update your server."

## Implementation Details

### File Locations

#### Server Version Header
```
Quest Sync Server/Version.h
```

#### Client Version Header
```
nvse_plugin_example/Version.h
```

### Key Components

#### Server-Side Classes
- `Version::CompatibilityResult` - Enum for detailed compatibility results
- `Version::IsCompatible()` - Boolean compatibility check
- `Version::GetCompatibilityResult()` - Detailed compatibility analysis
- `Version::GetCompatibilityErrorMessage()` - Human-readable error messages

#### Client-Side Functions
- `ClientVersion::Version` - Current client version constant
- `ClientVersion::GetVersionString()` - Version as string for logging
- `NetworkClient::ValidateClientVersion()` - Client-side validation

### Integration Points

- **Handshake Protocol**: Version exchange during initial connection
- **Logging System**: Detailed version information in logs
- **Error Handling**: Clear user feedback for version mismatches
- **Connection Management**: Automatic rejection of incompatible clients

## Updating Versions

### For Backward-Compatible Changes

1. Increment the **minor version** in the appropriate `Version.h` file
2. Update server compatibility ranges if needed
3. Test compatibility with existing clients

Example: `1.0` → `1.1`

### For Breaking Changes

1. Increment the **major version** and reset minor to 0
2. Update both client and server versions
3. Adjust server compatibility ranges
4. Coordinate deployment to avoid compatibility issues

Example: `1.9` → `2.0`

### Server Compatibility Range Updates

Adjust `MinClientVersion` and `MaxClientVersion` in the server's `Version.h`:

```cpp
// Example: Support clients from 1.2 to 1.9
constexpr std::array<int, 2> MinClientVersion = { 1, 2 };
constexpr std::array<int, 2> MaxClientVersion = { 1, 9 };
```

## Benefits

1. **Prevents Incompatible Connections** - Clients with incompatible versions are rejected before protocol issues occur
2. **Clear User Guidance** - Detailed error messages tell users exactly what needs to be updated
3. **Backward Compatibility** - Minor version ranges allow gradual rollouts within major versions
4. **Centralized Management** - Version constants are in one place per component, making updates easy
5. **Immutable by Design** - Versions can't be changed via configuration, preventing user errors
6. **Developer Friendly** - Clear separation between client and server version management

## Testing Version Compatibility

### Test Scenarios

1. **Compatible versions** - Same major, minor within range
2. **Major version mismatch** - Different major versions
3. **Client too old** - Client minor below minimum
4. **Client too new** - Client minor above maximum
5. **Invalid versions** - Negative or extremely large version numbers

### Expected Behaviors

- Compatible clients connect successfully
- Incompatible clients receive clear error messages
- Server logs detailed compatibility information
- No protocol confusion or crashes occur

## Troubleshooting

### Common Issues

1. **"Major version mismatch"** - Update both client and server to matching major versions
2. **"Client version too old"** - Update the client to a newer version
3. **"Client version too new"** - Update the server to support newer clients
4. **Connection rejected without clear message** - Check server logs for detailed error information

### Debug Information

Enable debug logging to see detailed version checking information:

- Client logs show version validation and handshake details
- Server logs show compatibility analysis and decision rationale
- Both components log version information during connection attempts

## Future Considerations

- **Patch versions**: Consider adding a third component for hotfixes
- **Feature flags**: Version-specific feature enablement
- **Deprecation warnings**: Advance notice for version support removal
- **Automatic updates**: Integration with update mechanisms
