#include "NetworkClient.h"
#include "Config.h"
#include "QuestSyncLogging.h"
#include "Version.h"
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
      m_clientVersion(ClientVersion::Version),
      m_reconnectInterval(60),
      m_maxReconnectAttempts(5),
      m_reconnectAttempts(0),
      m_maxAttemptsWarningLogged(false),
      m_threadRunning(false) {
}

// Destructor
NetworkClient::~NetworkClient() {
    Cleanup();
}

// Initialize the network client
bool NetworkClient::Initialize() {
    if (m_initialized) {
        QUESTSYNC_LOG_DEBUG("NetworkClient already initialized");
        return true;
    }

    QUESTSYNC_LOG_INFO("Initializing NetworkClient");

    // Initialize Winsock
    WSADATA wsData;
    WORD ver = MAKEWORD(2, 2);
    int wsResult = WSAStartup(ver, &wsData);
    if (wsResult != 0) {
        QUESTSYNC_LOG_ERROR("Failed to initialize Winsock: %d", wsResult);
        return false;
    }

    // Load configuration
    Config& config = Config::GetInstance();
    m_reconnectInterval = config.GetInt("Network.ReconnectInterval", 60);
    m_maxReconnectAttempts = config.GetInt("Network.MaxReconnectAttempts", 5);

    QUESTSYNC_LOG_INFO("NetworkClient initialized successfully");
    m_initialized = true;
    return true;
}

