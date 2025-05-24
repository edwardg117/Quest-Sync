# Quest Sync Configuration Guide

This document provides comprehensive configuration information for both the Quest Sync client (NVSE plugin) and server components.

## Client Configuration (quest_sync.ini)

The client configuration file `quest_sync.ini` is located in the Fallout New Vegas directory alongside the plugin DLL.

### File Location

```
Fallout New Vegas/
├── FalloutNV.exe
├── nvse_1_4.dll
├── QuestSync NVSE Plugin.dll
└── quest_sync.ini
```

### Configuration Format

The configuration file uses standard INI format with sections and key-value pairs:

```ini
[Section]
Key=Value
# Comments start with hash symbol
```

### Network Settings

#### [Network] Section

```ini
[Network]
# Server connection settings
ServerAddress=127.0.0.1
ServerPort=25575
ReconnectInterval=60
MaxReconnectAttempts=5
ConnectionTimeout=5
SaveGameReconnectAttempts=2
SaveGameReconnectDelay=1000
```

**ServerAddress**
- **Type**: String (IP address or hostname)
- **Default**: `127.0.0.1`
- **Description**: IP address or hostname of the Quest Sync server
- **Examples**:
  - `127.0.0.1` (local server)
  - `192.168.1.100` (LAN server)
  - `questsync.example.com` (internet server)

**ServerPort**
- **Type**: Integer
- **Default**: `25575`
- **Range**: 1-65535
- **Description**: TCP port number for server connection
- **Note**: Must match server configuration

**MaxReconnectAttempts**
- **Type**: Integer
- **Default**: `5`
- **Range**: 0-100 (0 = infinite attempts)
- **Description**: Maximum number of reconnection attempts after connection loss
- **Status**: ✅ **IMPLEMENTED** - Fully functional with counter reset on successful connections

**ReconnectInterval**
- **Type**: Integer (seconds)
- **Default**: `60`
- **Range**: 1-3600
- **Description**: Time delay between reconnection attempts
- **Status**: ✅ **IMPLEMENTED** - Configurable interval between retry attempts

**ConnectionTimeout**
- **Type**: Integer (seconds)
- **Default**: `5`
- **Range**: 1-120
- **Description**: Timeout for connection establishment
- **Status**: ✅ **IMPLEMENTED** - Configurable connection timeout

**SaveGameReconnectAttempts**
- **Type**: Integer
- **Default**: `2`
- **Range**: 0-10
- **Description**: Maximum reconnection attempts when loading save games
- **Status**: ✅ **IMPLEMENTED** - Separate limit for save game scenarios

**SaveGameReconnectDelay**
- **Type**: Integer (milliseconds)
- **Default**: `1000`
- **Range**: 100-10000
- **Description**: Delay between save game reconnection attempts
- **Status**: ✅ **IMPLEMENTED** - Faster retry for save game loading

### Logging Settings

#### [Client] Section

```ini
[Client]
# Logging configuration
LogLevel=INFO
LogToFile=true
LogFilePath=quest_sync_client.log
MaxLogFileSize=10485760
```

**LogLevel**
- **Type**: String (enum)
- **Default**: `INFO`
- **Values**: `DEBUG`, `INFO`, `WARNING`, `ERROR`
- **Description**: Minimum log level for messages
- **Status**: ✅ Implemented

**LogToFile**
- **Type**: Boolean
- **Default**: `true`
- **Description**: Enable logging to file

**LogFilePath**
- **Type**: String (file path)
- **Default**: `quest_sync_client.log`
- **Description**: Path for log file (relative to game directory)

**MaxLogFileSize**
- **Type**: Integer (bytes)
- **Default**: `10485760` (10 MB)
- **Description**: Maximum log file size before rotation

### Quest Synchronization Settings

#### [Quests] Section

```ini
[Quests]
# Quest synchronization options
SyncAllQuests=true
SyncModQuests=true
SyncVanillaQuests=true
ExcludedQuests=
```

**SyncAllQuests**
- **Type**: Boolean
- **Default**: `true`
- **Description**: Enable quest synchronization
- **Status**: ⚠️ Planned feature

**SyncModQuests**
- **Type**: Boolean
- **Default**: `true`
- **Description**: Synchronize quests from mods
- **Status**: ⚠️ Planned feature

**SyncVanillaQuests**
- **Type**: Boolean
- **Default**: `true`
- **Description**: Synchronize vanilla game quests
- **Status**: ⚠️ Planned feature

**ExcludedQuests**
- **Type**: String (comma-separated list)
- **Default**: (empty)
- **Description**: Quest IDs to exclude from synchronization
- **Example**: `MS01,MS02,PersonalQuest`
- **Status**: ⚠️ Planned feature

## Server Configuration (Quest Sync Server.cfg)

The server configuration file is located in the server executable directory.

### File Location

```
Quest Sync Server/
├── Quest Sync Server.exe
├── Quest Sync Server.cfg
└── Quest Sync Server.log
```

### Network Settings

#### [Network] Section

```ini
[Network]
# Server network configuration
Port=25575
MaxConnections=50
BindAddress=0.0.0.0
```

**Port**
- **Type**: Integer
- **Default**: `25575`
- **Range**: 1-65535
- **Description**: TCP port for client connections
- **Note**: Must be open in firewall

**MaxConnections**
- **Type**: Integer
- **Default**: `50`
- **Range**: 1-1000
- **Description**: Maximum concurrent client connections

