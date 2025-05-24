#include "TCPServer.h"
#include <iostream>
#include <algorithm>
#include "Logger.h"
#include "Config.h"
#include "SessionManager.h"
#include "RateLimiter.h"

// Constructor
TCPServer::TCPServer(const std::string& ipAddress, int port, MessageHandler messageHandler)
    : m_ipAddress(ipAddress), m_port(port), m_messageHandler(messageHandler),
      m_running(false), m_listenSocket(INVALID_SOCKET), m_cleanupRunning(false) {
    // Initialize the master set
    FD_ZERO(&m_masterSet);
}

// Destructor
TCPServer::~TCPServer() {
    // Stop the server if it's running
    if (m_running) {
        Stop();
    }

    // Stop cleanup tasks if running
    StopCleanupTasks();
}

// Initialize the server
bool TCPServer::Initialize() {
    LOG_INFO("Initializing TCP server on " + (m_ipAddress.empty() ? "0.0.0.0" : m_ipAddress) + ":" + std::to_string(m_port));

    // Initialize Winsock
    if (!InitializeWinsock()) {
        LOG_CRITICAL("Failed to initialize Winsock");
        return false;
    }

    // Create the listening socket
    if (!CreateListenSocket()) {
        LOG_CRITICAL("Failed to create listening socket");
        WSACleanup();
        return false;
    }

    LOG_INFO("Server initialization complete");
    return true;
}

// Start the server
bool TCPServer::Start() {
    if (m_running) {
        LOG_WARNING("Server is already running");
        return false;
    }

    // Set the running flag
    m_running = true;

    // Start the worker thread
    m_workerThread = std::thread(&TCPServer::ServerLoop, this);

    // Start background cleanup tasks
    StartCleanupTasks();

    LOG_INFO("Server started on " + (m_ipAddress.empty() ? "0.0.0.0" : m_ipAddress) + ":" + std::to_string(m_port));

    return true;
}

// Stop the server
void TCPServer::Stop() {
    if (!m_running) {
        return;
    }

    LOG_INFO("Stopping server...");

    // Clear the running flag first to prevent error logging during shutdown
    m_running = false;

    // Stop cleanup tasks first
    StopCleanupTasks();

    // Disconnect all clients first
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        LOG_INFO("Disconnecting " + std::to_string(m_clients.size()) + " clients");
        for (const auto& client : m_clients) {
            closesocket(client.first);
        }
        m_clients.clear();
    }

    // Close the listening socket to unblock the select call
    // This will cause the select call to return with an error, but we won't log it
    // because m_running is already false
    closesocket(m_listenSocket);

    // Wait for the worker thread to finish
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    // Cleanup Winsock
    WSACleanup();

    LOG_INFO("Server stopped");
}

// Send a message to a specific client
bool TCPServer::SendToClient(SOCKET clientSocket, const Message& message) {
    // Serialize the message
    std::vector<uint8_t> data = message.Serialize();

    // Send the data
    int bytesSent = send(clientSocket, reinterpret_cast<const char*>(data.data()),
                         static_cast<int>(data.size()), 0);

    if (bytesSent == SOCKET_ERROR) {
        int error = WSAGetLastError();
        LOG_ERROR("Failed to send message to client " + std::to_string(clientSocket) +
                 ": error " + std::to_string(error));
        return false;
    }

    LOG_DEBUG("Sent message to client " + std::to_string(clientSocket) +
             ", type: " + std::to_string(static_cast<int>(message.GetType())) +
             ", size: " + std::to_string(data.size()) + " bytes");
    return true;
}

// Send a message to all connected clients
void TCPServer::BroadcastMessage(const Message& message, SOCKET excludeSocket) {
    // Serialize the message once
    std::vector<uint8_t> data = message.Serialize();

    // Lock the clients map
    std::lock_guard<std::mutex> lock(m_clientsMutex);

    // Send to all clients except the excluded one
    for (const auto& client : m_clients) {
        if (client.first != excludeSocket) {
            send(client.first, reinterpret_cast<const char*>(data.data()),
                 static_cast<int>(data.size()), 0);
        }
    }
}

// Get the number of connected clients
size_t TCPServer::GetClientCount() {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    return m_clients.size();
}