// Connect to the server
bool NetworkClient::Connect() {
    QUESTSYNC_LOG_INFO("Attempting to connect to %s:%d", m_serverAddress.c_str(), m_serverPort);
    QUESTSYNC_LOG_INFO("Client version: %s", ClientVersion::GetVersionString().c_str());

    try {
        // Validate client version before attempting connection
        if (!ValidateClientVersion()) {
            QUESTSYNC_LOG_ERROR("Client version validation failed, aborting connection");
            return false;
        }

        // Check if already connected
        if (m_connected) {
            QUESTSYNC_LOG_INFO("Already connected");
            ResetReconnectCounter();  // Reset counter on successful connection
            return true;
        }

        // Initialize if not already initialized
        if (!m_initialized) {
            if (!Initialize()) {
                QUESTSYNC_LOG_ERROR("Failed to initialize");
                return false;
            }
        }

        // Log memory sizes for debugging
        QUESTSYNC_LOG_DEBUG("Size of bool: %u bytes", sizeof(bool));
        QUESTSYNC_LOG_DEBUG("Size of uint32_t: %u bytes", sizeof(uint32_t));
        QUESTSYNC_LOG_DEBUG("Size of MessageHeader: %u bytes", sizeof(MessageHeader));
        QUESTSYNC_LOG_DEBUG("Size of HandshakeRequest: %u bytes", sizeof(HandshakeRequest));
        QUESTSYNC_LOG_DEBUG("Size of HandshakeResponse: %u bytes", sizeof(HandshakeResponse));

        // Create socket
        QUESTSYNC_LOG_DEBUG("Creating socket");
        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) {
            int error = WSAGetLastError();
            QUESTSYNC_LOG_ERROR("Failed to create socket, error: %d (0x%08X)", error, error);
            return false;
        }
        QUESTSYNC_LOG_DEBUG("Socket created successfully: %d", m_socket);

        // Set up server address
        QUESTSYNC_LOG_DEBUG("Setting up server address");
        sockaddr_in serverAddr;
        ZeroMemory(&serverAddr, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(m_serverPort);

        int ipResult = inet_pton(AF_INET, m_serverAddress.c_str(), &serverAddr.sin_addr);
        if (ipResult != 1) {
            QUESTSYNC_LOG_ERROR("Failed to convert IP address, result: %d, error: %d",
                    ipResult, WSAGetLastError());
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &serverAddr.sin_addr, ipStr, INET_ADDRSTRLEN);
        QUESTSYNC_LOG_DEBUG("Server address set to %s:%d", ipStr, ntohs(serverAddr.sin_port));

        // Get connection timeout from config
        Config& config = Config::GetInstance();
        int timeoutSeconds = config.GetInt("Network.ConnectionTimeout", 5);
        DWORD timeout = timeoutSeconds * 1000; // Convert to milliseconds
        QUESTSYNC_LOG_DEBUG("Using connection timeout of %d seconds (%d ms)", timeoutSeconds, timeout);
        setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        // Connect to server
        QUESTSYNC_LOG_INFO("Connecting to server with %d second timeout...", timeoutSeconds);
        int result = connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr));
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            QUESTSYNC_LOG_ERROR("Failed to connect, error: %d (0x%08X)", error, error);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        QUESTSYNC_LOG_INFO("Connected successfully to %s:%d", ipStr, ntohs(serverAddr.sin_port));
        m_connected = true;

        // Set socket to non-blocking mode
        u_long mode = 1;  // 1 = non-blocking, 0 = blocking
        result = ioctlsocket(m_socket, FIONBIO, &mode);
        if (result == SOCKET_ERROR) {
            int error = WSAGetLastError();
            QUESTSYNC_LOG_WARNING("Failed to set socket to non-blocking mode, error: %d (0x%08X)",
                    error, error);
            // Continue anyway, but log the error
        } else {
            QUESTSYNC_LOG_DEBUG("Socket set to non-blocking mode");
        }

        // Start receive thread
        QUESTSYNC_LOG_DEBUG("Starting receive thread");
        m_threadRunning = true;

        // Start the receive thread
        QUESTSYNC_LOG_DEBUG("Creating receive thread");
        m_receiveThread = std::thread(&NetworkClient::ReceiveThreadFunction, this);
        QUESTSYNC_LOG_DEBUG("Receive thread created");

        // Send handshake request to authenticate with the server
        QUESTSYNC_LOG_INFO("Sending handshake request");
        m_handshakeCompleted = false;
        if (!SendHandshake()) {
            QUESTSYNC_LOG_ERROR("Failed to send handshake request");
            Disconnect();
            return false;
        }
        QUESTSYNC_LOG_DEBUG("Handshake request sent successfully");

        // Wait for handshake to complete (with timeout)
        QUESTSYNC_LOG_DEBUG("Waiting for handshake to complete");
        int handshakeAttempts = 0;
        const int maxHandshakeAttempts = 10; // Reduced timeout to fail faster if server is not responding

        while (!m_handshakeCompleted && handshakeAttempts < maxHandshakeAttempts) {
            QUESTSYNC_LOG_DEBUG("Handshake attempt %d/%d", handshakeAttempts + 1, maxHandshakeAttempts);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ProcessMessages(); // Process any pending messages, including handshake response
            handshakeAttempts++;
        }

        if (!m_handshakeCompleted) {
            QUESTSYNC_LOG_WARNING("Handshake timed out after %d attempts", handshakeAttempts);
            QUESTSYNC_LOG_WARNING("Server may not be running or may be incompatible");
            Disconnect();
            return false;
        }

        QUESTSYNC_LOG_INFO("Handshake completed successfully");
        return true;
    }
    catch (const std::exception& e) {
        QUESTSYNC_LOG_ERROR("Exception during connect: %s", e.what());

        // Clean up resources
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_connected = false;

        return false;
    }
    catch (...) {
        QUESTSYNC_LOG_ERROR("Unknown exception during connect");

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
    QUESTSYNC_LOG_INFO("Disconnecting from server");

    // Set connected flag to false first to prevent callbacks from being called multiple times
    bool wasConnected = m_connected.exchange(false);

    if (!wasConnected && m_socket == INVALID_SOCKET) {
        QUESTSYNC_LOG_DEBUG("Already disconnected");
        return;
    }

    QUESTSYNC_LOG_DEBUG("Stopping receive thread");
    // Stop receive thread
    m_threadRunning = false;

    // Clear message queue
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<std::unique_ptr<Message>> empty;
        std::swap(m_messageQueue, empty);
        QUESTSYNC_LOG_DEBUG("Message queue cleared");
    }

    // Close socket safely
    if (m_socket != INVALID_SOCKET) {
        QUESTSYNC_LOG_DEBUG("Closing socket");

        // Send disconnect message if possible
        try {
            Message disconnectMsg(MessageType::DISCONNECT);
            // Don't use SendMessage as it checks m_connected which we've already set to false
            std::vector<uint8_t> data = disconnectMsg.Serialize();
            int result = send(m_socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);
            if (result != SOCKET_ERROR) {
                QUESTSYNC_LOG_DEBUG("Sent disconnect message successfully");
            } else {
                QUESTSYNC_LOG_DEBUG("Failed to send disconnect message, error: %d", WSAGetLastError());
            }
        }
        catch (const std::exception& e) {
            QUESTSYNC_LOG_DEBUG("Error sending disconnect message: %s", e.what());
            // Ignore errors during disconnect
        }
        catch (...) {
            QUESTSYNC_LOG_DEBUG("Unknown error sending disconnect message");
            // Ignore errors during disconnect
        }

        // Shutdown the socket before closing it
        try {
            int result = shutdown(m_socket, SD_BOTH);  // Shutdown both send and receive operations
            if (result == SOCKET_ERROR) {
                QUESTSYNC_LOG_DEBUG("Socket shutdown failed, error: %d", WSAGetLastError());
            }
        }
        catch (const std::exception& e) {
            QUESTSYNC_LOG_DEBUG("Exception during socket shutdown: %s", e.what());
        }
        catch (...) {
            QUESTSYNC_LOG_DEBUG("Unknown exception during socket shutdown");
        }

        // Close the socket
        int result = closesocket(m_socket);
        if (result == SOCKET_ERROR) {
            QUESTSYNC_LOG_WARNING("Socket close failed, error: %d", WSAGetLastError());
        }

        m_socket = INVALID_SOCKET;
    }

    m_handshakeCompleted = false;
    QUESTSYNC_LOG_INFO("NetworkClient::Disconnect - Connection status set to disconnected");

    // Wait for receive thread to finish with a timeout
    if (m_receiveThread.joinable()) {
        try {
            QUESTSYNC_LOG_DEBUG("NetworkClient::Disconnect - Waiting for receive thread to finish");

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
                QUESTSYNC_LOG_WARNING("NetworkClient::Disconnect - Thread join timeout, detaching thread");

                // Detach the thread to prevent crashes
                try {
                    m_receiveThread.detach();
                    QUESTSYNC_LOG_INFO("NetworkClient::Disconnect - Thread detached successfully");
                }
                catch (const std::exception& e) {
                    QUESTSYNC_LOG_ERROR("NetworkClient::Disconnect - Exception during thread detach: %s", e.what());
                }
                catch (...) {
                    QUESTSYNC_LOG_ERROR("NetworkClient::Disconnect - Unknown exception during thread detach");
                }
            } else {
                QUESTSYNC_LOG_DEBUG("NetworkClient::Disconnect - Thread joined successfully");
            }
        }
        catch (const std::exception& e) {
            QUESTSYNC_LOG_ERROR("NetworkClient::Disconnect - Exception during thread join: %s", e.what());

            // If there's an exception, detach the thread
            if (m_receiveThread.joinable()) {
                try {
                    m_receiveThread.detach();
                    QUESTSYNC_LOG_INFO("NetworkClient::Disconnect - Thread detached after exception");
                }
                catch (...) {
                    QUESTSYNC_LOG_ERROR("NetworkClient::Disconnect - Failed to detach thread after exception");
                }
            }
        }
        catch (...) {
            QUESTSYNC_LOG_ERROR("NetworkClient::Disconnect - Unknown exception during thread join");

            // If there's an exception, detach the thread
            if (m_receiveThread.joinable()) {
                try {
                    m_receiveThread.detach();
                    QUESTSYNC_LOG_INFO("NetworkClient::Disconnect - Thread detached after unknown exception");
                }
                catch (...) {
                    QUESTSYNC_LOG_ERROR("NetworkClient::Disconnect - Failed to detach thread after unknown exception");
                }
            }
        }
    }

    // Reset reconnect attempts to ensure we can reconnect later
    m_reconnectAttempts = 0;

    // Notify connection status
    if (m_connectionCallback && wasConnected) {
        try {
            QUESTSYNC_LOG_DEBUG("NetworkClient::Disconnect - Calling connection status callback");
            m_connectionCallback(this, false);
        }
        catch (const std::exception& e) {
            QUESTSYNC_LOG_ERROR("NetworkClient::Disconnect - Exception in connection callback: %s", e.what());
        }
        catch (...) {
            QUESTSYNC_LOG_ERROR("NetworkClient::Disconnect - Unknown exception in connection callback");
        }
    }

    QUESTSYNC_LOG_INFO("NetworkClient::Disconnect - Disconnection complete");

    // Reset reconnect counter on manual disconnect
    ResetReconnectCounter();
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
    // Only log occasionally to avoid filling the log file, and only in debug mode
    static int logCounter = 0;
    if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG && logCounter++ % 1000 == 0) {
        QUESTSYNC_LOG_DEBUG("Connection status: %s", m_connected ? "connected" : "not connected");
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
        // Always log connection errors
        QUESTSYNC_LOG_WARNING("Not connected, cannot send message");
        return false;
    }

    // Serialize message
    std::vector<uint8_t> data = message.Serialize();

    // Log message details only in debug mode
    QUESTSYNC_LOG_DEBUG("Sending message type %d, size %u bytes",
             static_cast<int>(message.GetType()), data.size());

    // Dump message bytes for debugging (only in debug mode)
    if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
        std::string dataHex;
        for (size_t i = 0; i < data.size() && i < 64; ++i) {
            char hex[8];
            sprintf_s(hex, "%02X ", data[i]);
            dataHex += hex;
        }
        if (data.size() > 64) {
            dataHex += "...";
        }
        QUESTSYNC_LOG_DEBUG("Data (hex): %s", dataHex.c_str());
        QUESTSYNC_LOG_DEBUG("Sending %u bytes to socket", data.size());
    }

    // Send data
    int bytesSent = send(m_socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);

    if (bytesSent == SOCKET_ERROR) {
        int error = WSAGetLastError();
        // Always log errors
        QUESTSYNC_LOG_ERROR("Send error: %d", error);

        if (error != WSAEWOULDBLOCK) {
            QUESTSYNC_LOG_ERROR("Fatal send error: %d", error);
            Disconnect();
            return false;
        }

        // Would block, try again later
        QUESTSYNC_LOG_DEBUG("Would block, try again later");
        return false;
    }

    QUESTSYNC_LOG_DEBUG("Successfully sent %d bytes", bytesSent);
    return true;
}

