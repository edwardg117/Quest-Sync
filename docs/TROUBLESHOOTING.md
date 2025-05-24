# Quest Sync Troubleshooting Guide

This guide helps diagnose and resolve common issues with Quest Sync client and server components.

## Quick Diagnosis

### Connection Issues

#### "Cannot connect to server"

**Symptoms:**
- Client shows connection failed message
- No connection notification in game
- Client logs show connection timeout

**Quick Fixes:**
1. Verify server is running: Check if `Quest Sync Server.exe` is active
2. Check server address in `quest_sync.ini`: Ensure correct IP/hostname
3. Verify port settings: Client `ServerPort` must match server `Port`
4. Test network connectivity: `ping [server-address]`

#### "Connection refused"

**Symptoms:**
- Immediate connection rejection
- Server logs show no connection attempts
- Client receives instant failure

**Quick Fixes:**
1. Check firewall settings on server machine
2. Verify server is listening on correct port
3. Ensure no other application is using the port
4. Check server `BindAddress` setting

### Plugin Issues

#### "Plugin not loading"

**Symptoms:**
- No Quest Sync messages in game
- Plugin DLL not found in process
- xNVSE reports plugin load failure

**Quick Fixes:**
1. Verify xNVSE installation: Check `nvse_loader.exe` exists
2. Check plugin placement: DLL should be in game directory
3. Verify game compatibility: Ensure Fallout NV version matches
4. Check plugin dependencies: Ensure all required files present

## Detailed Troubleshooting

### Client Issues

#### Plugin Loading Problems

**Problem**: Quest Sync plugin fails to load with xNVSE

**Diagnosis Steps:**
1. Check xNVSE installation:
   ```
   Fallout New Vegas/
   ├── nvse_loader.exe
   ├── nvse_1_4.dll
   └── Data/NVSE/Plugins/
   ```

2. Verify plugin files:
   ```
   Fallout New Vegas/
   ├── QuestSync NVSE Plugin.dll
   └── quest_sync.ini
   ```

3. Check xNVSE log for errors:
   - Location: `Fallout New Vegas/nvse.log`
   - Look for plugin loading messages
   - Check for dependency errors

**Solutions:**
- **Missing xNVSE**: Download and install xNVSE 6.0+
- **Wrong plugin location**: Move DLL to game root directory
- **Corrupted files**: Re-download and reinstall plugin
- **Version mismatch**: Ensure plugin version matches xNVSE version

#### Configuration Issues

**Problem**: Plugin loads but doesn't connect to server

**Diagnosis Steps:**
1. Check configuration file format:
   ```ini
   [Network]
   ServerAddress=127.0.0.1
   ServerPort=25575
   ```

2. Validate configuration values:
   - ServerAddress: Valid IP or hostname
   - ServerPort: Number between 1-65535
   - No extra spaces or special characters

3. Check file permissions:
   - Ensure `quest_sync.ini` is readable
   - Verify no file corruption

**Solutions:**
- **Invalid format**: Recreate config file with correct format
- **Wrong values**: Update ServerAddress and ServerPort
- **Permission issues**: Run game as administrator
- **File corruption**: Delete and recreate configuration file

#### Runtime Connection Issues

**Problem**: Plugin connects initially but loses connection

**Diagnosis Steps:**
1. Enable debug logging:
   ```ini
   [Client]
   LogLevel=DEBUG
   ```

2. Monitor connection in logs:
   - Look for connection/disconnection messages
   - Check for network errors
   - Monitor reconnection attempts

3. Test network stability:
   - Use `ping -t [server-address]` for continuous ping
   - Check for packet loss or high latency
   - Verify network adapter stability

**Solutions:**
- **Network instability**: Check network hardware and drivers
- **Server overload**: Reduce number of connected clients
- **Firewall interference**: Add exceptions for game and plugin
- **Router issues**: Check port forwarding and UPnP settings

### Server Issues

#### Server Startup Problems

**Problem**: Quest Sync Server fails to start

**Diagnosis Steps:**
1. Check server logs:
   ```
   Quest Sync Server/Quest Sync Server.log
   ```

2. Verify configuration:
   ```ini
   [Network]
   Port=25575
   BindAddress=0.0.0.0
   ```

3. Test port availability:
   ```cmd
   netstat -an | findstr :25575
   ```

**Solutions:**
- **Port in use**: Change port number or stop conflicting application
- **Permission denied**: Run server as administrator
- **Invalid configuration**: Check config file syntax and values
- **Missing dependencies**: Install Visual C++ Redistributable

#### Client Connection Rejection

**Problem**: Server rejects client connections

**Diagnosis Steps:**
1. Check server logs for connection attempts
2. Verify version compatibility between client and server
3. Check security settings in server configuration
4. Monitor server resource usage

**Solutions:**
- **Version mismatch**: Update client or server to compatible versions
- **Security restrictions**: Check AllowedIPs setting
- **Connection limit**: Increase MaxConnections setting
- **Resource exhaustion**: Restart server or increase system resources

#### Performance Issues

**Problem**: Server becomes slow or unresponsive

**Diagnosis Steps:**
1. Monitor server performance:
   - CPU usage
   - Memory consumption
   - Network bandwidth
   - Client connection count

2. Check for error patterns in logs
3. Analyze message processing rates
4. Review client behavior patterns

**Solutions:**
- **High CPU usage**: Reduce client count or optimize message processing
- **Memory leaks**: Restart server and monitor for recurring issues
- **Network congestion**: Implement message batching or rate limiting
- **Client flooding**: Implement client rate limiting