// Get information about all connected clients
std::vector<std::tuple<SOCKET, std::string, bool>> TCPServer::GetClientInfo() {
    std::vector<std::tuple<SOCKET, std::string, bool>> clientInfo;

    // Lock the clients map
    std::lock_guard<std::mutex> lock(m_clientsMutex);

    // Iterate through all clients
    for (const auto& client : m_clients) {
        SOCKET socket = client.first;
        bool authenticated = client.second;

        // Get the client's IP address
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        std::string ipAddress = "Unknown";

        // Get the peer name (IP address)
        if (getpeername(socket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize) != SOCKET_ERROR) {
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            ipAddress = std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port));
        }

        // Add the client info to the vector
        clientInfo.emplace_back(socket, ipAddress, authenticated);
    }

    return clientInfo;
}

// Disconnect a client by socket ID
bool TCPServer::KickClient(SOCKET clientSocket) {
    // Check if the client exists
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        if (m_clients.find(clientSocket) == m_clients.end()) {
            return false;
        }
    }

    // Disconnect the client
    DisconnectClient(clientSocket);
    return true;
}

// Send a text message to all connected clients
void TCPServer::BroadcastText(const std::string& text, SOCKET excludeSocket) {
    // Create a text message
    Message message(MessageType::ERROR_MESSAGE);  // Changed from TEXT_MESSAGE to ERROR_MESSAGE

    // Create a simple payload with the text
    std::vector<uint8_t> payload(text.begin(), text.end());
    message.SetPayload(payload);

    // Broadcast the message
    BroadcastMessage(message, excludeSocket);
}

// Main server loop
void TCPServer::ServerLoop() {
    LOG_DEBUG("Server loop starting");

    // Add the listening socket to the master set
    FD_SET(m_listenSocket, &m_masterSet);

    while (m_running) {
        // Create a copy of the master set for select
        fd_set readSet = m_masterSet;

        // Set up the timeout (100ms)
        timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        // Wait for activity on any socket
        int socketCount = select(0, &readSet, nullptr, nullptr, &timeout);

        if (socketCount == SOCKET_ERROR) {
            int error = WSAGetLastError();

            // Only log the error if it's not due to shutdown
            // WSAEINTR is expected during shutdown
            if (m_running && error != WSAEINTR) {
                LOG_ERROR("select failed: error " + std::to_string(error));
            }

            // If we're shutting down or got an interrupt, exit gracefully
            if (!m_running || error == WSAEINTR) {
                break;
            }

            // For other errors, also break the loop
            break;
        }

        // Check each socket for activity
        for (unsigned int i = 0; i < readSet.fd_count; i++) {
            SOCKET socket = readSet.fd_array[i];

            // If it's the listening socket, accept a new connection
            if (socket == m_listenSocket) {
                SOCKET clientSocket = AcceptClient();
                if (clientSocket != INVALID_SOCKET) {
                    // Add the new client to the master set
                    FD_SET(clientSocket, &m_masterSet);

                    // Add the client to the clients map (not authenticated yet)
                    std::lock_guard<std::mutex> lock(m_clientsMutex);
                    m_clients[clientSocket] = false;

                    LOG_INFO("Client connected: " + std::to_string(clientSocket));
                }
            }
            // Otherwise, it's a client socket with data
            else {
                // Handle the client data
                bool keepClient = HandleClientData(socket);

                // If the client should be disconnected, remove it
                if (!keepClient) {
                    DisconnectClient(socket);
                }
            }
        }
    }

    LOG_DEBUG("Server loop exiting");
}

// Accept a new client connection
SOCKET TCPServer::AcceptClient() {
    // Accept the connection
    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);
    SOCKET clientSocket = accept(m_listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize);

    if (clientSocket == INVALID_SOCKET) {
        int error = WSAGetLastError();

        // Only log the error if the server is still running
        // During shutdown, errors are expected and shouldn't be logged
        if (m_running && error != WSAEINTR) {
            LOG_ERROR("Failed to accept client connection: error " + std::to_string(error));
        }
        return INVALID_SOCKET;
    }

    // Get the client's IP address
    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);

    LOG_INFO("New connection from " + std::string(clientIP) + ":" + std::to_string(ntohs(clientAddr.sin_port)));

    return clientSocket;
}