// Send a string message to the server
bool NetworkClient::SendMessage(MessageType type, const std::string& payload) {
    QUESTSYNC_LOG_DEBUG("Sending message - Type: %d, Payload: %s", static_cast<int>(type), payload.c_str());

    if (!IsConnected()) {
        // Always log connection errors
        QUESTSYNC_LOG_WARNING("Cannot send message, not connected");
        return false;
    }

    try {
        // Create a binary message with the string payload
        Message message(type, payload);

        // Send the binary message
        bool result = SendMessage(message);

        if (result) {
            QUESTSYNC_LOG_DEBUG("Message send successful");
        } else {
            QUESTSYNC_LOG_WARNING("Message send failed");
        }

        return result;
    }
    catch (const std::exception& e) {
        // Always log errors
        QUESTSYNC_LOG_ERROR("Exception during send: %s", e.what());
        return false;
    }
    catch (...) {
        // Always log errors
        QUESTSYNC_LOG_ERROR("Unknown exception during send");
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

// Validate client version for basic sanity checks
bool NetworkClient::ValidateClientVersion() const {
    // Basic sanity checks for version numbers
    if (m_clientVersion[0] < 0 || m_clientVersion[1] < 0) {
        QUESTSYNC_LOG_ERROR("Invalid client version: negative numbers not allowed (%d.%d)",
                           m_clientVersion[0], m_clientVersion[1]);
        return false;
    }

    if (m_clientVersion[0] > 99 || m_clientVersion[1] > 999) {
        QUESTSYNC_LOG_ERROR("Invalid client version: numbers too large (%d.%d)",
                           m_clientVersion[0], m_clientVersion[1]);
        return false;
    }

    QUESTSYNC_LOG_DEBUG("Client version validation passed: %s", ClientVersion::GetVersionString().c_str());
    return true;
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

        // Log message details only in debug mode
        QUESTSYNC_LOG_DEBUG("Processing message type %d, size %u bytes",
                 static_cast<int>(message->GetType()), message->GetPayloadSize());

        // Handle system messages
        if (message->GetType() == MessageType::HANDSHAKE_RESPONSE) {
            // Always log important connection events
            QUESTSYNC_LOG_INFO("Received handshake response");
            bool result = ProcessHandshakeResponse(*message);
            QUESTSYNC_LOG_INFO("Handshake processing result: %s",
                     result ? "success" : "failure");
            continue;
        }
        else if (message->GetType() == MessageType::DISCONNECT) {
            // Always log important connection events
            QUESTSYNC_LOG_INFO("Received disconnect message");
            Disconnect();
            break;
        }
        else if (message->GetType() == MessageType::HEARTBEAT) {
            // Log heartbeats only in debug mode
            QUESTSYNC_LOG_DEBUG("Received heartbeat, responding");
            // Respond to heartbeat
            SendMessage(MessageType::HEARTBEAT, "");
            continue;
        }

        // Notify message received
        if (m_messageCallback && m_handshakeCompleted) {
            QUESTSYNC_LOG_DEBUG("Forwarding message to callback");
            try {
                m_messageCallback(this, *message);
            }
            catch (const std::exception& e) {
                // Always log errors
                QUESTSYNC_LOG_ERROR("Exception in message callback: %s", e.what());
            }
            catch (...) {
                // Always log errors
                QUESTSYNC_LOG_ERROR("Unknown exception in message callback");
            }
        }
        else if (!m_handshakeCompleted) {
            QUESTSYNC_LOG_DEBUG("Ignoring message, handshake not completed");
        }
        else if (!m_messageCallback) {
            QUESTSYNC_LOG_DEBUG("Ignoring message, no callback registered");
        }
    }
}

// Receive thread function
void NetworkClient::ReceiveThreadFunction() {
    QUESTSYNC_LOG_INFO("NetworkClient::ReceiveThreadFunction - Started");

    // Buffer for receiving data
    char buffer[BUFFER_SIZE];

    // Track consecutive errors to avoid log spam
    int consecutiveErrors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 10;

    // Set socket to non-blocking mode
    u_long mode = 1;
    if (ioctlsocket(m_socket, FIONBIO, &mode) != 0) {
        QUESTSYNC_LOG_ERROR("NetworkClient::ReceiveThreadFunction - Failed to set socket to non-blocking mode");
    }

    // Receive loop
    while (m_threadRunning) {
        // Check if we're still connected
        if (!m_connected) {
            QUESTSYNC_LOG_DEBUG("NetworkClient::ReceiveThreadFunction - Not connected, exiting thread");
            break;
        }

        // Try to receive data
        int bytesReceived = recv(m_socket, buffer, BUFFER_SIZE, 0);

        if (bytesReceived > 0) {
            // Process received data
            try {
                // Add a small delay to prevent CPU hogging
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                // Log the raw received data only in debug mode
                if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                    std::string rawDataHex;
                    for (int i = 0; i < bytesReceived && i < 64; ++i) {
                        char hex[8];
                        sprintf_s(hex, "%02X ", buffer[i]);
                        rawDataHex += hex;
                    }
                    if (bytesReceived > 64) {
                        rawDataHex += "...";
                    }
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ReceiveThreadFunction - Raw received data (hex): %s", rawDataHex.c_str());
                }

                // Process received data
                if (!ProcessReceivedData(buffer, bytesReceived)) {
                    // Error processing data
                    QUESTSYNC_LOG_WARNING("NetworkClient::ReceiveThreadFunction - Error processing data");

                    // Don't break immediately, just clear the buffer and continue
                    std::memset(buffer, 0, BUFFER_SIZE);

                    // Add a small delay to prevent CPU spinning
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            catch (const std::exception& e) {
                QUESTSYNC_LOG_ERROR("NetworkClient::ReceiveThreadFunction - Exception processing received data: %s", e.what());
                std::memset(buffer, 0, BUFFER_SIZE);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            catch (...) {
                QUESTSYNC_LOG_ERROR("NetworkClient::ReceiveThreadFunction - Unknown exception processing received data");
                std::memset(buffer, 0, BUFFER_SIZE);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        else if (bytesReceived == 0) {
            // Connection closed
            QUESTSYNC_LOG_INFO("NetworkClient::ReceiveThreadFunction - Connection closed by server (received 0 bytes)");
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
                QUESTSYNC_LOG_WARNING("NetworkClient::ReceiveThreadFunction - Connection reset by server (error: %d)", error);
                // Don't break immediately, try to reconnect
                m_connected = false;

                // Try to reconnect immediately
                try {
                    QUESTSYNC_LOG_INFO("NetworkClient::ReceiveThreadFunction - Attempting immediate reconnect");
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
                        QUESTSYNC_LOG_DEBUG("NetworkClient::ReceiveThreadFunction - Using connection timeout of %d seconds (%d ms)", timeoutSeconds, timeout);
                        setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
                        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

                        // Connect to server
                        QUESTSYNC_LOG_INFO("NetworkClient::ReceiveThreadFunction - Connecting to server with %d second timeout...", timeoutSeconds);
                        int result = connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr));
                        if (result != SOCKET_ERROR) {
                            QUESTSYNC_LOG_INFO("NetworkClient::ReceiveThreadFunction - Reconnected successfully");
                            m_connected = true;
                            continue;
                        } else {
                            QUESTSYNC_LOG_WARNING("NetworkClient::ReceiveThreadFunction - Reconnect failed, error: %d", WSAGetLastError());
                            closesocket(m_socket);
                            m_socket = INVALID_SOCKET;
                        }
                    }
                }
                catch (const std::exception& e) {
                    QUESTSYNC_LOG_ERROR("NetworkClient::ReceiveThreadFunction - Exception during reconnect attempt: %s", e.what());
                }
                catch (...) {
                    QUESTSYNC_LOG_ERROR("NetworkClient::ReceiveThreadFunction - Unknown exception during reconnect attempt");
                }
            } else {
                // Only log if we haven't seen too many consecutive errors
                if (consecutiveErrors < MAX_CONSECUTIVE_ERRORS) {
                    QUESTSYNC_LOG_ERROR("NetworkClient::ReceiveThreadFunction - Error receiving data: %d", error);
                    consecutiveErrors++;
                }
            }

            // For any error other than WSAEWOULDBLOCK, break the loop
            break;
        }
    }

    QUESTSYNC_LOG_INFO("NetworkClient::ReceiveThreadFunction - Exiting");

    // If we exited the loop due to an error and we're still connected, disconnect
    if (m_connected) {
        QUESTSYNC_LOG_INFO("NetworkClient::ReceiveThreadFunction - Disconnecting due to error");
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
    QUESTSYNC_LOG_INFO("Starting handshake process");

    // Create handshake request
    HandshakeRequest request(m_clientVersion);
    std::vector<uint8_t> payload = request.Serialize();

    // Always log handshake details for troubleshooting
    QUESTSYNC_LOG_INFO("Client version: %s", ClientVersion::GetVersionString().c_str());
    QUESTSYNC_LOG_DEBUG("Payload size: %u bytes", payload.size());

    // Dump payload bytes for debugging
    std::string payloadHex;
    for (size_t i = 0; i < payload.size(); ++i) {
        char hex[8];
        sprintf_s(hex, "%02X ", payload[i]);
        payloadHex += hex;
    }
    QUESTSYNC_LOG_DEBUG("Full payload (hex): %s", payloadHex.c_str());

    // Detailed analysis of the payload
    QUESTSYNC_LOG_DEBUG("Payload analysis:");
    if (payload.size() >= 8) {
        int major, minor;
        std::memcpy(&major, payload.data(), sizeof(int));
        std::memcpy(&minor, payload.data() + sizeof(int), sizeof(int));
        QUESTSYNC_LOG_DEBUG("  - Major version: %d (0x%08X)", major, major);
        QUESTSYNC_LOG_DEBUG("  - Minor version: %d (0x%08X)", minor, minor);
    } else {
        QUESTSYNC_LOG_WARNING("  - Payload too small for version data!");
    }

    // Create message
    Message message(MessageType::HANDSHAKE_REQUEST, payload);

    // Log the full message (only in debug mode)
    if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
        std::vector<uint8_t> fullMessage = message.Serialize();
        std::string fullMessageHex;
        for (size_t i = 0; i < fullMessage.size(); ++i) {
            char hex[8];
            sprintf_s(hex, "%02X ", fullMessage[i]);
            fullMessageHex += hex;
        }
        QUESTSYNC_LOG_DEBUG("Full message with header (hex): %s", fullMessageHex.c_str());

        // Analyze the message header
        if (fullMessage.size() >= sizeof(MessageHeader)) {
            const MessageHeader* header = reinterpret_cast<const MessageHeader*>(fullMessage.data());
            QUESTSYNC_LOG_DEBUG("Message header analysis:");
            QUESTSYNC_LOG_DEBUG("  - Message type: %d (0x%02X)", static_cast<int>(header->type), static_cast<int>(header->type));
            QUESTSYNC_LOG_DEBUG("  - Payload size: %u (0x%08X)", header->payloadSize, header->payloadSize);
        }
    }

    // Send message
    QUESTSYNC_LOG_DEBUG("Sending handshake request to server");
    bool result = SendMessage(message);

    QUESTSYNC_LOG_INFO("Handshake send result: %s", result ? "success" : "failure");

    return result;
}

// Process handshake response
bool NetworkClient::ProcessHandshakeResponse(const Message& message) {
    try {
        // Verify message type
        if (message.GetType() != MessageType::HANDSHAKE_RESPONSE) {
            QUESTSYNC_LOG_ERROR("NetworkClient::ProcessHandshakeResponse - Unexpected message type: %d", static_cast<int>(message.GetType()));
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
        QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessHandshakeResponse - Payload size: %u bytes", payload.size());
        QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessHandshakeResponse - Payload (hex): %s", payloadHex.c_str());

        // Check payload size
        if (payload.size() < sizeof(bool) + sizeof(uint32_t)) {
            QUESTSYNC_LOG_WARNING("NetworkClient::ProcessHandshakeResponse - Payload too small: %u bytes, minimum required: %u bytes",
                     payload.size(), sizeof(bool) + sizeof(uint32_t));

            // Try to parse the server's response even if it's malformed
            if (payload.size() > 0) {
                QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessHandshakeResponse - Attempting to extract partial data");

                // Try to extract at least the accepted flag if available
                if (payload.size() >= sizeof(bool)) {
                    bool accepted;
                    std::memcpy(&accepted, payload.data(), sizeof(bool));
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessHandshakeResponse - Extracted accepted flag: %s",
                             accepted ? "true" : "false");

                    // If accepted, we'll consider the handshake successful despite the malformed response
                    if (accepted) {
                        QUESTSYNC_LOG_INFO("NetworkClient::ProcessHandshakeResponse - Server accepted connection despite malformed response");
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
                QUESTSYNC_LOG_INFO("NetworkClient::ProcessHandshakeResponse - Handshake accepted: %s", response.message.c_str());
                return true;
            }
            else {
                QUESTSYNC_LOG_WARNING("NetworkClient::ProcessHandshakeResponse - Handshake rejected: %s", response.message.c_str());
                Disconnect();
                return false;
            }
        }
        catch (const std::exception& e) {
            QUESTSYNC_LOG_ERROR("NetworkClient::ProcessHandshakeResponse - Error deserializing handshake response: %s", e.what());

            // Try a more direct approach to extract the accepted flag
            if (payload.size() >= sizeof(bool)) {
                bool accepted;
                std::memcpy(&accepted, payload.data(), sizeof(bool));
                QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessHandshakeResponse - Direct extraction of accepted flag: %s",
                         accepted ? "true" : "false");

                // If accepted, we'll consider the handshake successful despite deserialization issues
                if (accepted) {
                    QUESTSYNC_LOG_INFO("NetworkClient::ProcessHandshakeResponse - Server accepted connection (direct extraction)");
                    m_handshakeCompleted = true;
                    return true;
                }
            }

            Disconnect();
            return false;
        }
    }
    catch (const std::exception& e) {
        QUESTSYNC_LOG_ERROR("NetworkClient::ProcessHandshakeResponse - Error processing handshake response: %s", e.what());
        Disconnect();
        return false;
    }
    catch (...) {
        QUESTSYNC_LOG_ERROR("NetworkClient::ProcessHandshakeResponse - Unknown error processing handshake response");
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
        QUESTSYNC_LOG_ERROR("NetworkClient::ProcessReceivedData - Exception: %s", e.what());
        return false;
    }
    catch (...) {
        QUESTSYNC_LOG_ERROR("NetworkClient::ProcessReceivedData - Unknown exception");
        return false;
    }
}

// Process received data
bool NetworkClient::ProcessReceivedData(std::vector<uint8_t>& data) {
    size_t processedBytes = 0;

    // Log the received data only in debug mode
    if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
        QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Processing %u bytes of data", data.size());

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
        QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Raw data (hex): %s", dataHex.c_str());
    }

    try {
        while (processedBytes < data.size()) {
            // Check if we have enough data for a header
            if (data.size() - processedBytes < sizeof(MessageHeader)) {
                if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Not enough data for header, need %u bytes, have %u bytes",
                             sizeof(MessageHeader), data.size() - processedBytes);
                }
                break;
            }

            // Get header
            const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data.data() + processedBytes);

            // Log header details only in debug mode
            if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Message header: type=%d, payloadSize=%u",
                         static_cast<int>(header->type), header->payloadSize);
            }

            // Validate message type to ensure it's within the valid range
            int messageType = static_cast<int>(header->type);
            if (messageType < 0 || messageType > static_cast<int>(MessageType::RESERVED)) {
                // Always log validation errors
                QUESTSYNC_LOG_ERROR("NetworkClient::ProcessReceivedData - Invalid message type: %d", messageType);

                if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                    // Dump the header bytes for debugging only in debug mode
                    std::string headerHex;
                    for (size_t i = 0; i < sizeof(MessageHeader) && i + processedBytes < data.size(); ++i) {
                        char hex[8];
                        sprintf_s(hex, "%02X ", data[processedBytes + i]);
                        headerHex += hex;
                    }
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Invalid header bytes: %s", headerHex.c_str());
                }

                // Skip this header and try to find a valid one
                processedBytes += sizeof(MessageHeader);
                continue;
            }

            // Validate payload size to prevent excessive memory allocation
            if (header->payloadSize > 1024 * 1024) { // 1MB max payload size
                // Always log validation errors
                QUESTSYNC_LOG_ERROR("NetworkClient::ProcessReceivedData - Payload size too large: %u bytes", header->payloadSize);
                // Skip this header and try to find a valid one
                processedBytes += sizeof(MessageHeader);
                continue;
            }

            // Check if we have the full message
            size_t messageSize = sizeof(MessageHeader) + header->payloadSize;
            if (data.size() - processedBytes < messageSize) {
                if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Incomplete message, need %u bytes, have %u bytes",
                             messageSize, data.size() - processedBytes);
                }
                break;
            }

            // Log the full message only in debug mode
            if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Full message size: %u bytes", messageSize);
                std::string messageHex;
                for (size_t i = 0; i < messageSize && i + processedBytes < data.size(); ++i) {
                    char hex[8];
                    sprintf_s(hex, "%02X ", data[processedBytes + i]);
                    messageHex += hex;
                }
                QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Full message (hex): %s", messageHex.c_str());
            }

            try {
                // Deserialize message
                if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Deserializing message");
                }
                std::unique_ptr<Message> message = Message::Deserialize(data.data() + processedBytes, messageSize);

                // Log payload details only in debug mode
                if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                    const std::vector<uint8_t>& payload = message->GetPayload();
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Deserialized message: type=%d, payloadSize=%u",
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
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Payload (hex): %s", payloadHex.c_str());
                }

                // Add message to queue
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                        QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Adding message to queue");
                    }
                    m_messageQueue.push(std::move(message));
                }

                if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Successfully processed message type %d", static_cast<int>(header->type));
                }

                // Update processed bytes
                processedBytes += messageSize;
            }
            catch (const std::exception& e) {
                // Always log deserialization errors
                QUESTSYNC_LOG_ERROR("NetworkClient::ProcessReceivedData - Error deserializing message: %s", e.what());

                // Dump the message bytes for debugging only in debug mode
                if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                    std::string errorMessageHex;
                    for (size_t i = 0; i < messageSize && i + processedBytes < data.size(); ++i) {
                        char hex[8];
                        sprintf_s(hex, "%02X ", data[processedBytes + i]);
                        errorMessageHex += hex;
                    }
                    QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Error message bytes: %s", errorMessageHex.c_str());
                }

                // Skip this message and try to find the next one
                processedBytes += sizeof(MessageHeader);
            }
        }

        // Remove processed bytes from buffer
        if (processedBytes > 0) {
            if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Removing %u processed bytes from buffer", processedBytes);
            }
            data.erase(data.begin(), data.begin() + processedBytes);
            if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG) {
                QUESTSYNC_LOG_DEBUG("NetworkClient::ProcessReceivedData - Remaining buffer size: %u bytes", data.size());
            }
        }

        return true;
    }
    catch (const std::exception& e) {
        QUESTSYNC_LOG_ERROR("NetworkClient::ProcessReceivedData - Exception: %s", e.what());
        return false;
    }
    catch (...) {
        QUESTSYNC_LOG_ERROR("NetworkClient::ProcessReceivedData - Unknown exception");
        return false;
    }
}

