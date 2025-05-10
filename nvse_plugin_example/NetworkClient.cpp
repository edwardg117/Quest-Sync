#include "NetworkClient.h"
#include "Config.h"
#include <iostream>
#include <chrono>
#include "nvse/PluginAPI.h"

// Constructor
NetworkClient::NetworkClient(const std::string& serverAddress, int serverPort)
    : m_serverAddress(serverAddress),
      m_serverPort(serverPort),
      m_socket(INVALID_SOCKET),
      m_connected(false),
      m_initialized(false),
      m_handshakeCompleted(false),
      m_clientVersion{1, 0},
      m_debugMode(true),
      m_reconnectInterval(60),
      m_maxReconnectAttempts(5),
      m_reconnectAttempts(0),
      m_threadRunning(false) {
}

// Destructor
NetworkClient::~NetworkClient() {
    Cleanup();
}

// Initialize the network client
bool NetworkClient::Initialize() {
    if (m_initialized) {
        return true;
    }

    // Initialize Winsock
    WSADATA wsData;
    WORD ver = MAKEWORD(2, 2);
    int wsResult = WSAStartup(ver, &wsData);
    if (wsResult != 0) {
        std::cerr << "Failed to initialize Winsock: " << wsResult << std::endl;
        return false;
    }

    // Load configuration
    Config& config = Config::GetInstance();
    m_reconnectInterval = config.GetInt("Network.ReconnectInterval", 60);
    m_maxReconnectAttempts = config.GetInt("Network.MaxReconnectAttempts", 5);

    m_initialized = true;
    return true;
}

// Connect to the server
bool NetworkClient::Connect() {
    _MESSAGE("NetworkClient::Connect - Attempting to connect to %s:%d", m_serverAddress.c_str(), m_serverPort);
    _MESSAGE("NetworkClient::Connect - Client version: %d.%d", m_clientVersion[0], m_clientVersion[1]);
    _MESSAGE("NetworkClient::Connect - Debug mode: %s", m_debugMode ? "enabled" : "disabled");

    try {
        // Check if already connected
        if (m_connected) {
            _MESSAGE("NetworkClient::Connect - Already connected");
            return true;
        }

        // Initialize if not already initialized
        if (!m_initialized) {
            if (!Initialize()) {
                _MESSAGE("NetworkClient::Connect - Failed to initialize");
                return false;
            }
        }
        
        // Log memory sizes for debugging
        _MESSAGE("NetworkClient::Connect - Size of bool: %u bytes", sizeof(bool));
        _MESSAGE("NetworkClient::Connect - Size of uint32_t: %u bytes", sizeof(uint32_t));
        _MESSAGE("NetworkClient::Connect - Size of MessageHeader: %u bytes", sizeof(MessageHeader));
        _MESSAGE("NetworkClient::Connect - Size of HandshakeRequest: %u bytes", sizeof(HandshakeRequest));
        _MESSAGE("NetworkClient::Connect - Size of HandshakeResponse: %u bytes", sizeof(HandshakeResponse));

        // Create socket
        _MESSAGE("NetworkClient::Connect - Creating socket");
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) {
            int error = WSAGetLastError();
            _MESSAGE("NetworkClient::Connect - Failed to create socket, error: %d (0x%08X)", error, error);
            return false;
        }
        _MESSAGE("NetworkClient::Connect - Socket created successfully: %d", m_socket);

        // Set up server address
        _MESSAGE("NetworkClient::Connect - Setting up server address");
        sockaddr_in serverAddr;
        ZeroMemory(&serverAddr, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(m_serverPort);

        int ipResult = inet_pton(AF_INET, m_serverAddress.c_str(), &serverAddr.sin_addr);
        if (ipResult != 1) {
            _MESSAGE("NetworkClient::Connect - Failed to convert IP address, result: %d, error: %d",
                    ipResult, WSAGetLastError());
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &serverAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        _MESSAGE("NetworkClient::Connect - Server address set to %s:%d", ipStr, ntohs(serverAddr.sin_port));

        // Get connection timeout from config
        Config& config = Config::GetInstance();
        int timeoutSeconds = config.GetInt("Network.ConnectionTimeout", 5);
        DWORD timeout = timeoutSeconds * 1000; // Convert to milliseconds
        _MESSAGE("NetworkClient::Connect - Using connection timeout of %d seconds (%d ms)", timeoutSeconds, timeout);
        setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        // Connect to server
        _MESSAGE("NetworkClient::Connect - Connecting to server with %d second timeout...", timeoutSeconds);
        int result = connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr));
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            _MESSAGE("NetworkClient::Connect - Failed to connect, error: %d (0x%08X)", error, error);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        _MESSAGE("NetworkClient::Connect - Connected successfully to %s:%d", ipStr, ntohs(serverAddr.sin_port));
        m_connected = true;

        // Set socket to non-blocking mode
        u_long mode = 1;  // 1 = non-blocking, 0 = blocking
        result = ioctlsocket(m_socket, FIONBIO, &mode);
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            _MESSAGE("NetworkClient::Connect - Failed to set socket to non-blocking mode, error: %d (0x%08X)",
                    error, error);
            // Continue anyway, but log the error
        } else {
            _MESSAGE("NetworkClient::Connect - Socket set to non-blocking mode");
        }

        // Start receive thread
        _MESSAGE("NetworkClient::Connect - Starting receive thread");
        m_threadRunning = true;

        // Start the receive thread
        _MESSAGE("NetworkClient::Connect - Creating receive thread");
        m_receiveThread = std::thread(&NetworkClient::ReceiveThreadFunction, this);
        _MESSAGE("NetworkClient::Connect - Receive thread created");

        // Send handshake request to authenticate with the server
        _MESSAGE("NetworkClient::Connect - Sending handshake request");
        m_handshakeCompleted = false;
        if (!SendHandshake()) {
            _MESSAGE("NetworkClient::Connect - Failed to send handshake request");
            Disconnect();
            return false;
        }
        _MESSAGE("NetworkClient::Connect - Handshake request sent successfully");

        // Wait for handshake to complete (with timeout)
        _MESSAGE("NetworkClient::Connect - Waiting for handshake to complete");
        int handshakeAttempts = 0;
        const int maxHandshakeAttempts = 10; // Reduced timeout to fail faster if server is not responding

        while (!m_handshakeCompleted && handshakeAttempts < maxHandshakeAttempts) {
            _MESSAGE("NetworkClient::Connect - Handshake attempt %d/%d", handshakeAttempts + 1, maxHandshakeAttempts);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ProcessMessages(); // Process any pending messages, including handshake response
            handshakeAttempts++;
        }

        if (!m_handshakeCompleted) {
            _MESSAGE("NetworkClient::Connect - Handshake timed out after %d attempts", handshakeAttempts);
            _MESSAGE("NetworkClient::Connect - Server may not be running or may be incompatible");
            Disconnect();
            return false;
        }

        _MESSAGE("NetworkClient::Connect - Handshake completed successfully");
        return true;
    }
    catch (const std::exception& e) {
        _MESSAGE("NetworkClient::Connect - Exception during connect: %s", e.what());
        
        // Clean up resources
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_connected = false;
        
        return false;
    }
    catch (...) {
        _MESSAGE("NetworkClient::Connect - Unknown exception during connect");
        
        // Clean up resources
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_connected = false;
        
        return false;
    }
}