// Handle data from a client
bool TCPServer::HandleClientData(SOCKET clientSocket) {
    // Buffer for receiving data
    char buffer[BUFFER_SIZE];

    // Receive data from the client
    int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);

    // Check for errors or disconnection
    if (bytesReceived <= 0) {
        if (bytesReceived == 0) {
            LOG_INFO("Client disconnected: " + std::to_string(clientSocket));
        } else {
            int error = WSAGetLastError();

            // Only log the error if the server is still running
            // During shutdown, errors are expected and shouldn't be logged
            if (m_running && error != WSAEINTR) {
                LOG_ERROR("Error receiving data from client " + std::to_string(clientSocket) +
                         ": error " + std::to_string(error));
            }
        }
        return false;
    }

    try {
        // Deserialize the message
        std::vector<uint8_t> data(buffer, buffer + bytesReceived);
        std::unique_ptr<Message> message = Message::Deserialize(data);

        // Check if this is a handshake request
        if (message->GetType() == MessageType::HANDSHAKE_REQUEST) {
            // Extract the handshake request
            HandshakeRequest request = HandshakeRequest::Deserialize(message->GetPayload());

            // Process the handshake
            ProcessHandshake(clientSocket, request);
        }
        // Otherwise, pass the message to the handler
        else {
            // Check if the client is authenticated
            bool authenticated = false;
            {
                std::lock_guard<std::mutex> lock(m_clientsMutex);
                auto it = m_clients.find(clientSocket);
                if (it != m_clients.end()) {
                    authenticated = it->second;
                }
            }

            // For authenticated clients, validate session token if authentication is enabled
            if (authenticated) {
                Config& config = Config::GetInstance();
                bool authenticationEnabled = config.GetBool("Security.EnableAuthentication", false);

                if (authenticationEnabled) {
                    // Validate session token for authenticated clients
                    SessionManager& sessionManager = SessionManager::GetInstance();
                    bool sessionValid = false;

                    // Check if this message type requires session token validation
                    if (RequiresSessionTokenValidation(message->GetType())) {
                        // Validate the session token from the message header
                        std::string messageToken = message->GetSessionToken();
                        if (!messageToken.empty()) {
                            sessionValid = sessionManager.ValidateSessionToken(clientSocket, messageToken);
                            if (sessionValid) {
                                LOG_DEBUG("Session token validated for client " + std::to_string(clientSocket) +
                                         " message type " + std::to_string(static_cast<int>(message->GetType())));
                            } else {
                                LOG_WARNING("Invalid session token for client " + std::to_string(clientSocket) +
                                           " message type " + std::to_string(static_cast<int>(message->GetType())));
                            }
                        } else {
                            LOG_WARNING("Missing session token for client " + std::to_string(clientSocket) +
                                       " message type " + std::to_string(static_cast<int>(message->GetType())));
                        }
                    } else {
                        // System messages that don't require token validation
                        sessionValid = true;
                        LOG_DEBUG("Message type " + std::to_string(static_cast<int>(message->GetType())) +
                                 " from client " + std::to_string(clientSocket) + " - no token validation required");
                    }

                    if (!sessionValid) {
                        LOG_WARNING("Received message from client with invalid/missing session token: " + std::to_string(clientSocket));
                        return false;
                    }
                }

                if (m_messageHandler) {
                    m_messageHandler(this, clientSocket, *message);
                }
            }
            else {
                LOG_WARNING("Received message from unauthenticated client: " + std::to_string(clientSocket));
                return false;
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Error processing message from client " + std::to_string(clientSocket) +
                 ": " + std::string(e.what()));
        return false;
    }

    return true;
}

// Disconnect a client
void TCPServer::DisconnectClient(SOCKET clientSocket) {
    // Remove the client from the master set
    FD_CLR(clientSocket, &m_masterSet);

    // Remove the client from the clients map
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients.erase(clientSocket);
    }

    // Clean up session for this client
    SessionManager::GetInstance().RemoveSession(clientSocket);

    // Close the socket
    closesocket(clientSocket);

    LOG_INFO("Client disconnected: " + std::to_string(clientSocket));
}

