#pragma once
#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "NetworkClient.h"
#include "nvse/PluginAPI.h"
#include "nvse/GameAPI.h"  // Include GameAPI.h for QueueUIMessage
#include <memory>
#include <string>
#include <queue>
#include <mutex>

// Forward declarations
class QuestStateTracker;

/**
 * @brief Network Manager for Quest Sync
 *
 * This class manages the network communication for the Quest Sync plugin.
 * It provides a singleton interface for accessing the network client and
 * handles initialization, connection, and message processing.
 */
class NetworkManager {
public:
    /**
     * @brief Get the singleton instance
     *
     * @return Reference to the NetworkManager instance
     */
    static NetworkManager& GetInstance();

    /**
     * @brief Initialize the network manager
     *
     * @param nvseInterface The NVSE interface
     * @param consoleInterface The NVSE console interface
     * @return true if initialization was successful, false otherwise
     */
    bool Initialize(const void* nvseInterface, NVSEConsoleInterface* consoleInterface);

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
     * @brief Check if connected to the server
     *
     * @return true if connected, false otherwise
     */
    bool IsConnected() const;

    /**
     * @brief Process network messages
     *
     * This method should be called regularly to process received messages
     * and handle reconnection attempts.
     */
    void ProcessMessages();

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
     * @brief Queue a message to be sent to the server
     *
     * This method adds a message to the queue to be sent during the next
     * ProcessMessages call. This is useful for batching messages.
     *
     * @param type The message type
     * @param payload The message payload
     */
    void QueueMessage(MessageType type, const std::string& payload);

    /**
     * @brief Get the network client
     *
     * @return Pointer to the network client
     */
    NetworkClient* GetClient();

    /**
     * @brief Show a notification message in the game
     *
     * @param message The message to display
     */
    void ShowNotification(const std::string& message);

    /**
     * @brief Reset the network manager
     *
     * This should be called when starting a new game or loading a save.
     */
    void Reset();

    /**
     * @brief Handle save game loading
     *
     * This should be called when a save game is loaded to ensure
     * the connection is stable before sending quest updates.
     */
    void HandleSaveGameLoaded();

private:
    // Private constructor for singleton
    NetworkManager();

    // Deleted copy constructor and assignment operator
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;

    // Network client
    std::unique_ptr<NetworkClient> m_client;

    // Initialization state
    bool m_initialized;

    // NVSE interfaces
    NVSEConsoleInterface* m_consoleInterface;

    // Quest state tracker
    QuestStateTracker* m_questStateTracker;

    // Message queue for batching
    std::queue<std::pair<MessageType, std::string>> m_messageQueue;
    std::mutex m_queueMutex;

    // Message handling
    void OnMessageReceived(NetworkClient* client, const Message& message);
    void OnConnectionStatusChanged(NetworkClient* client, bool connected);
    void ProcessQueuedMessages();
};

#endif // NETWORK_MANAGER_H