// Try to reconnect to the server
void NetworkClient::TryReconnect() {
    // Check if we're already connected
    if (m_connected) {
        QUESTSYNC_LOG_DEBUG("NetworkClient::TryReconnect - Already connected");
        return;
    }

    // Check if we've exceeded the maximum number of reconnect attempts
    if (m_reconnectAttempts >= m_maxReconnectAttempts) {
        // Only log the warning once to prevent log spam
        if (!m_maxAttemptsWarningLogged) {
            QUESTSYNC_LOG_WARNING("NetworkClient::TryReconnect - Maximum reconnect attempts (%d) exceeded",
                    m_maxReconnectAttempts);
            m_maxAttemptsWarningLogged = true;
        }
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

    QUESTSYNC_LOG_INFO("NetworkClient::TryReconnect - Attempting to reconnect to server (%d/%d)...",
            m_reconnectAttempts, m_maxReconnectAttempts);

    // Make sure we're fully disconnected before reconnecting
    if (m_connected || m_socket != INVALID_SOCKET) {
        QUESTSYNC_LOG_DEBUG("NetworkClient::TryReconnect - Disconnecting before reconnect attempt");
        Disconnect();

        // Add a small delay to ensure resources are cleaned up
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Double-check socket is closed
    if (m_socket != INVALID_SOCKET) {
        QUESTSYNC_LOG_DEBUG("NetworkClient::TryReconnect - Socket still open, closing before reconnect");
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    // Reset connection state
    m_connected = false;
    m_handshakeCompleted = false;

    // Attempt to connect
    try {
        if (Connect()) {
            QUESTSYNC_LOG_INFO("NetworkClient::TryReconnect - Reconnected to server successfully");
            // Reset reconnect attempts on successful connection
            m_reconnectAttempts = 0;
            m_maxAttemptsWarningLogged = false;
            // Connection status callback will handle the notification
        }
        else {
            QUESTSYNC_LOG_WARNING("NetworkClient::TryReconnect - Failed to reconnect to server");

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
        QUESTSYNC_LOG_ERROR("NetworkClient::TryReconnect - Exception during reconnect: %s", e.what());

        // Ensure we're in a clean state
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_connected = false;
        m_handshakeCompleted = false;
    }
    catch (...) {
        QUESTSYNC_LOG_ERROR("NetworkClient::TryReconnect - Unknown exception during reconnect");

        // Ensure we're in a clean state
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        m_connected = false;
        m_handshakeCompleted = false;
    }
}

void NetworkClient::ResetReconnectCounter() {
    m_reconnectAttempts = 0;
    m_maxAttemptsWarningLogged = false;
}