// Process a handshake request
void TCPServer::ProcessHandshake(SOCKET clientSocket, const HandshakeRequest& request) {
    // Get client IP address
    std::string clientIP = GetClientIP(clientSocket);

    // Log client version
    LOG_INFO("Processing handshake from client " + std::to_string(clientSocket) +
             " (IP: " + clientIP + "), version: " + Version::VersionToString(request.clientVersion));

    // Check if the client version is compatible
    bool accepted = Version::IsCompatible(request.clientVersion);
    std::string compatibilityMessage = Version::GetCompatibilityErrorMessage(request.clientVersion);

    LOG_INFO("Client version compatibility check: " + std::string(accepted ? "COMPATIBLE" : "INCOMPATIBLE"));
    LOG_INFO("Compatibility details: " + compatibilityMessage);
    LOG_INFO("Server version: " + Version::VersionToString(Version::ServerVersion) +
             ", supported client versions: " + Version::VersionToString(Version::MinClientVersion) +
             " to " + Version::VersionToString(Version::MaxClientVersion));

    // Check authentication if enabled and version is compatible
    std::string sessionToken;
    if (accepted) {
        Config& config = Config::GetInstance();
        bool authenticationEnabled = config.GetBool("Security.EnableAuthentication", false);

        if (authenticationEnabled) {
            // Check rate limiting first
            bool rateLimitEnabled = config.GetBool("Security.RateLimitEnabled", true);
            if (rateLimitEnabled) {
                int maxAttempts = config.GetInt("Security.RateLimitAttempts", 5);
                int windowSeconds = config.GetInt("Security.RateLimitWindow", 300);

                RateLimiter& rateLimiter = RateLimiter::GetInstance();
                if (!rateLimiter.IsAllowed(clientIP, maxAttempts, windowSeconds)) {
                    accepted = false;
                    compatibilityMessage = "Rate limit exceeded: Too many authentication attempts";
                    LOG_WARNING("Rate limit exceeded for client " + std::to_string(clientSocket) + " (IP: " + clientIP + ")");

                    // Prepare response and return early (don't record this attempt)
                    std::string message = compatibilityMessage;
                    HandshakeResponse response(accepted, message);

                    Message responseMsg(MessageType::HANDSHAKE_RESPONSE);
                    std::vector<uint8_t> payload = response.Serialize();
                    responseMsg.SetPayload(payload);

                    SendToClient(clientSocket, responseMsg);
                    LOG_INFO("Handshake response sent: REJECTED (rate limited)");
                    return;
                }

                // Record the authentication attempt (only if not rate limited)
                rateLimiter.RecordAttempt(clientIP);
            }

            std::string serverPassword = config.GetString("Security.Password", "");

            LOG_DEBUG("Authentication enabled, validating credentials");

            // Validate that server has a non-empty password when authentication is enabled
            if (serverPassword.empty()) {
                accepted = false;
                compatibilityMessage = "Authentication failed: Server password not configured";
                LOG_ERROR("Authentication enabled but server password is empty");
            }
            // Check if passwords match
            else if (request.password != serverPassword) {
                accepted = false;
                compatibilityMessage = "Authentication failed: Invalid password";
                LOG_WARNING("Client authentication failed: invalid credentials");
            } else {
                LOG_INFO("Client authentication successful");

                // Generate session token
                SessionManager& sessionManager = SessionManager::GetInstance();
                int tokenExpiry = config.GetInt("Security.SessionTokenExpiry", 3600);
                sessionToken = sessionManager.GenerateSessionToken(clientSocket, clientIP, tokenExpiry);

                // Reset rate limit for successful authentication
                if (config.GetBool("Security.RateLimitEnabled", true)) {
                    RateLimiter::GetInstance().ResetRateLimit(clientIP);
                }
            }
        } else {
            LOG_DEBUG("Authentication disabled, accepting client");
        }
    }

    // Prepare the response message
    std::string message;
    if (accepted) {
        message = "Connection accepted. Server version: " +
                  Version::VersionToString(Version::ServerVersion);
    } else {
        message = compatibilityMessage;
    }

    // Create the handshake response with session token and expiry time
    Config& config = Config::GetInstance();
    int tokenExpiry = config.GetInt("Security.SessionTokenExpiry", 3600);
    HandshakeResponse response(accepted, message, sessionToken, accepted ? tokenExpiry : 0);

    // Log the response
    LOG_INFO("Handshake response: " + std::string(accepted ? "ACCEPTED" : "REJECTED") +
             ", message: " + message);

    // Create and send the message
    Message responseMsg(MessageType::HANDSHAKE_RESPONSE);
    std::vector<uint8_t> payload = response.Serialize();
    responseMsg.SetPayload(payload);

    // Log the full message
    std::vector<uint8_t> fullMessage = responseMsg.Serialize();
    std::string fullMessageHex;
    for (size_t i = 0; i < fullMessage.size() && i < 128; ++i) {
        char hex[8];
        sprintf_s(hex, "%02X ", fullMessage[i]);
        fullMessageHex += hex;
    }
    if (fullMessage.size() > 128) {
        fullMessageHex += "...";
    }
    LOG_DEBUG("Full handshake response message (hex): " + fullMessageHex);

    // Send the response
    bool sendResult = SendToClient(clientSocket, responseMsg);
    LOG_INFO("Handshake response send result: " + std::string(sendResult ? "SUCCESS" : "FAILURE"));

    // Update the client's authentication status
    if (accepted) {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        m_clients[clientSocket] = true;
        LOG_INFO("Client authenticated: " + std::to_string(clientSocket));
    } else {
        LOG_WARNING("Client rejected (incompatible version): " + std::to_string(clientSocket));
        // The client will be disconnected on the next iteration
    }
}