// Disconnect from the server
void NetworkClient::Disconnect() {
    _MESSAGE("NetworkClient::Disconnect - Called");

    // Set connected flag to false first to prevent callbacks from being called multiple times
    bool wasConnected = m_connected.exchange(false);

    if (!wasConnected && m_socket == INVALID_SOCKET) {
        _MESSAGE("NetworkClient::Disconnect - Already disconnected");
        return;
    }

    _MESSAGE("NetworkClient::Disconnect - Stopping receive thread");
    // Stop receive thread
    m_threadRunning = false;

    // Clear message queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<std::unique_ptr<Message>> empty;
        std::swap(m_messageQueue, empty);
        _MESSAGE("NetworkClient::Disconnect - Message queue cleared");
    }

    // Close socket safely
    if (m_socket != INVALID_SOCKET) {
        _MESSAGE("NetworkClient::Disconnect - Closing socket");

        // Send disconnect message if possible
        try {
            Message disconnectMsg(MessageType::DISCONNECT);
            // Don't use SendMessage as it checks m_connected which we've already set to false
            std::vector<uint8_t> data = disconnectMsg.Serialize();
            int result = send(m_socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);
            if (result != SOCKET_ERROR) {
                _MESSAGE("NetworkClient::Disconnect - Sent disconnect message successfully");
            } else {
                _MESSAGE("NetworkClient::Disconnect - Failed to send disconnect message, error: %d", WSAGetLastError());
            }
        }
        catch (const std::exception& e) {
            _MESSAGE("NetworkClient::Disconnect - Error sending disconnect message: %s", e.what());
            // Ignore errors during disconnect
        }
        catch (...) {
            _MESSAGE("NetworkClient::Disconnect - Unknown error sending disconnect message");
            // Ignore errors during disconnect
        }

        // Shutdown the socket before closing it
        try {
            int result = shutdown(m_socket, SD_BOTH);  // Shutdown both send and receive operations
            if (result == SOCKET_ERROR) {
                _MESSAGE("NetworkClient::Disconnect - Socket shutdown failed, error: %d", WSAGetLastError());
            }
        }
        catch (const std::exception& e) {
            _MESSAGE("NetworkClient::Disconnect - Exception during socket shutdown: %s", e.what());
        }
        catch (...) {
            _MESSAGE("NetworkClient::Disconnect - Unknown exception during socket shutdown");
        }

        // Close the socket
        int result = closesocket(m_socket);
        if (result == SOCKET_ERROR) {
            _MESSAGE("NetworkClient::Disconnect - Socket close failed, error: %d", WSAGetLastError());
        }

        m_socket = INVALID_SOCKET;
    }

    m_handshakeCompleted = false;
    _MESSAGE("NetworkClient::Disconnect - Connection status set to disconnected");

    // Wait for receive thread to finish with a timeout
    if (m_receiveThread.joinable()) {
        try {
            _MESSAGE("NetworkClient::Disconnect - Waiting for receive thread to finish");

            // Signal the thread to exit
            m_threadRunning = false;

            // Use a timeout to avoid hanging if the thread is stuck
            auto start = std::chrono::steady_clock::now();
            auto timeout = std::chrono::seconds(1); // Reduced timeout to 1 second

            // First, try to join with a timeout using polling
            while (m_receiveThread.joinable() &&
                   std::chrono::steady_clock::now() - start < timeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Shorter sleep intervals
            }

            // If thread is still joinable, detach it instead of waiting indefinitely
            if (m_receiveThread.joinable()) {
                _MESSAGE("NetworkClient::Disconnect - Thread join timeout, detaching thread");

                // Detach the thread to prevent crashes
                try {
                    m_receiveThread.detach();
                    _MESSAGE("NetworkClient::Disconnect - Thread detached successfully");
                }
                catch (const std::exception& e) {
                    _MESSAGE("NetworkClient::Disconnect - Exception during thread detach: %s", e.what());
                }
                catch (...) {
                    _MESSAGE("NetworkClient::Disconnect - Unknown exception during thread detach");
                }
            } else {
                _MESSAGE("NetworkClient::Disconnect - Thread joined successfully");
            }
        }
        catch (const std::exception& e) {
            _MESSAGE("NetworkClient::Disconnect - Exception during thread join: %s", e.what());

            // If there's an exception, detach the thread
            if (m_receiveThread.joinable()) {
                try {
                    m_receiveThread.detach();
                    _MESSAGE("NetworkClient::Disconnect - Thread detached after exception");
                }
                catch (...) {
                    _MESSAGE("NetworkClient::Disconnect - Failed to detach thread after exception");
                }
            }
        }
        catch (...) {
            _MESSAGE("NetworkClient::Disconnect - Unknown exception during thread join");

            // If there's an exception, detach the thread
            if (m_receiveThread.joinable()) {
                try {
                    m_receiveThread.detach();
                    _MESSAGE("NetworkClient::Disconnect - Thread detached after unknown exception");
                }
                catch (...) {
                    _MESSAGE("NetworkClient::Disconnect - Failed to detach thread after unknown exception");
                }
            }
        }
    }

    // Reset reconnect attempts to ensure we can reconnect later
    m_reconnectAttempts = 0;

    // Notify connection status
    if (m_connectionCallback && wasConnected) {
        try {
            _MESSAGE("NetworkClient::Disconnect - Calling connection status callback");
            m_connectionCallback(this, false);
        }
        catch (const std::exception& e) {
            _MESSAGE("NetworkClient::Disconnect - Exception in connection callback: %s", e.what());
        }
        catch (...) {
            _MESSAGE("NetworkClient::Disconnect - Unknown exception in connection callback");
        }
    }

    _MESSAGE("NetworkClient::Disconnect - Disconnection complete");
}

