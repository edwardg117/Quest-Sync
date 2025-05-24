#pragma once
#ifndef NETWORK_CLIENT_H
#define NETWORK_CLIENT_H

#include <string>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <atomic>
#include <memory>
#include <WS2tcpip.h>
#pragma comment (lib, "ws2_32.lib")

#include "Message.h"

// Forward declarations
class NetworkClient;

// Callback types
using MessageReceivedCallback = std::function<void(NetworkClient*, const Message&)>;
using ConnectionStatusCallback = std::function<void(NetworkClient*, bool)>;

/**
 * @brief TCP Network Client for Quest Sync
 *
 * This class handles the network communication with the Quest Sync server.
 * It provides a clean interface for sending and receiving messages, with
 * automatic reconnection and version compatibility checking.
 */
class NetworkClient {
public:
    // Buffer size for receiving data (8KB)
    static constexpr size_t BUFFER_SIZE = 8192;

    /**
     * @brief Constructor
     *
     * @param serverAddress The server IP address
     * @param serverPort The server port
     */
    NetworkClient(const std::string& serverAddress = "", int serverPort = 0);

    /**
     * @brief Destructor
     */
    ~NetworkClient();

    /**
     * @brief Initialize the network client
     *
     * @return true if initialization was successful, false otherwise
     */
    bool Initialize();

    /**
     * @brief Connect to the server
     *
     * @return true if connection was successful, false otherwise
     */
    bool Connect();

    /**
     * @brief Disconnect from the server
     */
    void Disconnect();

    /**
     * @brief Clean up resources
     */
    void Cleanup();

    /**
     * @brief Check if connected to the server
     *
     * @return true if connected, false otherwise
     */
    bool IsConnected() const;

    /**
     * @brief Check if the client is initialized
     *
     * @return true if initialized, false otherwise
     */
    bool IsInitialized() const;

    /**
     * @brief Send a message to the server
     *
     * @param message The message to send
     * @return true if the message was sent successfully, false otherwise
     */
    bool SendMessage(const Message& message);

    /**
     * @brief Send a string message to the server
     *
     * @param type The message type
     * @param payload The message payload
     * @return true if the message was sent successfully, false otherwise
     */
    bool SendMessage(MessageType type, const std::string& payload);

    /**
     * @brief Get the next received message
     *
     * @return The next message, or nullptr if no messages are available
     */
    std::unique_ptr<Message> GetNextMessage();

    /**
     * @brief Set the server address
     *
     * @param address The server IP address
     */
    void SetServerAddress(const std::string& address);

    /**
     * @brief Set the server port
     *
     * @param port The server port
     */
    void SetServerPort(int port);

    /**
     * @brief Set the message received callback
     *
     * @param callback The callback function
     */
    void SetMessageReceivedCallback(MessageReceivedCallback callback);

    /**
     * @brief Set the connection status callback
     *
     * @param callback The callback function
     */
    void SetConnectionStatusCallback(ConnectionStatusCallback callback);

    /**
     * @brief Get the client version
     *
     * @return The client version as an array [major, minor]
     */
    std::array<int, 2> GetClientVersion() const;

    /**
     * @brief Set the client version
     *
     * @param major The major version number
     * @param minor The minor version number
     */
    void SetClientVersion(int major, int minor);

    /**
     * @brief Validate client version for basic sanity checks
     *
     * @return True if client version appears valid, false otherwise
     */
    bool ValidateClientVersion() const;

    /**
     * @brief Process any pending messages
     *
     * This method should be called regularly to process received messages
     * and handle reconnection attempts.
     */
    void ProcessMessages();

    void ResetReconnectCounter();  // Reset counter on successful connection

    /**
     * @brief Try to reconnect to the server
     *
     * This method checks if reconnection is needed and attempts to reconnect
     * if the maximum number of attempts hasn't been exceeded and enough time
     * has passed since the last attempt.
     */
    void TryReconnect();

private:
    // Server information
    std::string m_serverAddress;
    int m_serverPort;

    // Socket and connection state
    SOCKET m_socket;
    std::atomic<bool> m_connected;
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_handshakeCompleted;

    // Client version
    std::array<int, 2> m_clientVersion;

    // Reconnection settings
    int m_reconnectInterval;
    int m_maxReconnectAttempts;
    int m_reconnectAttempts;
    std::chrono::steady_clock::time_point m_lastReconnectAttempt;
    bool m_maxAttemptsWarningLogged;

    // Message queue and thread safety
    std::queue<std::unique_ptr<Message>> m_messageQueue;
    std::mutex m_queueMutex;

    // Receive thread
    std::thread m_receiveThread;
    std::atomic<bool> m_threadRunning;

    // Callbacks
    MessageReceivedCallback m_messageCallback;
    ConnectionStatusCallback m_connectionCallback;

    // Private methods
    void ReceiveThreadFunction();
    bool SendHandshake();
    bool ProcessHandshakeResponse(const Message& message);
    bool ProcessReceivedData(std::vector<uint8_t>& data);
    bool ProcessReceivedData(char* buffer, int bytesReceived);
};

#endif // NETWORK_CLIENT_H



