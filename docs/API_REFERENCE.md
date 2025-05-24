# Quest Sync API Reference

This document provides comprehensive reference information for Quest Sync's configuration options, server commands, and network protocol.

**Status Legend:**
- ✅ **Implemented** - Feature is fully implemented and working
- ⚠️ **Partial** - Feature is partially implemented or has limitations
- ❌ **Planned** - Feature is planned but not yet implemented

## Server Command-Line Interface

The Quest Sync Server provides an interactive command-line interface for server management and monitoring.

### Available Commands

#### help ✅
**Usage:** `help [command]`
**Description:** Display help information for all commands or a specific command
**Examples:**
```
> help
> help stop
> help status
```

#### stop ✅
**Usage:** `stop`
**Aliases:** `exit`, `quit`
**Description:** Gracefully shutdown the server
**Behavior:**
- Disconnects all clients with notification
- Saves current state
- Closes network sockets
- Exits application

#### status ✅
**Usage:** `status`
**Description:** Display current server status and statistics
**Note:** Output format may vary based on current implementation

#### clients ✅
**Usage:** `clients`
**Aliases:** `list`
**Description:** List all connected clients
**Note:** Output format may vary based on current implementation

#### kick ✅
**Usage:** `kick <client_id>`
**Aliases:** `disconnect`
**Description:** Disconnect a specific client
**Parameters:**
- `client_id`: Client identifier from `clients` command

#### broadcast ✅
**Usage:** `broadcast <message>`
**Aliases:** `say`
**Description:** Send a message to all connected clients
**Parameters:**
- `message`: Text message to broadcast

#### config ✅
**Usage:** `config [key] [value]`
**Aliases:** `settings`
**Description:** View or change configuration settings
**Parameters:**
- `key`: Configuration key (optional)
- `value`: New value for the key (optional)

### Planned Commands ❌

The following commands are documented for future implementation:

#### reload ❌
**Usage:** `reload`
**Description:** Reload server configuration from file

#### log ❌
**Usage:** `log <level>`
**Description:** Change logging level at runtime

#### stats ❌
**Usage:** `stats`
**Description:** Display detailed server statistics

#### version ❌
**Usage:** `version`
**Description:** Display server version information

#### clear ❌
**Usage:** `clear`
**Description:** Clear the console screen

## Configuration Reference

### Client Configuration (quest_sync.ini) ✅

The client configuration file is located in the Fallout New Vegas Data/NVSE/Plugins/ directory.

#### [Network] Section ✅

| Setting | Type | Default | Range | Description | Status |
|---------|------|---------|-------|-------------|--------|
| ServerAddress | String | 127.0.0.1 | Valid IP/hostname | Server IP address or hostname | ✅ |
| ServerPort | Integer | 25575 | 1-65535 | Server TCP port number | ✅ |
| ReconnectInterval | Integer | 60 | 1-3600 | Delay between reconnect attempts (seconds) | ✅ |
| MaxReconnectAttempts | Integer | 5 | 0-100 | Max reconnection attempts (0=infinite) | ✅ |
| ConnectionTimeout | Integer | 5 | 1-120 | Connection timeout (seconds) | ✅ |
| SaveGameReconnectAttempts | Integer | 2 | 0-10 | Reconnect attempts when loading save | ✅ |
| SaveGameReconnectDelay | Integer | 1000 | 100-10000 | Delay between save game reconnects (ms) | ✅ |

#### [Client] Section ✅

| Setting | Type | Default | Range | Description | Status |
|---------|------|---------|-------|-------------|--------|
| Version | String | 1.0 | Version string | Client version (auto-managed) | ✅ |
| LogLevel | String | INFO | DEBUG/INFO/WARNING/ERROR | Minimum log level | ✅ |

#### [Quests] Section ❌

The following quest synchronization settings are planned for future implementation:

| Setting | Type | Default | Range | Description | Status |
|---------|------|---------|-------|-------------|--------|
| SyncAllQuests | Boolean | true | true/false | Enable quest synchronization | ❌ |
| SyncModQuests | Boolean | true | true/false | Sync mod-added quests | ❌ |
| SyncVanillaQuests | Boolean | true | true/false | Sync vanilla game quests | ❌ |
| ExcludedQuests | String | (empty) | Comma-separated | Quest IDs to exclude | ❌ |

### Server Configuration (Quest Sync Server.cfg) ✅

The server configuration file is created automatically in the server's running directory.