### Network Issues

#### Firewall Configuration

**Problem**: Connections blocked by firewall

**Windows Firewall:**
1. Open Windows Defender Firewall
2. Click "Allow an app or feature through Windows Defender Firewall"
3. Add exceptions for:
   - `Quest Sync Server.exe`
   - `FalloutNV.exe`
   - Port 25575 (or configured port)

**Router Configuration:**
1. Access router admin interface
2. Navigate to Port Forwarding settings
3. Forward server port to server machine IP
4. Enable UPnP if available

#### Network Connectivity

**Problem**: Clients cannot reach server over network

**Local Network (LAN):**
1. Verify all machines on same network
2. Test connectivity: `ping [server-ip]`
3. Check IP address configuration
4. Verify subnet masks and gateways

**Internet Connection:**
1. Verify public IP address of server
2. Check port forwarding configuration
3. Test external connectivity
4. Consider using dynamic DNS service

### Game Integration Issues

#### Quest Synchronization Problems

**Problem**: Quests not synchronizing between clients

**Diagnosis Steps:**
1. Enable debug logging on all clients
2. Check for quest compatibility issues
3. Verify all clients have same mods installed
4. Monitor quest update messages in logs

**Solutions:**
- **Mod differences**: Ensure all clients have identical mod setup
- **Quest conflicts**: Disable conflicting quest mods
- **Save game issues**: Start with fresh save files
- **Version differences**: Use same game version across all clients

#### Game Stability Issues

**Problem**: Game crashes or becomes unstable with plugin

**Diagnosis Steps:**
1. Test game stability without plugin
2. Check for mod conflicts
3. Review crash logs and error messages
4. Test with minimal mod configuration

**Solutions:**
- **Mod conflicts**: Disable other mods to isolate conflicts
- **Memory issues**: Use 4GB patch and memory optimization mods
- **Plugin bugs**: Report issues with detailed crash information
- **Game corruption**: Verify game file integrity

## Advanced Diagnostics

### Log Analysis

#### Client Logs

**Location**: `Fallout New Vegas/quest_sync_client.log`

**Key Messages:**
```
[INFO] Quest Sync Plugin initialized
[INFO] Connecting to server 127.0.0.1:25575
[INFO] Connected to server successfully
[DEBUG] Quest update sent: Quest 0x12345, Stage 10
[ERROR] Connection lost, attempting reconnection
```

#### Server Logs

**Location**: `Quest Sync Server/Quest Sync Server.log`

**Key Messages:**
```
[INFO] Server started on port 25575
[INFO] Client connected from 192.168.1.100
[DEBUG] Quest update received from client
[WARNING] Client version mismatch: 1.0 vs 1.1
[ERROR] Client disconnected unexpectedly
```

### Network Monitoring

#### Packet Capture

Use Wireshark to analyze network traffic:

1. Install Wireshark
2. Capture on network interface
3. Filter by port: `tcp.port == 25575`
4. Analyze connection patterns and message flow

#### Connection Testing

**Basic Connectivity:**
```cmd
telnet [server-address] [port]
```

**Port Scanning:**
```cmd
nmap -p 25575 [server-address]
```

### Performance Monitoring

#### Server Performance

Monitor these metrics:
- CPU usage per core
- Memory usage and growth patterns
- Network bandwidth utilization
- Client connection count
- Message processing rate

#### Client Performance

Monitor these metrics:
- Game frame rate impact
- Memory usage increase
- Network bandwidth usage
- Quest update frequency

## Common Error Messages

### Client Error Messages

#### "Failed to initialize network client"
- **Cause**: Network subsystem initialization failure
- **Solution**: Check network adapter drivers and Windows socket support

#### "Server version incompatible"
- **Cause**: Client and server version mismatch
- **Solution**: Update client or server to matching versions

#### "Configuration file not found"
- **Cause**: Missing `quest_sync.ini` file
- **Solution**: Create configuration file with default settings

### Server Error Messages

#### "Failed to bind to port"
- **Cause**: Port already in use or permission denied
- **Solution**: Change port or run as administrator

#### "Client authentication failed"
- **Cause**: Invalid client credentials (if authentication enabled)
- **Solution**: Check authentication settings and credentials

#### "Maximum connections reached"
- **Cause**: Too many clients connected
- **Solution**: Increase MaxConnections setting or disconnect unused clients

## Getting Help

### Information to Provide

When reporting issues, include:

1. **System Information:**
   - Operating system version
   - Fallout New Vegas version
   - xNVSE version
   - Quest Sync version

2. **Configuration Files:**
   - `quest_sync.ini` (client)
   - `Quest Sync Server.cfg` (server)

3. **Log Files:**
   - Client logs with debug level enabled
   - Server logs with debug level enabled
   - Game crash logs if applicable

4. **Network Information:**
   - Network topology (LAN/Internet)
   - Firewall and router configuration
   - Number of connected clients

### Support Channels

- **GitHub Issues**: Create detailed issue reports
- **Discord**: Join xNVSE Discord for community support
- **Documentation**: Check other documentation files for detailed information

### Self-Help Resources

- [Configuration Guide](CONFIGURATION.md) - Detailed configuration options
- [Networking Guide](NETWORKING.md) - Network architecture and protocols
- [Development Guide](DEVELOPMENT.md) - Development and debugging information
- [Testing Guide](TESTING.md) - Testing procedures and validation