// Clean up resources
void NetworkClient::Cleanup() {
    Disconnect();

    if (m_initialized) {
        WSACleanup();
        m_initialized = false;
    }
}

// Check if connected to the server
bool NetworkClient::IsConnected() const {
    // Only log occasionally to avoid filling the log file
    static int logCounter = 0;
    if (logCounter++ % 1000 == 0) {
        _MESSAGE("NetworkClient::IsConnected - Status: %s", m_connected ? "connected" : "not connected");
    }
    return m_connected;
}

// Check if the client is initialized
bool NetworkClient::IsInitialized() const {
    return m_initialized;
}

// Send a message to the server
bool NetworkClient::SendMessage(const Message& message) {
    if (!m_connected) {
        _MESSAGE("NetworkClient::SendMessage - Not connected, cannot send message");
        return false;
    }

    // Serialize message
    std::vector<uint8_t> data = message.Serialize();

    // Log message details
    if (m_debugMode) {
        _MESSAGE("NetworkClient::SendMessage - Sending message type %d, size %u bytes",
                 static_cast<int>(message.GetType()), data.size());

        // Dump message bytes for debugging
        std::string dataHex;
        for (size_t i = 0; i < data.size() && i < 64; ++i) {
            char hex[8];
            sprintf_s(hex, "%02X ", data[i]);
            dataHex += hex;
        }
        if (data.size() > 64) {
            dataHex += "...";
        }
        _MESSAGE("NetworkClient::SendMessage - Data (hex): %s", dataHex.c_str());
    }

    // Send data
    _MESSAGE("NetworkClient::SendMessage - Sending %u bytes to socket", data.size());
    int bytesSent = send(m_socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);

    if (bytesSent == SOCKET_ERROR) {
        int error = WSAGetLastError();
        _MESSAGE("NetworkClient::SendMessage - Send error: %d", error);

        if (error != WSAEWOULDBLOCK) {
            _MESSAGE("NetworkClient::SendMessage - Fatal send error: %d", error);
            Disconnect();
            return false;
        }

        // Would block, try again later
        _MESSAGE("NetworkClient::SendMessage - Would block, try again later");
        return false;
    }

    _MESSAGE("NetworkClient::SendMessage - Successfully sent %d bytes", bytesSent);
    return true;
}