#### [CommandInterface] Section ✅

| Setting | Type | Default | Range | Description | Status |
|---------|------|---------|-------|-------------|--------|
| Enabled | Boolean | true | true/false | Enable command-line interface | ✅ |

#### [Server] Section ✅

| Setting | Type | Default | Range | Description | Status |
|---------|------|---------|-------|-------------|--------|
| IpAddress | String | (empty) | Valid IP | IP address to bind to (empty = any) | ✅ |
| Port | Integer | 25575 | 1-65535 | TCP listening port | ✅ |

#### [Connection] Section ✅

| Setting | Type | Default | Range | Description | Status |
|---------|------|---------|-------|-------------|--------|
| MaxClients | Integer | 10 | 1-1000 | Maximum concurrent clients | ✅ |
| Timeout | Integer | 30 | 5-300 | Connection timeout (seconds) | ✅ |

#### [Logging] Section ✅

| Setting | Type | Default | Range | Description | Status |
|---------|------|---------|-------|-------------|--------|
| ConsoleLevel | String | INFO | DEBUG/INFO/WARNING/ERROR | Console log level | ✅ |
| FileLevel | String | DEBUG | DEBUG/INFO/WARNING/ERROR | File log level | ✅ |
| LogFile | String | Quest Sync Server.log | Valid path | Log file path | ✅ |

#### [Security] Section ⚠️

Basic security settings (partially implemented):

| Setting | Type | Default | Range | Description | Status |
|---------|------|---------|-------|-------------|--------|
| EnableAuthentication | Boolean | false | true/false | Enable client authentication | ⚠️ |
| AllowedIPs | String | (empty) | IP/CIDR list | Allowed client IP addresses | ⚠️ |

#### [Performance] Section ⚠️

Performance settings (partially implemented):

| Setting | Type | Default | Range | Description | Status |
|---------|------|---------|-------|-------------|--------|
| HeartbeatInterval | Integer | 5 | 1-300 | Client heartbeat interval (seconds) | ⚠️ |

## Network Protocol Reference ✅

### Message Format ✅

All network messages use the following binary format:

```
┌─────────────┬─────────────┬─────────────┐
│ MessageType │ PayloadSize │   Payload   │
│  (2 bytes)  │  (4 bytes)  │ (Variable)  │
└─────────────┴─────────────┴─────────────┘
```

#### Header Fields ✅

| Field | Size | Type | Description |
|-------|------|------|-------------|
| MessageType | 2 bytes | uint16_t | Message type identifier |
| PayloadSize | 4 bytes | uint32_t | Payload size in bytes |
| Payload | Variable | bytes | Message-specific data |

### Message Types ✅

#### Connection Management ✅

| Type | Value | Direction | Description | Status |
|------|-------|-----------|-------------|--------|
| HANDSHAKE_REQUEST | 0 | Client→Server | Initial connection request | ✅ |
| HANDSHAKE_RESPONSE | 1 | Server→Client | Connection response | ✅ |
| HEARTBEAT | 2 | Bidirectional | Keep-alive message | ✅ |
| DISCONNECT | 3 | Bidirectional | Graceful disconnection | ✅ |
| ERROR_MESSAGE | 4 | Bidirectional | Error message | ✅ |

#### Quest Synchronization ✅

| Type | Value | Direction | Description | Status |
|------|-------|-----------|-------------|--------|
| UPDATE_QUEST | 5 | Bidirectional | General quest update | ✅ |
| COMPLETE_QUEST | 6 | Bidirectional | Quest completion | ✅ |
| FAIL_QUEST | 7 | Bidirectional | Quest failure | ✅ |
| START_QUEST | 8 | Bidirectional | Quest started | ✅ |
| COMPLETE_OBJECTIVE | 9 | Bidirectional | Objective completion | ✅ |
| OBJECTIVE_UPDATE | 20 | Bidirectional | Objective state update | ✅ |

#### Reserved ✅

| Type | Value | Direction | Description | Status |
|------|-------|-----------|-------------|--------|
| RESERVED | 65535 | N/A | Reserved for future use | ✅ |

### Data Serialization ✅

#### Basic Types ✅

The current implementation uses simple binary serialization:

| Type | Size | Encoding | Description | Status |
|------|------|----------|-------------|--------|
| uint16_t | 2 bytes | Native | Message type identifier | ✅ |
| uint32_t | 4 bytes | Native | Payload size | ✅ |
| int | 4 bytes | Native | Version components | ✅ |
| bytes | Variable | Raw | Message payload data | ✅ |