// Initialize Winsock
bool TCPServer::InitializeWinsock() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG_CRITICAL("WSAStartup failed: " + std::to_string(result));
        return false;
    }
    return true;
}

// Create the listening socket
bool TCPServer::CreateListenSocket() {
    // Create the socket
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) {
        int error = WSAGetLastError();
        LOG_CRITICAL("Failed to create socket: error " + std::to_string(error));
        return false;
    }

    // Set up the server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);

    // Bind to specific IP or any IP
    if (m_ipAddress.empty()) {
        serverAddr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, m_ipAddress.c_str(), &serverAddr.sin_addr);
    }

    // Bind the socket
    int result = bind(m_listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        LOG_CRITICAL("Failed to bind socket: error " + std::to_string(error));
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    // Start listening
    result = listen(m_listenSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        LOG_CRITICAL("Failed to listen on socket: error " + std::to_string(error));
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
        return false;
    }

    return true;
}

// Get the IP address of a client socket
std::string TCPServer::GetClientIP(SOCKET clientSocket) {
    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    if (getpeername(clientSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize) == SOCKET_ERROR) {
        LOG_WARNING("Failed to get client IP address: error " + std::to_string(WSAGetLastError()));
        return "unknown";
    }

    char ipStr[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, INET_ADDRSTRLEN) == nullptr) {
        LOG_WARNING("Failed to convert client IP address to string");
        return "unknown";
    }

    return std::string(ipStr);
}

// Start background cleanup tasks
void TCPServer::StartCleanupTasks() {
    if (m_cleanupRunning) {
        LOG_WARNING("Cleanup tasks are already running");
        return;
    }

    m_cleanupRunning = true;
    m_cleanupThread = std::thread(&TCPServer::CleanupTaskLoop, this);
    LOG_INFO("Background cleanup tasks started");
}

// Stop background cleanup tasks
void TCPServer::StopCleanupTasks() {
    if (!m_cleanupRunning) {
        return;
    }

    LOG_DEBUG("Stopping cleanup tasks...");
    m_cleanupRunning = false;

    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }

    LOG_INFO("Background cleanup tasks stopped");
}

// Background cleanup task loop
void TCPServer::CleanupTaskLoop() {
    LOG_DEBUG("Cleanup task loop starting");

    while (m_cleanupRunning) {
        try {
            // Clean up expired sessions every 5 minutes
            SessionManager::GetInstance().CleanupExpiredSessions();

            // Clean up old rate limit attempts every 10 minutes
            Config& config = Config::GetInstance();
            int rateLimitWindow = config.GetInt("Security.RateLimitWindow", 300);
            RateLimiter::GetInstance().CleanupOldAttempts(rateLimitWindow);

            LOG_DEBUG("Cleanup tasks completed successfully");
        }
        catch (const std::exception& e) {
            LOG_ERROR("Exception in cleanup task loop: " + std::string(e.what()));
        }
        catch (...) {
            LOG_ERROR("Unknown exception in cleanup task loop");
        }

        // Sleep for 5 minutes before next cleanup cycle
        for (int i = 0; i < 300 && m_cleanupRunning; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    LOG_DEBUG("Cleanup task loop ending");
}

// Check if a message type requires session token validation
bool TCPServer::RequiresSessionTokenValidation(MessageType messageType) const {
    // System messages that don't require session token validation
    switch (messageType) {
        case MessageType::HANDSHAKE_REQUEST:
        case MessageType::HANDSHAKE_RESPONSE:
        case MessageType::SESSION_TOKEN_REQUEST:
        case MessageType::SESSION_TOKEN_RESPONSE:
        case MessageType::DISCONNECT:
            return false;
        default:
            // All other messages require session token validation when authentication is enabled
            return true;
    }
}