// Send a string message to the server
bool NetworkClient::SendMessage(MessageType type, const std::string& payload) {
    _MESSAGE("NetworkClient::SendMessage - Type: %d, Payload: %s", static_cast<int>(type), payload.c_str());

    if (!IsConnected()) {
        _MESSAGE("NetworkClient: Cannot send message, not connected");
        return false;
    }

    try {
        // Create a binary message with the string payload
        Message message(type, payload);

        // Send the binary message
        bool result = SendMessage(message);

        if (result) {
            _MESSAGE("NetworkClient: Message send successful");
        } else {
            _MESSAGE("NetworkClient: Message send failed");
        }

        return result;
    }
    catch (const std::exception& e) {
        _MESSAGE("NetworkClient: Exception during send: %s", e.what());
        return false;
    }
    catch (...) {
        _MESSAGE("NetworkClient: Unknown exception during send");
        return false;
    }
}

// Get the next received message
std::unique_ptr<Message> NetworkClient::GetNextMessage() {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_messageQueue.empty()) {
        return nullptr;
    }

    std::unique_ptr<Message> message = std::move(m_messageQueue.front());
    m_messageQueue.pop();
    return message;
}

// Set the server address
void NetworkClient::SetServerAddress(const std::string& address) {
    if (m_connected) {
        Disconnect();
    }
    m_serverAddress = address;
}

// Set the server port
void NetworkClient::SetServerPort(int port) {
    if (m_connected) {
        Disconnect();
    }
    m_serverPort = port;
}

// Set the message received callback
void NetworkClient::SetMessageReceivedCallback(MessageReceivedCallback callback) {
    m_messageCallback = callback;
}

// Set the connection status callback
void NetworkClient::SetConnectionStatusCallback(ConnectionStatusCallback callback) {
    m_connectionCallback = callback;
}

// Get the client version
std::array<int, 2> NetworkClient::GetClientVersion() const {
    return m_clientVersion;
}

// Set the client version
void NetworkClient::SetClientVersion(int major, int minor) {
    m_clientVersion[0] = major;
    m_clientVersion[1] = minor;
}

// Enable or disable debug mode
void NetworkClient::SetDebugMode(bool enable) {
    m_debugMode = enable;
    _MESSAGE("NetworkClient::SetDebugMode - Debug mode %s", enable ? "enabled" : "disabled");
}

// Process any pending messages
void NetworkClient::ProcessMessages() {
    // Check if we need to reconnect
    if (!m_connected) {
        TryReconnect();
        return;
    }

    // Process all available messages
    while (true) {
        std::unique_ptr<Message> message = GetNextMessage();
        if (!message) {
            break;
        }

        // Log message details
        _MESSAGE("NetworkClient::ProcessMessages - Processing message type %d, size %u bytes",
                 static_cast<int>(message->GetType()), message->GetPayloadSize());

        // Handle system messages
        if (message->GetType() == MessageType::HANDSHAKE_RESPONSE) {
            _MESSAGE("NetworkClient::ProcessMessages - Received handshake response");
            bool result = ProcessHandshakeResponse(*message);
            _MESSAGE("NetworkClient::ProcessMessages - Handshake processing result: %s",
                     result ? "success" : "failure");
            continue;
        }
        else if (message->GetType() == MessageType::DISCONNECT) {
            _MESSAGE("NetworkClient::ProcessMessages - Received disconnect message");
            Disconnect();
            break;
        }
        else if (message->GetType() == MessageType::HEARTBEAT) {
            _MESSAGE("NetworkClient::ProcessMessages - Received heartbeat, responding");
            // Respond to heartbeat
            SendMessage(MessageType::HEARTBEAT, "");
            continue;
        }

        // Notify message received
        if (m_messageCallback && m_handshakeCompleted) {
            _MESSAGE("NetworkClient::ProcessMessages - Forwarding message to callback");
            try {
                m_messageCallback(this, *message);
            }
            catch (const std::exception& e) {
                _MESSAGE("NetworkClient::ProcessMessages - Exception in message callback: %s", e.what());
            }
            catch (...) {
                _MESSAGE("NetworkClient::ProcessMessages - Unknown exception in message callback");
            }
        }
        else if (!m_handshakeCompleted) {
            _MESSAGE("NetworkClient::ProcessMessages - Ignoring message, handshake not completed");
        }
        else if (!m_messageCallback) {
            _MESSAGE("NetworkClient::ProcessMessages - Ignoring message, no callback registered");
        }
    }
}