**BindAddress**
- **Type**: String (IP address)
- **Default**: `0.0.0.0`
- **Description**: IP address to bind server socket
- **Values**:
  - `0.0.0.0` (all interfaces)
  - `127.0.0.1` (localhost only)
  - Specific IP address

### Security Settings

#### [Security] Section

```ini
[Security]
# Security and access control
EnableAuthentication=false
AllowedIPs=
RequireEncryption=false
```

**EnableAuthentication**
- **Type**: Boolean
- **Default**: `false`
- **Description**: Enable user authentication
- **Status**: ⚠️ Planned feature (not yet implemented)

**AllowedIPs**
- **Type**: String (comma-separated list)
- **Default**: (empty - all IPs allowed)
- **Description**: IP addresses allowed to connect
- **Example**: `192.168.1.0/24,10.0.0.100`
- **Status**: ⚠️ Planned feature (not yet implemented)

**RequireEncryption**
- **Type**: Boolean
- **Default**: `false`
- **Description**: Require encrypted connections
- **Status**: ⚠️ Planned feature (not yet implemented)

### Performance Settings

#### [Performance] Section

```ini
[Performance]
# Performance and optimization
HeartbeatInterval=30000
MessageQueueSize=1000
ThreadPoolSize=4
```

**HeartbeatInterval**
- **Type**: Integer (milliseconds)
- **Default**: `30000`
- **Range**: 10000-300000
- **Description**: Interval for client heartbeat checks
- **Status**: ⚠️ Planned feature (not yet implemented)

**MessageQueueSize**
- **Type**: Integer
- **Default**: `1000`
- **Range**: 100-10000
- **Description**: Maximum queued messages per client

**ThreadPoolSize**
- **Type**: Integer
- **Default**: `4`
- **Range**: 1-16
- **Description**: Number of worker threads for message processing

### Logging Settings

#### [Logging] Section

```ini
[Logging]
# Server logging configuration
LogLevel=INFO
LogToFile=true
LogFilePath=Quest Sync Server.log
MaxLogFileSize=52428800
EnableConsoleOutput=true
```

**LogLevel**
- **Type**: String (enum)
- **Default**: `INFO`
- **Values**: `DEBUG`, `INFO`, `WARNING`, `ERROR`
- **Description**: Minimum log level for messages

**LogToFile**
- **Type**: Boolean
- **Default**: `true`
- **Description**: Enable logging to file

**LogFilePath**
- **Type**: String (file path)
- **Default**: `Quest Sync Server.log`
- **Description**: Path for log file

**MaxLogFileSize**
- **Type**: Integer (bytes)
- **Default**: `52428800` (50 MB)
- **Description**: Maximum log file size before rotation

**EnableConsoleOutput**
- **Type**: Boolean
- **Default**: `true`
- **Description**: Display log messages in console

## Configuration Management

### Default Configuration Creation

Both client and server automatically create default configuration files if none exist:

- **Client**: Creates `quest_sync.ini` with default values on first run
- **Server**: Creates `Quest Sync Server.cfg` with default values on startup

### Configuration Validation

#### Startup Validation

Both components validate configuration on startup:

- Check for required settings
- Validate value ranges and types
- Report configuration errors
- Use default values for missing settings

#### Runtime Validation

- Network settings are validated before connection attempts
- Invalid values are logged with warnings
- Fallback to default values when possible

### Configuration Updates

#### Hot Reload

- **Server**: Supports configuration reload via command interface
- **Client**: Requires game restart for configuration changes

#### Backward Compatibility

- New configuration options include default values
- Existing configurations continue to work with new versions
- Deprecated options are logged with warnings

## Troubleshooting Configuration

### Common Issues

#### Connection Problems

1. **"Cannot connect to server"**
   - Verify `ServerAddress` and `ServerPort` in client config
   - Check server is running and accessible
   - Verify firewall settings

2. **"Connection refused"**
   - Check server `Port` setting matches client `ServerPort`
   - Verify server `BindAddress` allows connections
   - Check for port conflicts

#### Configuration File Issues

1. **"Configuration file not found"**
   - File will be created automatically with defaults
   - Check file permissions in game/server directory

2. **"Invalid configuration value"**
   - Check value types and ranges
   - Review log files for specific error details
   - Restore default configuration if needed

### Debug Configuration

For troubleshooting, use these debug settings:

#### Client Debug Config
```ini
[Client]
LogLevel=DEBUG

[Network]
ConnectionTimeout=30
MaxReconnectAttempts=10
ReconnectInterval=30
```

#### Server Debug Config
```ini
[Logging]
LogLevel=DEBUG
EnableConsoleOutput=true

[Performance]
HeartbeatInterval=10000
```

### Configuration Examples

#### LAN Server Setup

**Server Config:**
```ini
[Network]
Port=25575
BindAddress=0.0.0.0
MaxConnections=10
```

**Client Config:**
```ini
[Network]
ServerAddress=192.168.1.100
ServerPort=25575
ReconnectInterval=60
MaxReconnectAttempts=5
ConnectionTimeout=5
```

#### Internet Server Setup

**Server Config:**
```ini
[Network]
Port=25575
BindAddress=0.0.0.0
MaxConnections=20

[Security]
AllowedIPs=203.0.113.0/24
```

**Client Config:**
```ini
[Network]
ServerAddress=questsync.example.com
ServerPort=25575
ReconnectInterval=60
MaxReconnectAttempts=5
ConnectionTimeout=15
```

For additional configuration help, see the [Troubleshooting Guide](TROUBLESHOOTING.md) or create an issue on the project repository.

