#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <chrono>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include "Message.h"
#include "Version.h"

// Forward declaration
class TCPServer;

// Callback type for message handling
using MessageHandler = std::function<void(TCPServer*, SOCKET, const Message&)>;

/**
 * @brief TCP Server class that handles client connections and messages
 */
class TCPServer {
public:
    /**
     * @brief Constructor
     *
     * @param ipAddress IP address to bind to (empty for any)
     * @param port Port to listen on
     * @param messageHandler Callback function for handling received messages
     */
    TCPServer(const std::string& ipAddress, int port, MessageHandler messageHandler);

    /**
     * @brief Destructor
     */
    ~TCPServer();

    /**
     * @brief Initialize the server
     *
     * @return true if initialization was successful, false otherwise
     */
    bool Initialize();

    /**
     * @brief Start the server
     *
     * @return true if server started successfully, false otherwise
     */
    bool Start();

    /**
     * @brief Stop the server
     */
    void Stop();

    /**
     * @brief Send a message to a specific client
     *
     * @param clientSocket Socket of the client to send to
     * @param message Message to send
     * @return true if message was sent successfully, false otherwise
     */
    bool SendToClient(SOCKET clientSocket, const Message& message);

    /**
     * @brief Send a message to all connected clients
     *
     * @param message Message to send
     * @param excludeSocket Optional socket to exclude from broadcast
     */
    void BroadcastMessage(const Message& message, SOCKET excludeSocket = INVALID_SOCKET);

    /**
     * @brief Check if the server is running
     *
     * @return true if server is running, false otherwise
     */
    bool IsRunning() const { return m_running; }

    /**
     * @brief Get the number of connected clients
     *
     * @return Number of connected clients
     */
    size_t GetClientCount();

    /**
     * @brief Get information about all connected clients
     *
     * @return Vector of client information (socket, IP address, authenticated status)
     */
    std::vector<std::tuple<SOCKET, std::string, bool>> GetClientInfo();

    /**
     * @brief Disconnect a client by socket ID
     *
     * @param clientSocket Socket of the client to disconnect
     * @return true if client was disconnected, false if client was not found
     */
    bool KickClient(SOCKET clientSocket);

    /**
     * @brief Send a text message to all connected clients
     *
     * @param text Text message to send
     * @param excludeSocket Optional socket to exclude from broadcast
     */
    void BroadcastText(const std::string& text, SOCKET excludeSocket = INVALID_SOCKET);

    /**
     * @brief Start background cleanup tasks
     */
    void StartCleanupTasks();

    /**
     * @brief Stop background cleanup tasks
     */
    void StopCleanupTasks();

private:
    // Server configuration
    std::string m_ipAddress;
    int m_port;
    MessageHandler m_messageHandler;

    // Server state
    std::atomic<bool> m_running;
    SOCKET m_listenSocket;

    // Client management
    fd_set m_masterSet;
    std::mutex m_clientsMutex;
    std::unordered_map<SOCKET, bool> m_clients; // Socket -> authenticated flag

    // Worker thread
    std::thread m_workerThread;

    // Cleanup tasks
    std::atomic<bool> m_cleanupRunning;
    std::thread m_cleanupThread;

    // Buffer for receiving data
    static constexpr size_t BUFFER_SIZE = 8192;

    /**
     * @brief Main server loop
     */
    void ServerLoop();

    /**
     * @brief Accept a new client connection
     *
     * @return Socket of the new client, or INVALID_SOCKET on failure
     */
    SOCKET AcceptClient();

    /**
     * @brief Handle data from a client
     *
     * @param clientSocket Socket of the client
     * @return true if client should be kept, false if client should be disconnected
     */
    bool HandleClientData(SOCKET clientSocket);

    /**
     * @brief Disconnect a client
     *
     * @param clientSocket Socket of the client to disconnect
     */
    void DisconnectClient(SOCKET clientSocket);

    /**
     * @brief Process a handshake request
     *
     * @param clientSocket Socket of the client
     * @param request Handshake request message
     */
    void ProcessHandshake(SOCKET clientSocket, const HandshakeRequest& request);

    /**
     * @brief Initialize Winsock
     *
     * @return true if initialization was successful, false otherwise
     */
    bool InitializeWinsock();

    /**
     * @brief Create the listening socket
     *
     * @return true if socket creation was successful, false otherwise
     */
    bool CreateListenSocket();

    /**
     * @brief Get the IP address of a client socket
     *
     * @param clientSocket Socket of the client
     * @return IP address as string
     */
    std::string GetClientIP(SOCKET clientSocket);

    /**
     * @brief Background cleanup task loop
     */
    void CleanupTaskLoop();

    /**
     * @brief Check if a message type requires session token validation
     *
     * @param messageType The message type to check
     * @return true if session token validation is required, false otherwise
     */
    bool RequiresSessionTokenValidation(MessageType messageType) const;
};