**Note:** Current implementation uses native byte order (not explicitly little-endian).

#### Handshake Data Structure ✅

**HandshakeRequest Message:**
```cpp
struct HandshakeRequest {
    std::array<int, 2> clientVersion;  // [major, minor]

    // Serialization: two 4-byte integers
    std::vector<uint8_t> Serialize() const;
    static HandshakeRequest Deserialize(const std::vector<uint8_t>& data);
};
```

#### Quest Data Structures ❌

Quest synchronization data structures are planned for future implementation.

### Error Handling ⚠️

The current implementation uses simple error messages through the ERROR_MESSAGE type. Structured error codes are planned for future implementation.

#### Current Error Handling ✅

- **ERROR_MESSAGE (4)**: General error message with string payload
- **Version compatibility checking**: Automatic rejection of incompatible clients
- **Connection timeouts**: Configurable timeout handling

#### Planned Error Codes ❌

Structured error code system is planned for future implementation:

- Connection errors (timeouts, refused connections)
- Protocol errors (malformed messages, version mismatches)
- Authentication errors (when authentication is implemented)
- Quest synchronization errors (when quest sync is implemented)

## Version Compatibility ✅

### Current Version System ✅

The Quest Sync system uses a simple major.minor version scheme:

- **Server Version**: 1.0 (defined in `Version.h`)
- **Client Version**: 1.0 (defined in client configuration)
- **Compatibility Range**: Server accepts clients 1.0 through 1.9

### Version Compatibility Rules ✅

| Component | Current Version | Compatible Range | Status |
|-----------|----------------|------------------|--------|
| Server | 1.0 | 1.0 (fixed) | ✅ |
| Client | 1.0 | 1.0 - 1.9 | ✅ |

### Compatibility Logic ✅

```cpp
// Server compatibility check
constexpr std::array<int, 2> ServerVersion = { 1, 0 };
constexpr std::array<int, 2> MinClientVersion = { 1, 0 };
constexpr std::array<int, 2> MaxClientVersion = { 1, 9 };

enum class CompatibilityResult {
    Compatible,
    MajorVersionMismatch,
    MinorVersionTooOld,
    MinorVersionTooNew
};
```

### Version Negotiation Process ✅

1. Client sends HANDSHAKE_REQUEST with its version array `[major, minor]`
2. Server checks compatibility using version ranges
3. Server responds with HANDSHAKE_RESPONSE (success) or rejects connection
4. Connection established if compatible, otherwise client is disconnected

### Future Version Planning ❌

Extended version compatibility matrix is planned for future releases when new features are added.

## Usage Examples ✅

### Basic Server Setup ✅

```bash
# Start server with default configuration
Quest Sync Server.exe

# Available commands (implemented)
> help
> status
> clients
> kick <client_id>
> broadcast <message>
> config [key] [value]
> stop
```

### Client Configuration ✅

```ini
# Basic LAN setup (quest_sync.ini)
[Network]
ServerAddress=192.168.1.100
ServerPort=25575
ReconnectInterval=60
MaxReconnectAttempts=5
ConnectionTimeout=5

[Client]
Version=1.0
LogLevel=INFO
```

### Server Configuration ✅

```ini
# Current server configuration (Quest Sync Server.cfg)
[CommandInterface]
Enabled=true

[Server]
IpAddress=
Port=25575

[Connection]
MaxClients=10
Timeout=30

[Logging]
ConsoleLevel=INFO
FileLevel=DEBUG
LogFile=Quest Sync Server.log

[Security]
EnableAuthentication=false
AllowedIPs=

[Performance]
HeartbeatInterval=5
```

### Example Configuration Files ✅

Both client and server configuration files are automatically created with default values if they don't exist.

**Client**: Located in `Fallout New Vegas/Data/NVSE/Plugins/quest_sync.ini`
**Server**: Located in server directory as `Quest Sync Server.cfg`

## Related Documentation

For more detailed information, see the specific documentation files:
- [Configuration Guide](CONFIGURATION.md) ✅
- [Version Compatibility Guide](VERSION_COMPATIBILITY.md) ✅
- [Troubleshooting Guide](TROUBLESHOOTING.md) ⚠️

**Note**: This API reference now accurately reflects the current implementation status. Features marked with ❌ are planned for future development.