// Receive thread function
void NetworkClient::ReceiveThreadFunction() {
    _MESSAGE("NetworkClient::ReceiveThreadFunction - Started");

    // Buffer for receiving data
    char buffer[BUFFER_SIZE];
    
    // Track consecutive errors to avoid log spam
    int consecutiveErrors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 10;

    // Set socket to non-blocking mode
    u_long mode = 1;
    if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
        _MESSAGE("NetworkClient::ReceiveThreadFunction - Failed to set socket to non-blocking mode");
    }

    // Receive loop
    while (m_threadRunning) {
        // Check if we're still connected
        if (!m_connected) {
            _MESSAGE("NetworkClient::ReceiveThreadFunction - Not connected, exiting thread");
            break;
        }

        // Try to receive data
        int bytesReceived = recv(m_socket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived > 0) {
            // Process received data
            try {
                // Add a small delay to prevent CPU hogging
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                // Log the raw received data
                std::string rawDataHex;
                for (int i = 0; i < bytesReceived && i < 64; ++i) {
                    char hex[8];
                    sprintf_s(hex, "%02X ", buffer[i]);
                    rawDataHex += hex;
                }
                if (bytesReceived > 64) {
                    rawDataHex += "...";
                }
                _MESSAGE("NetworkClient::ReceiveThreadFunction - Raw received data (hex): %s", rawDataHex.c_str());

                // Process received data
                if (!ProcessReceivedData(buffer, bytesReceived)) {
                    // Error processing data
                    _MESSAGE("NetworkClient::ReceiveThreadFunction - Error processing data");

                    // Don't break immediately, just clear the buffer and continue
                    std::memset(buffer, 0, BUFFER_SIZE);

                    // Add a small delay to prevent CPU spinning
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            catch (const std::exception& e) {
                _MESSAGE("NetworkClient::ReceiveThreadFunction - Exception processing received data: %s", e.what());
                std::memset(buffer, 0, BUFFER_SIZE);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            catch (...) {
                _MESSAGE("NetworkClient::ReceiveThreadFunction - Unknown exception processing received data");
                std::memset(buffer, 0, BUFFER_SIZE);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        else if (bytesReceived == 0) {
            // Connection closed
            _MESSAGE("NetworkClient::ReceiveThreadFunction - Connection closed by server (received 0 bytes)");
            break;
        }
        else {
            // Error or would block
            int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK) {
                // This is normal, just continue
                continue;
            }

            // Handle connection reset specifically
            if (error == WSAECONNRESET) {
                _MESSAGE("NetworkClient::ReceiveThreadFunction - Connection reset by server (error: %d)", error);
                // Don't break immediately, try to reconnect
                m_connected = false;

                // Try to reconnect immediately
                try {
                    _MESSAGE("NetworkClient::ReceiveThreadFunction - Attempting immediate reconnect");
                    // Close the socket first
                    if (m_socket != INVALID_SOCKET) {
                        closesocket(m_socket);
                        m_socket = INVALID_SOCKET;
                    }

                    // Create a new socket
                    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (m_socket != INVALID_SOCKET) {
                        // Set up server address
                        sockaddr_in serverAddr;
                        serverAddr.sin_family = AF_INET;
                        serverAddr.sin_port = htons(m_serverPort);
                        inet_pton(AF_INET, m_serverAddress.c_str(), &serverAddr.sin_addr);

                        // Get connection timeout from config
                        Config& config = Config::GetInstance();
                        int timeoutSeconds = config.GetInt("Network.ConnectionTimeout", 5);
                        DWORD timeout = timeoutSeconds * 1000; // Convert to milliseconds
                        _MESSAGE("NetworkClient::ReceiveThreadFunction - Using connection timeout of %d seconds (%d ms)", timeoutSeconds, timeout);
                        setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
                        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

                        // Connect to server
                        _MESSAGE("NetworkClient::ReceiveThreadFunction - Connecting to server with %d second timeout...", timeoutSeconds);
                        int result = connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr));
                        if (result != SOCKET_ERROR) {
                            _MESSAGE("NetworkClient::ReceiveThreadFunction - Reconnected successfully");
                            m_connected = true;
                            continue;
                        } else {
                            _MESSAGE("NetworkClient::ReceiveThreadFunction - Reconnect failed, error: %d", WSAGetLastError());
                            closesocket(m_socket);
                            m_socket = INVALID_SOCKET;
                        }
                    }
                }
                catch (const std::exception& e) {
                    _MESSAGE("NetworkClient::ReceiveThreadFunction - Exception during reconnect attempt: %s", e.what());
                }
                catch (...) {
                    _MESSAGE("NetworkClient::ReceiveThreadFunction - Unknown exception during reconnect attempt");
                }
            } else {
                // Only log if we haven't seen too many consecutive errors
                if (consecutiveErrors < MAX_CONSECUTIVE_ERRORS) {
                    _MESSAGE("NetworkClient::ReceiveThreadFunction - Error receiving data: %d", error);
                    consecutiveErrors++;
                }
            }

            // For any error other than WSAEWOULDBLOCK, break the loop
            break;
        }
    }

    _MESSAGE("NetworkClient::ReceiveThreadFunction - Exiting");

    // If we exited the loop due to an error and we're still connected, disconnect
    if (m_connected) {
        _MESSAGE("NetworkClient::ReceiveThreadFunction - Disconnecting due to error");
        // Use a flag to avoid recursive calls to Disconnect
        static bool disconnecting = false;
        if (!disconnecting) {
            disconnecting = true;
            Disconnect();
            disconnecting = false;
        }
    }
}

// Send handshake
bool NetworkClient::SendHandshake() {
    _MESSAGE("NetworkClient::SendHandshake - Starting handshake process");

    // Create handshake request
    HandshakeRequest request(m_clientVersion);
    std::vector<uint8_t> payload = request.Serialize();

    // Always log handshake details for troubleshooting
    _MESSAGE("NetworkClient::SendHandshake - Client version: %d.%d",
             m_clientVersion[0], m_clientVersion[1]);
    _MESSAGE("NetworkClient::SendHandshake - Payload size: %u bytes", payload.size());

    // Dump payload bytes for debugging
    std::string payloadHex;
    for (size_t i = 0; i < payload.size(); ++i) {
        char hex[8];
        sprintf_s(hex, "%02X ", payload[i]);
        payloadHex += hex;
    }
    _MESSAGE("NetworkClient::SendHandshake - Full payload (hex): %s", payloadHex.c_str());

    // Detailed analysis of the payload
    _MESSAGE("NetworkClient::SendHandshake - Payload analysis:");
    if (payload.size() >= 8) {
        int major, minor;
        std::memcpy(&major, payload.data(), sizeof(int));
        std::memcpy(&minor, payload.data() + sizeof(int), sizeof(int));
        _MESSAGE("  - Major version: %d (0x%08X)", major, major);
        _MESSAGE("  - Minor version: %d (0x%08X)", minor, minor);
    } else {
        _MESSAGE("  - Payload too small for version data!");
    }

    // Create message
    Message message(MessageType::HANDSHAKE_REQUEST, payload);

    // Log the full message
    std::vector<uint8_t> fullMessage = message.Serialize();
    std::string fullMessageHex;
    for (size_t i = 0; i < fullMessage.size(); ++i) {
        char hex[8];
        sprintf_s(hex, "%02X ", fullMessage[i]);
        fullMessageHex += hex;
    }
    _MESSAGE("NetworkClient::SendHandshake - Full message with header (hex): %s", fullMessageHex.c_str());

    // Analyze the message header
    if (fullMessage.size() >= sizeof(MessageHeader)) {
        const MessageHeader* header = reinterpret_cast<const MessageHeader*>(fullMessage.data());
        _MESSAGE("NetworkClient::SendHandshake - Message header analysis:");
        _MESSAGE("  - Message type: %d (0x%02X)", static_cast<int>(header->type), static_cast<int>(header->type));
        _MESSAGE("  - Payload size: %u (0x%08X)", header->payloadSize, header->payloadSize);
    }

    // Send message
    _MESSAGE("NetworkClient::SendHandshake - Sending handshake request to server");
    bool result = SendMessage(message);

    _MESSAGE("NetworkClient::SendHandshake - Send result: %s", result ? "success" : "failure");

    return result;
}

// Process handshake response
bool NetworkClient::ProcessHandshakeResponse(const Message& message) {
    try {
        // Verify message type
        if (message.GetType() != MessageType::HANDSHAKE_RESPONSE) {
            _MESSAGE("NetworkClient::ProcessHandshakeResponse - Unexpected message type: %d", static_cast<int>(message.GetType()));
            Disconnect();
            return false;
        }

        // Always dump payload bytes for debugging regardless of debug mode
        const std::vector<uint8_t>& payload = message.GetPayload();
        std::string payloadHex;
        for (size_t i = 0; i < payload.size() && i < 64; ++i) {
            char hex[8];
            sprintf_s(hex, "%02X ", payload[i]);
            payloadHex += hex;
        }
        if (payload.size() > 64) {
            payloadHex += "...";
        }
        _MESSAGE("NetworkClient::ProcessHandshakeResponse - Payload size: %u bytes", payload.size());
        _MESSAGE("NetworkClient::ProcessHandshakeResponse - Payload (hex): %s", payloadHex.c_str());

        // Check payload size
        if (payload.size() < sizeof(bool) + sizeof(uint32_t)) {
            _MESSAGE("NetworkClient::ProcessHandshakeResponse - Payload too small: %u bytes, minimum required: %u bytes",
                     payload.size(), sizeof(bool) + sizeof(uint32_t));

            // Try to parse the server's response even if it's malformed
            if (payload.size() > 0) {
                _MESSAGE("NetworkClient::ProcessHandshakeResponse - Attempting to extract partial data");

                // Try to extract at least the accepted flag if available
                if (payload.size() >= sizeof(bool)) {
                    bool accepted;
                    std::memcpy(&accepted, payload.data(), sizeof(bool));
                    _MESSAGE("NetworkClient::ProcessHandshakeResponse - Extracted accepted flag: %s",
                             accepted ? "true" : "false");

                    // If accepted, we'll consider the handshake successful despite the malformed response
                    if (accepted) {
                        _MESSAGE("NetworkClient::ProcessHandshakeResponse - Server accepted connection despite malformed response");
                        m_handshakeCompleted = true;
                        return true;
                    }
                }
            }

            Disconnect();
            return false;
        }

        try {
            // Deserialize handshake response
            HandshakeResponse response = HandshakeResponse::Deserialize(payload);

            // Check if handshake was accepted
            if (response.accepted) {
                m_handshakeCompleted = true;
                _MESSAGE("NetworkClient::ProcessHandshakeResponse - Handshake accepted: %s", response.message.c_str());
                return true;
            }
            else {
                _MESSAGE("NetworkClient::ProcessHandshakeResponse - Handshake rejected: %s", response.message.c_str());
                Disconnect();
                return false;
            }
        }
        catch (const std::exception& e) {
            _MESSAGE("NetworkClient::ProcessHandshakeResponse - Error deserializing handshake response: %s", e.what());

            // Try a more direct approach to extract the accepted flag
            if (payload.size() >= sizeof(bool)) {
                bool accepted;
                std::memcpy(&accepted, payload.data(), sizeof(bool));
                _MESSAGE("NetworkClient::ProcessHandshakeResponse - Direct extraction of accepted flag: %s",
                         accepted ? "true" : "false");

                // If accepted, we'll consider the handshake successful despite deserialization issues
                if (accepted) {
                    _MESSAGE("NetworkClient::ProcessHandshakeResponse - Server accepted connection (direct extraction)");
                    m_handshakeCompleted = true;
                    return true;
                }
            }

            Disconnect();
            return false;
        }
    }
    catch (const std::exception& e) {
        _MESSAGE("NetworkClient::ProcessHandshakeResponse - Error processing handshake response: %s", e.what());
        Disconnect();
        return false;
    }
    catch (...) {
        _MESSAGE("NetworkClient::ProcessHandshakeResponse - Unknown error processing handshake response");
        Disconnect();
        return false;
    }
}

// Process received data
bool NetworkClient::ProcessReceivedData(char* buffer, int bytesReceived) {
    try {
        // Convert buffer to vector for easier handling
        std::vector<uint8_t> data(buffer, buffer + bytesReceived);
        return ProcessReceivedData(data);
    }
    catch (const std::exception& e) {
        _MESSAGE("NetworkClient::ProcessReceivedData - Exception: %s", e.what());
        return false;
    }
    catch (...) {
        _MESSAGE("NetworkClient::ProcessReceivedData - Unknown exception");
        return false;
    }
}

// Process received data
bool NetworkClient::ProcessReceivedData(std::vector<uint8_t>& data) {
    size_t processedBytes = 0;

    // Log the received data
    _MESSAGE("NetworkClient::ProcessReceivedData - Processing %u bytes of data", data.size());

    // Dump the raw received data for debugging
    std::string dataHex;
    for (size_t i = 0; i < data.size() && i < 128; ++i) {
        char hex[8];
        sprintf_s(hex, "%02X ", data[i]);
        dataHex += hex;
    }
    if (data.size() > 128) {
        dataHex += "...";
    }
    _MESSAGE("NetworkClient::ProcessReceivedData - Raw data (hex): %s", dataHex.c_str());

    try {
        while (processedBytes < data.size()) {
            // Check if we have enough data for a header
            if (data.size() - processedBytes < sizeof(MessageHeader)) {
                _MESSAGE("NetworkClient::ProcessReceivedData - Not enough data for header, need %u bytes, have %u bytes",
                         sizeof(MessageHeader), data.size() - processedBytes);
                break;
            }

            // Get header
            const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data.data() + processedBytes);

            // Log header details
            _MESSAGE("NetworkClient::ProcessReceivedData - Message header: type=%d, payloadSize=%u",
                     static_cast<int>(header->type), header->payloadSize);

            // Validate message type to ensure it's within the valid range
            int messageType = static_cast<int>(header->type);
            if (messageType < 0 || messageType > static_cast<int>(MessageType::RESERVED)) {
                _MESSAGE("NetworkClient::ProcessReceivedData - Invalid message type: %d", messageType);

                // Dump the header bytes for debugging
                std::string headerHex;
                for (size_t i = 0; i < sizeof(MessageHeader) && i + processedBytes < data.size(); ++i) {
                    char hex[8];
                    sprintf_s(hex, "%02X ", data[processedBytes + i]);
                    headerHex += hex;
                }
                _MESSAGE("NetworkClient::ProcessReceivedData - Invalid header bytes: %s", headerHex.c_str());

                // Skip this header and try to find a valid one
                processedBytes += sizeof(MessageHeader);
                continue;
            }

            // Validate payload size to prevent excessive memory allocation
            if (header->payloadSize > 1024 * 1024) { // 1MB max payload size
                _MESSAGE("NetworkClient::ProcessReceivedData - Payload size too large: %u bytes", header->payloadSize);
                // Skip this header and try to find a valid one
                processedBytes += sizeof(MessageHeader);
                continue;
            }

            // Check if we have the full message
            size_t messageSize = sizeof(MessageHeader) + header->payloadSize;
            if (data.size() - processedBytes < messageSize) {
                _MESSAGE("NetworkClient::ProcessReceivedData - Incomplete message, need %u bytes, have %u bytes",
                         messageSize, data.size() - processedBytes);
                break;
            }

            // Log the full message
            _MESSAGE("NetworkClient::ProcessReceivedData - Full message size: %u bytes", messageSize);
            std::string messageHex;
            for (size_t i = 0; i < messageSize && i + processedBytes < data.size(); ++i) {
                char hex[8];
                sprintf_s(hex, "%02X ", data[processedBytes + i]);
                messageHex += hex;
            }
            _MESSAGE("NetworkClient::ProcessReceivedData - Full message (hex): %s", messageHex.c_str());

            try {
                // Deserialize message
                _MESSAGE("NetworkClient::ProcessReceivedData - Deserializing message");
                std::unique_ptr<Message> message = Message::Deserialize(data.data() + processedBytes, messageSize);

                // Log payload details
                const std::vector<uint8_t>& payload = message->GetPayload();
                _MESSAGE("NetworkClient::ProcessReceivedData - Deserialized message: type=%d, payloadSize=%u",
                         static_cast<int>(message->GetType()), payload.size());

                std::string payloadHex;
                for (size_t i = 0; i < payload.size() && i < 64; ++i) {
                    char hex[8];
                    sprintf_s(hex, "%02X ", payload[i]);
                    payloadHex += hex;
                }
                if (payload.size() > 64) {
                    payloadHex += "...";
                }
                _MESSAGE("NetworkClient::ProcessReceivedData - Payload (hex): %s", payloadHex.c_str());

                // Add message to queue
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    _MESSAGE("NetworkClient::ProcessReceivedData - Adding message to queue");
                    m_messageQueue.push(std::move(message));
                }

                _MESSAGE("NetworkClient::ProcessReceivedData - Successfully processed message type %d", static_cast<int>(header->type));

                // Update processed bytes
                processedBytes += messageSize;
            }
            catch (const std::exception& e) {
                _MESSAGE("NetworkClient::ProcessReceivedData - Error deserializing message: %s", e.what());

                // Dump the message bytes for debugging
                std::string errorMessageHex;
                for (size_t i = 0; i < messageSize && i + processedBytes < data.size(); ++i) {
                    char hex[8];
                    sprintf_s(hex, "%02X ", data[processedBytes + i]);
                    errorMessageHex += hex;
                }
                _MESSAGE("NetworkClient::ProcessReceivedData - Error message bytes: %s", errorMessageHex.c_str());

                // Skip this message and try to find the next one
                processedBytes += sizeof(MessageHeader);
            }
        }

        // Remove processed bytes from buffer
        if (processedBytes > 0) {
            _MESSAGE("NetworkClient::ProcessReceivedData - Removing %u processed bytes from buffer", processedBytes);
            data.erase(data.begin(), data.begin() + processedBytes);
            _MESSAGE("NetworkClient::ProcessReceivedData - Remaining buffer size: %u bytes", data.size());
        }

        return true;
    }
    catch (const std::exception& e) {
        _MESSAGE("NetworkClient::ProcessReceivedData - Exception: %s", e.what());
        return false;
    }
    catch (...) {
        _MESSAGE("NetworkClient::ProcessReceivedData - Unknown exception");
        return false;
    }
}

