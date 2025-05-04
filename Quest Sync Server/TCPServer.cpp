#include "TCPServer.h"
#include <iostream>
#include <algorithm>
#include "Logger.h"

// Constructor
TCPServer::TCPServer(const std::string& ipAddress, int port, MessageHandler messageHandler)
    : m_ipAddress(ipAddress), m_port(port), m_messageHandler(messageHandler),
      m_running(false), m_listenSocket(INVALID_SOCKET) {
    // Initialize the master set
    FD_ZERO(&m_masterSet);
}

// Destructor
TCPServer::~TCPServer() {
    // Stop the server if it's running
    if (m_running) {
        Stop();
    }
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
    Message message(MessageType::TEXT_MESSAGE);

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

            // Only process messages from authenticated clients
            if (authenticated) {
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

    // Close the socket
    closesocket(clientSocket);

    LOG_INFO("Client disconnected: " + std::to_string(clientSocket));
}

// Process a handshake request
void TCPServer::ProcessHandshake(SOCKET clientSocket, const HandshakeRequest& request) {
    // Check if the client version is compatible
    bool accepted = Version::IsCompatible(request.clientVersion);

    // Prepare the response message
    std::string message;
    if (accepted) {
        message = "Connection accepted. Server version: " +
                  Version::VersionToString(Version::ServerVersion);
    } else {
        message = "Connection rejected. Incompatible version. Server version: " +
                  Version::VersionToString(Version::ServerVersion) +
                  ", supported client versions: " +
                  Version::VersionToString(Version::MinClientVersion) +
                  " to " +
                  Version::VersionToString(Version::MaxClientVersion);
    }

    // Create the handshake response
    HandshakeResponse response(accepted, message);

    // Create and send the message
    Message responseMsg(MessageType::HANDSHAKE_RESPONSE);
    responseMsg.SetPayload(response.Serialize());
    SendToClient(clientSocket, responseMsg);

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
