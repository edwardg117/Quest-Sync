#include "NetworkClient.h"
#include "Config.h"
#include <iostream>
#include <chrono>

// Constructor
NetworkClient::NetworkClient(const std::string& serverAddress, int serverPort)
    : m_serverAddress(serverAddress),
      m_serverPort(serverPort),
      m_socket(INVALID_SOCKET),
      m_connected(false),
      m_initialized(false),
      m_handshakeCompleted(false),
      m_clientVersion{1, 0},
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
    if (!m_initialized && !Initialize()) {
        return false;
    }

    if (m_connected) {
        return true;
    }

    // Create socket
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
        return false;
    }

    // Set up server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_serverPort);
    
    // Convert IP address string to binary
    if (inet_pton(AF_INET, m_serverAddress.c_str(), &serverAddr.sin_addr) != 1) {
        std::cerr << "Invalid IP address: " << m_serverAddress << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    // Connect to server
    int connectResult = connect(m_socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (connectResult == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server: " << WSAGetLastError() << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    // Set socket to non-blocking mode
    u_long mode = 1;
    if (ioctlsocket(m_socket, FIONBIO, &mode) == SOCKET_ERROR) {
        std::cerr << "Failed to set socket to non-blocking mode: " << WSAGetLastError() << std::endl;
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    // Connection successful
    m_connected = true;
    m_reconnectAttempts = 0;
    
    // Start receive thread
    m_threadRunning = true;
    m_receiveThread = std::thread(&NetworkClient::ReceiveThreadFunction, this);
    m_receiveThread.detach();

    // Send handshake
    if (!SendHandshake()) {
        std::cerr << "Failed to send handshake" << std::endl;
        Disconnect();
        return false;
    }

    // Notify connection status
    if (m_connectionCallback) {
        m_connectionCallback(this, true);
    }

    return true;
}

// Disconnect from the server
void NetworkClient::Disconnect() {
    if (!m_connected) {
        return;
    }

    // Stop receive thread
    m_threadRunning = false;

    // Close socket
    if (m_socket != INVALID_SOCKET) {
        // Send disconnect message if possible
        try {
            Message disconnectMsg(MessageType::DISCONNECT);
            SendMessage(disconnectMsg);
        }
        catch (...) {
            // Ignore errors during disconnect
        }

        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }

    m_connected = false;
    m_handshakeCompleted = false;

    // Notify connection status
    if (m_connectionCallback) {
        m_connectionCallback(this, false);
    }
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
    return m_connected && m_handshakeCompleted;
}

// Send a message to the server
bool NetworkClient::SendMessage(const Message& message) {
    if (!m_connected) {
        return false;
    }

    // Serialize message
    std::vector<uint8_t> data = message.Serialize();
    
    // Send data
    int bytesSent = send(m_socket, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0);
    if (bytesSent == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            std::cerr << "Failed to send message: " << error << std::endl;
            Disconnect();
            return false;
        }
        // Would block, try again later
        return false;
    }

    return true;
}

// Send a string message to the server
bool NetworkClient::SendMessage(MessageType type, const std::string& payload) {
    Message message(type, payload);
    return SendMessage(message);
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

        // Handle system messages
        if (message->GetType() == MessageType::HANDSHAKE_RESPONSE) {
            ProcessHandshakeResponse(*message);
            continue;
        }
        else if (message->GetType() == MessageType::DISCONNECT) {
            Disconnect();
            break;
        }
        else if (message->GetType() == MessageType::HEARTBEAT) {
            // Respond to heartbeat
            SendMessage(MessageType::HEARTBEAT, "");
            continue;
        }

        // Notify message received
        if (m_messageCallback && m_handshakeCompleted) {
            m_messageCallback(this, *message);
        }
    }
}

// Receive thread function
void NetworkClient::ReceiveThreadFunction() {
    const int BUFFER_SIZE = 4096;
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    std::vector<uint8_t> receiveBuffer;

    while (m_threadRunning && m_connected) {
        // Receive data
        int bytesReceived = recv(m_socket, reinterpret_cast<char*>(buffer.data()), BUFFER_SIZE, 0);
        
        if (bytesReceived > 0) {
            // Append received data to buffer
            receiveBuffer.insert(receiveBuffer.end(), buffer.begin(), buffer.begin() + bytesReceived);
            
            // Process received data
            if (!ProcessReceivedData(receiveBuffer)) {
                // Error processing data
                break;
            }
        }
        else if (bytesReceived == 0) {
            // Connection closed
            break;
        }
        else {
            // Error or would block
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                std::cerr << "Error receiving data: " << error << std::endl;
                break;
            }
        }

        // Sleep to avoid high CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // If we exited the loop due to an error, disconnect
    if (m_connected) {
        Disconnect();
    }
}

// Send handshake
bool NetworkClient::SendHandshake() {
    // Create handshake request
    HandshakeRequest request(m_clientVersion);
    std::vector<uint8_t> payload = request.Serialize();
    
    // Create message
    Message message(MessageType::HANDSHAKE_REQUEST, payload);
    
    // Send message
    return SendMessage(message);
}

// Process handshake response
bool NetworkClient::ProcessHandshakeResponse(const Message& message) {
    try {
        // Deserialize handshake response
        HandshakeResponse response = HandshakeResponse::Deserialize(message.GetPayload());
        
        // Check if handshake was accepted
        if (response.accepted) {
            m_handshakeCompleted = true;
            std::cout << "Handshake completed: " << response.message << std::endl;
            return true;
        }
        else {
            std::cerr << "Handshake rejected: " << response.message << std::endl;
            Disconnect();
            return false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error processing handshake response: " << e.what() << std::endl;
        Disconnect();
        return false;
    }
}

// Process received data
bool NetworkClient::ProcessReceivedData(const std::vector<uint8_t>& data) {
    size_t processedBytes = 0;
    
    while (processedBytes < data.size()) {
        // Check if we have enough data for a header
        if (data.size() - processedBytes < sizeof(MessageHeader)) {
            break;
        }
        
        // Get header
        const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data.data() + processedBytes);
        
        // Check if we have the full message
        size_t messageSize = sizeof(MessageHeader) + header->payloadSize;
        if (data.size() - processedBytes < messageSize) {
            break;
        }
        
        try {
            // Deserialize message
            std::unique_ptr<Message> message = Message::Deserialize(data.data() + processedBytes, messageSize);
            
            // Add message to queue
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_messageQueue.push(std::move(message));
            }
            
            // Update processed bytes
            processedBytes += messageSize;
        }
        catch (const std::exception& e) {
            std::cerr << "Error deserializing message: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Remove processed bytes from buffer
    std::vector<uint8_t> newData(data.begin() + processedBytes, data.end());
    const_cast<std::vector<uint8_t>&>(data) = newData;
    
    return true;
}

// Try to reconnect to the server
void NetworkClient::TryReconnect() {
    // Check if we've reached the maximum number of reconnect attempts
    if (m_reconnectAttempts >= m_maxReconnectAttempts) {
        return;
    }
    
    // Check if it's time to reconnect
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastReconnectAttempt).count();
    
    if (elapsed < m_reconnectInterval) {
        return;
    }
    
    // Try to reconnect
    m_lastReconnectAttempt = now;
    m_reconnectAttempts++;
    
    std::cout << "Attempting to reconnect to server (" << m_reconnectAttempts << "/" << m_maxReconnectAttempts << ")..." << std::endl;
    
    if (Connect()) {
        std::cout << "Reconnected to server" << std::endl;
    }
    else {
        std::cerr << "Failed to reconnect to server" << std::endl;
    }
}