// Try to reconnect to the server
void NetworkClient::TryReconnect() {
    // Check if we're already connected
    if (m_connected) {
        _MESSAGE("NetworkClient::TryReconnect - Already connected");
        return;
    }

    // Check if we've exceeded the maximum number of reconnect attempts
    if (m_reconnectAttempts >= m_maxReconnectAttempts) {
        _MESSAGE("NetworkClient::TryReconnect - Maximum reconnect attempts (%d) exceeded",
                m_maxReconnectAttempts);
        return;
    }

    // Check if we need to wait before reconnecting
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - m_lastReconnectAttempt).count();

    if (elapsed < m_reconnectInterval) {
        // Not enough time has passed since the last attempt
        return;
    }

    // Try to reconnect
    m_lastReconnectAttempt = now;
    m_reconnectAttempts++;

    _MESSAGE("NetworkClient::TryReconnect - Attempting to reconnect to server (%d/%d)...",
            m_reconnectAttempts, m_maxReconnectAttempts);

    // Make sure we're fully disconnected before reconnecting
    if (m_connected || m_socket != INVALID_SOCKET) {
        _MESSAGE("NetworkClient::TryReconnect - Disconnecting before reconnect attempt");
        Disconnect();

        // Add a small delay to ensure resources are cleaned up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Double-check socket is closed
    if (m_socket != INVALID_SOCKET) {
        _MESSAGE("NetworkClient::TryReconnect - Socket still open, closing before reconnect");
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    // Reset connection state
    m_connected = false;
    m_handshakeCompleted = false;

    // Attempt to connect
    try {
        if (Connect()) {
            _MESSAGE("NetworkClient::TryReconnect - Reconnected to server successfully");
            // Reset reconnect attempts on successful connection
            m_reconnectAttempts = 0;
            // Connection status callback will handle the notification
        }
        else {
            _MESSAGE("NetworkClient::TryReconnect - Failed to reconnect to server");

            // If we failed to connect, make sure we're in a clean state
            if (m_socket != INVALID_SOCKET) {
                closesocket(m_socket);
                m_socket = INVALID_SOCKET;
            }
            m_connected = false;
            m_handshakeCompleted = false;
        }
    }
    catch (const std::exception& e) {
        _MESSAGE("NetworkClient::TryReconnect - Exception during reconnect: %s", e.what());

        // Ensure we're in a clean state
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_connected = false;
        m_handshakeCompleted = false;
    }
    catch (...) {
        _MESSAGE("NetworkClient::TryReconnect - Unknown exception during reconnect");

        // Ensure we're in a clean state
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_connected = false;
        m_handshakeCompleted = false;
    }
}
















