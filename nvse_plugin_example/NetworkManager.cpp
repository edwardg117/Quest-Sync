#include "NetworkManager.h"
#include "Config.h"
#include "QuestStateTracker.h"
#include "QuestSyncLogging.h"
#include "nvse/PluginAPI.h"
#include "nvse/GameAPI.h"
#include <iostream>
#include <sstream>

// Singleton instance
NetworkManager& NetworkManager::GetInstance() {
    static NetworkManager instance;
    return instance;
}

// Constructor
NetworkManager::NetworkManager()
    : m_initialized(false),
      m_consoleInterface(nullptr),
      m_questStateTracker(nullptr),
      m_client(nullptr) {
}

// Initialize the network manager
bool NetworkManager::Initialize(const void* nvseInterface, NVSEConsoleInterface* consoleInterface) {
    QUESTSYNC_LOG_INFO("NetworkManager Initialize called");

    if (m_initialized) {
        QUESTSYNC_LOG_DEBUG("NetworkManager already initialized");
        return true;
    }

    try {
        // Store console interface
        m_consoleInterface = consoleInterface;

        // Load configuration
        Config& config = Config::GetInstance();

        // Try to load the config file, if it fails, continue with defaults
        if (!config.Load(nvseInterface, "QuestSync.ini")) {
            QUESTSYNC_LOG_WARNING("Failed to load config file, using defaults");
        }

        std::string serverAddress = config.GetString("Network.ServerAddress", "127.0.0.1");
        int serverPort = config.GetInt("Network.ServerPort", 25575);

        QUESTSYNC_LOG_INFO("Server address: %s, port: %d", serverAddress.c_str(), serverPort);

        // Initialize quest state tracker first to ensure it's available even if networking fails
        m_questStateTracker = &QuestStateTracker::GetInstance();
        if (!m_questStateTracker->Initialize(consoleInterface)) {
            QUESTSYNC_LOG_ERROR("Failed to initialize quest state tracker");
            return false;
        }

        // Set the network manager in the quest state tracker
        m_questStateTracker->SetNetworkManager(this);
        QUESTSYNC_LOG_INFO("Quest state tracker initialized and linked");

        // Create network client
        m_client = std::make_unique<NetworkClient>(serverAddress, serverPort);

        // Initialize client
        if (!m_client->Initialize()) {
            QUESTSYNC_LOG_ERROR("Failed to initialize network client");

            // Continue without network functionality rather than failing completely
            QUESTSYNC_LOG_WARNING("Continuing without network functionality");
            ShowNotification("Quest Sync network functionality disabled");

            m_initialized = true;
            return true;
        }



        // Set connection callback
        m_client->SetConnectionStatusCallback([this](NetworkClient* client, bool connected) {
            OnConnectionStatusChanged(client, connected);
        });

        // Set message callback
        m_client->SetMessageReceivedCallback([this](NetworkClient* client, const Message& message) {
            OnMessageReceived(client, message);
        });

        m_initialized = true;
        QUESTSYNC_LOG_INFO("NetworkManager initialized successfully");

        return true;
    }
    catch (const std::exception& e) {
        QUESTSYNC_LOG_ERROR("NetworkManager initialization exception: %s", e.what());

        // Show a notification to the user
        if (consoleInterface) {
            std::string errorMsg = "Quest Sync initialization error: " + std::string(e.what());
            ShowNotification(errorMsg);
        }

        // Still initialize the quest state tracker to allow the game to run without network functionality
        try {
            if (!m_questStateTracker) {
                m_questStateTracker = &QuestStateTracker::GetInstance();
                if (m_questStateTracker->Initialize(consoleInterface)) {
                    QUESTSYNC_LOG_INFO("Quest state tracker initialized in fallback mode");
                    m_initialized = true;
                    return true;
                }
            } else {
                // Quest state tracker already initialized
                m_initialized = true;
                return true;
            }
        }
        catch (const std::exception& e) {
            QUESTSYNC_LOG_ERROR("Failed to initialize quest state tracker in fallback mode: %s", e.what());
        }
        catch (...) {
            QUESTSYNC_LOG_ERROR("Failed to initialize quest state tracker in fallback mode");
        }

        return false;
    }
    catch (...) {
        QUESTSYNC_LOG_ERROR("NetworkManager initialization unknown exception");

        // Show a notification to the user
        if (consoleInterface) {
            ShowNotification("Quest Sync initialization error: Unknown exception");
        }

        // Still initialize the quest state tracker to allow the game to run without network functionality
        try {
            if (!m_questStateTracker) {
                m_questStateTracker = &QuestStateTracker::GetInstance();
                if (m_questStateTracker->Initialize(consoleInterface)) {
                    QUESTSYNC_LOG_INFO("Quest state tracker initialized in fallback mode");
                    m_initialized = true;
                    return true;
                }
            } else {
                // Quest state tracker already initialized
                m_initialized = true;
                return true;
            }
        }
        catch (...) {
            QUESTSYNC_LOG_ERROR("Failed to initialize quest state tracker in fallback mode");
        }

        return false;
    }
}

// Connect to the server
bool NetworkManager::Connect() {
    QUESTSYNC_LOG_INFO("NetworkManager Connect called");

    if (!m_initialized) {
        QUESTSYNC_LOG_ERROR("NetworkManager not initialized");
        return false;
    }

    // Check if client is valid
    if (!m_client) {
        QUESTSYNC_LOG_ERROR("Client is null");
        return false;
    }

    // Check if client is initialized
    if (!m_client->IsInitialized()) {
        QUESTSYNC_LOG_ERROR("Client is not initialized");
        return false;
    }

    try {
        QUESTSYNC_LOG_INFO("Attempting to connect to server");
        bool result = m_client->Connect();
        QUESTSYNC_LOG_INFO("Connection attempt result: %s", result ? "success" : "failure");

        if (!result) {
            // Show a notification to the user that connection failed
            ShowNotification("Failed to connect to Quest Sync server. Will try again later.");
        } else {
            // Show a success notification
            ShowNotification("Connected to Quest Sync server");
        }

        return result;
    }
    catch (const std::exception& e) {
        QUESTSYNC_LOG_ERROR("Exception during connect: %s", e.what());

        // Show a notification to the user
        std::string errorMsg = "Quest Sync connection error: " + std::string(e.what());
        ShowNotification(errorMsg);

        return false;
    }
    catch (...) {
        QUESTSYNC_LOG_ERROR("Unknown exception during connect");

        // Show a notification to the user
        ShowNotification("Quest Sync connection error: Unknown exception");

        return false;
    }
}

// Disconnect from the server
void NetworkManager::Disconnect() {
    QUESTSYNC_LOG_INFO("NetworkManager Disconnect called");

    // Clear message queue first to avoid sending messages during disconnect
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<std::pair<MessageType, std::string>> empty;
        std::swap(m_messageQueue, empty);
        QUESTSYNC_LOG_DEBUG("Message queue cleared");
    }

    // Disconnect client
    if (m_client) {
        QUESTSYNC_LOG_DEBUG("Disconnecting client");
        try {
            m_client->Disconnect();
            QUESTSYNC_LOG_INFO("Client disconnected successfully");
        }
        catch (const std::exception& e) {
            QUESTSYNC_LOG_ERROR("Exception during client disconnect: %s", e.what());
        }
        catch (...) {
            QUESTSYNC_LOG_ERROR("Unknown exception during client disconnect");
        }
    } else {
        QUESTSYNC_LOG_DEBUG("Client is null");
    }
}

// Check if connected to the server
bool NetworkManager::IsConnected() const {
    bool connected = m_client && m_client->IsConnected();

    // Only log occasionally to avoid filling the log file, and only in debug mode
    static int logCounter = 0;
    if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG && logCounter++ % 1000 == 0) {
        QUESTSYNC_LOG_DEBUG("Connection status: %s", connected ? "connected" : "not connected");
    }

    return connected;
}

// Process network messages
void NetworkManager::ProcessMessages() {
    static int counter = 0;
    counter++;

    // Only log every 1000 frames to avoid log spam, and only if debug mode is enabled
    bool shouldLog = (counter % 1000 == 0) && (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG);

    if (!m_initialized) {
        if (shouldLog) {
            QUESTSYNC_LOG_DEBUG("NetworkManager not initialized");
        }
        return;
    }

    if (!m_client) {
        if (shouldLog) {
            QUESTSYNC_LOG_DEBUG("Client is null");
        }
        return;
    }

    // Check if client is initialized
    if (!m_client->IsInitialized()) {
        if (shouldLog) {
            QUESTSYNC_LOG_DEBUG("Client is not initialized");
        }
        return;
    }

    try {
        // Check connection status
        bool isConnected = m_client->IsConnected();

        // Only log connection status changes or occasionally
        static bool lastConnectionStatus = false;
        if (lastConnectionStatus != isConnected || shouldLog) {
            QUESTSYNC_LOG_DEBUG("Connection status: %s", isConnected ? "connected" : "not connected");
            lastConnectionStatus = isConnected;
        }

        if (!isConnected) {
            // Delegate reconnection to NetworkClient which properly respects MaxReconnectAttempts
            m_client->TryReconnect();
            return;
        }

        // Let the client process its messages
        m_client->ProcessMessages();

        // Process outgoing messages from our queue
        ProcessQueuedMessages();
    }
    catch (const std::exception& e) {
        QUESTSYNC_LOG_ERROR("ProcessMessages exception: %s", e.what());

        // Only show notification occasionally to avoid spamming the user
        static int errorCounter = 0;
        if (errorCounter++ % 100 == 0) {
            ShowNotification("Quest Sync error: " + std::string(e.what()));
        }
    }
    catch (...) {
        QUESTSYNC_LOG_ERROR("ProcessMessages unknown exception");

        // Only show notification occasionally to avoid spamming the user
        static int errorCounter = 0;
        if (errorCounter++ % 100 == 0) {
            ShowNotification("Quest Sync error: Unknown exception");
        }
    }
}

// Process queued messages
void NetworkManager::ProcessQueuedMessages() {
    // Only log when there are actually messages to process
    static int logCounter = 0;
    bool shouldLog = false;

    // Check connection status first
    if (!IsConnected()) {
        // Only log disconnected state occasionally and only in debug mode
        if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG && logCounter++ % 1000 == 0) {
            QUESTSYNC_LOG_DEBUG("Not connected, cannot process messages");
        }
        return;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);

    // Only log when there are messages or occasionally, and only in debug mode
    if (GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG && (!m_messageQueue.empty() || logCounter++ % 1000 == 0)) {
        shouldLog = true;
    }

    if (shouldLog && !m_messageQueue.empty()) {
        QUESTSYNC_LOG_DEBUG("Queue size: %d", m_messageQueue.size());
    }

    if (m_messageQueue.empty()) {
        return;
    }

    // Process up to 3 messages per frame to avoid overwhelming the server
    int messageCount = 0;
    const int maxMessagesPerFrame = 3;
    bool connectionLost = false;

    while (!m_messageQueue.empty() && messageCount < maxMessagesPerFrame && !connectionLost) {
        auto [type, payload] = m_messageQueue.front();

        if (shouldLog) {
            QUESTSYNC_LOG_DEBUG("Processing message type %d", static_cast<int>(type));
        }

        // Send the message
        bool success = false;
        try {
            success = SendMessage(type, payload);
            if (shouldLog) {
                QUESTSYNC_LOG_DEBUG("Message send %s", success ? "successful" : "failed");
            }
        }
        catch (const std::exception& e) {
            QUESTSYNC_LOG_ERROR("Exception during send: %s", e.what());
            success = false;
        }
        catch (...) {
            QUESTSYNC_LOG_ERROR("Unknown exception during send");
            success = false;
        }

        // Only remove the message from the queue if it was sent successfully
        if (success) {
            m_messageQueue.pop();
            messageCount++;
        } else {
            // Check if we're still connected
            if (!IsConnected()) {
                QUESTSYNC_LOG_WARNING("Connection lost during message processing");
                connectionLost = true;

                // Don't remove the message from the queue so we can retry after reconnecting
                // But limit how many messages we keep to avoid memory issues
                if (m_messageQueue.size() > 20) {
                    QUESTSYNC_LOG_WARNING("Queue too large, removing oldest message");
                    m_messageQueue.pop();
                }

                break;
            } else {
                // If we're still connected but send failed, remove the message to avoid getting stuck
                QUESTSYNC_LOG_WARNING("Removing failed message from queue");
                m_messageQueue.pop();
                messageCount++;
            }
        }

        // Add a larger delay between messages to avoid overwhelming the server
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Log if there are still messages in the queue
    if (shouldLog && !m_messageQueue.empty()) {
        QUESTSYNC_LOG_DEBUG("%d messages still in queue", m_messageQueue.size());
    }
}

// Send a message to the server
bool NetworkManager::SendMessage(const Message& message) {
    if (!m_client || !m_client->IsConnected()) {
        return false;
    }

    return m_client->SendMessage(message);
}

// Send a string message to the server
bool NetworkManager::SendMessage(MessageType type, const std::string& payload) {
    if (!m_client || !m_client->IsConnected()) {
        return false;
    }

    return m_client->SendMessage(type, payload);
}

// Queue a message to be sent to the server
void NetworkManager::QueueMessage(MessageType type, const std::string& payload) {
    // Only log important message types or occasionally, and only in debug mode
    static int logCounter = 0;
    bool shouldLog = GetCurrentLogLevel() == QuestSyncLogLevel::DEBUG &&
                    (type == MessageType::ERROR_MESSAGE ||
                     type == MessageType::HANDSHAKE_REQUEST ||
                     type == MessageType::HANDSHAKE_RESPONSE ||
                     logCounter++ % 100 == 0);

    if (shouldLog) {
        QUESTSYNC_LOG_DEBUG("QueueMessage - Type: %d, Payload: %s", static_cast<int>(type), payload.c_str());
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_messageQueue.push(std::make_pair(type, payload));

    if (shouldLog) {
        QUESTSYNC_LOG_DEBUG("Queue size after adding message: %d", m_messageQueue.size());
    }
}

// Get the network client
NetworkClient* NetworkManager::GetClient() {
    return m_client.get();
}

// Reset the network manager
void NetworkManager::Reset() {
    // Clear message queue
    std::lock_guard<std::mutex> lock(m_queueMutex);
    std::queue<std::pair<MessageType, std::string>> empty;
    std::swap(m_messageQueue, empty);

    // Reset quest state tracker
    if (m_questStateTracker) {
        m_questStateTracker->Reset();
    }

    QUESTSYNC_LOG_INFO("NetworkManager reset completed");
}

// Handle save game loading
void NetworkManager::HandleSaveGameLoaded() {
    QUESTSYNC_LOG_INFO("NetworkManager::HandleSaveGameLoaded - Called");

    // Check if we're already connected
    if (m_client && m_client->IsConnected()) {
        QUESTSYNC_LOG_INFO("NetworkManager::HandleSaveGameLoaded - Already connected, sending heartbeat to verify connection");

        // Send a heartbeat to verify the connection is still valid
        if (m_client->SendMessage(MessageType::HEARTBEAT, "")) {
            QUESTSYNC_LOG_INFO("NetworkManager::HandleSaveGameLoaded - Heartbeat sent successfully, maintaining connection");

            // Clear any pending messages to start fresh
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                if (!m_messageQueue.empty()) {
                    QUESTSYNC_LOG_DEBUG("NetworkManager::HandleSaveGameLoaded - Clearing %d pending messages", m_messageQueue.size());
                    std::queue<std::pair<MessageType, std::string>> empty;
                    std::swap(m_messageQueue, empty);
                }
            }

            // Connection is still good, no need to disconnect and reconnect
            return;
        }

        QUESTSYNC_LOG_WARNING("NetworkManager::HandleSaveGameLoaded - Heartbeat failed, will disconnect and reconnect");
    }

    // If we get here, we need to disconnect and reconnect
    QUESTSYNC_LOG_INFO("NetworkManager::HandleSaveGameLoaded - Disconnecting to reset connection state");
    Disconnect();

    // Reduce delay before reconnecting
    QUESTSYNC_LOG_DEBUG("NetworkManager::HandleSaveGameLoaded - Waiting before reconnecting");
    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // Further reduced from 500ms

    // Clear any pending messages to start fresh
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_messageQueue.empty()) {
            QUESTSYNC_LOG_DEBUG("NetworkManager::HandleSaveGameLoaded - Clearing %d pending messages", m_messageQueue.size());
            std::queue<std::pair<MessageType, std::string>> empty;
            std::swap(m_messageQueue, empty);
        }
    }

    // Try to reconnect
    QUESTSYNC_LOG_INFO("NetworkManager::HandleSaveGameLoaded - Attempting to reconnect");

    // Get reconnect settings from config
    Config& config = Config::GetInstance();
    int maxReconnectAttempts = config.GetInt("Network.SaveGameReconnectAttempts", 2); // Default to 2 attempts for save game loading
    int reconnectDelayMs = config.GetInt("Network.SaveGameReconnectDelay", 1000); // Default to 1 second between attempts

    QUESTSYNC_LOG_DEBUG("NetworkManager::HandleSaveGameLoaded - Using max reconnect attempts: %d, delay: %d ms",
             maxReconnectAttempts, reconnectDelayMs);

    int reconnectAttempts = 0;

    // Only try to reconnect if the server is likely to be running
    while (reconnectAttempts < maxReconnectAttempts) {
        if (Connect()) {
            QUESTSYNC_LOG_INFO("NetworkManager::HandleSaveGameLoaded - Reconnected successfully on attempt %d", reconnectAttempts + 1);
            ShowNotification("Connected to Quest Sync server");

            // The Connect method now handles the handshake, so we don't need to verify with a heartbeat
            // Just add a small delay to ensure stability
            std::this_thread::sleep_for(std::chrono::milliseconds(300));

            // Connection is good, we're done
            return;
        } else {
            QUESTSYNC_LOG_WARNING("NetworkManager::HandleSaveGameLoaded - Reconnect attempt %d failed", reconnectAttempts + 1);
        }

        reconnectAttempts++;

        // Only sleep between attempts, not after the last one
        if (reconnectAttempts < maxReconnectAttempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(reconnectDelayMs));
        }
    }

    // If we get here, all reconnect attempts failed
    QUESTSYNC_LOG_WARNING("NetworkManager::HandleSaveGameLoaded - Failed to establish a connection after %d attempts", maxReconnectAttempts);

    // Only show notification if we actually tried to connect (maxReconnectAttempts > 0)
    if (maxReconnectAttempts > 0) {
        ShowNotification("Failed to connect to Quest Sync server. Will try again later.");
    }
}

// Message received callback
void NetworkManager::OnMessageReceived(NetworkClient* client, const Message& message) {
    // Handle different message types
    switch (message.GetType()) {
        case MessageType::HEARTBEAT:
            // Respond to heartbeat
            client->SendMessage(MessageType::HEARTBEAT, "");
            break;

        case MessageType::UPDATE_QUEST:
        case MessageType::COMPLETE_QUEST:
        case MessageType::FAIL_QUEST:
        case MessageType::START_QUEST:
        case MessageType::COMPLETE_OBJECTIVE:
            // Forward to quest state tracker
            if (m_questStateTracker) {
                if (m_questStateTracker->ProcessQuestStateMessage(message)) {
                    QUESTSYNC_LOG_DEBUG("Successfully processed quest state message of type %d",
                             static_cast<int>(message.GetType()));
                } else {
                    QUESTSYNC_LOG_ERROR("Failed to process quest state message of type %d",
                             static_cast<int>(message.GetType()));
                }
            } else {
                QUESTSYNC_LOG_ERROR("Cannot process quest state message: QuestStateTracker is NULL");
            }
            break;

        case MessageType::ERROR_MESSAGE:
            // Handle error message
            {
                std::string errorMsg = message.GetPayloadAsString();
                QUESTSYNC_LOG_WARNING("Error from server: %s", errorMsg.c_str());
                ShowNotification("Error from server: " + errorMsg);
            }
            break;

        default:
            // Unknown message type
            QUESTSYNC_LOG_WARNING("Received message of type %d", static_cast<int>(message.GetType()));
            break;
    }
}

// Show a notification message in the game
void NetworkManager::ShowNotification(const std::string& message) {
    // Use Console_Print for basic notification
    Console_Print("%s", message.c_str());

    // Try multiple methods to show a toast notification

    // Method 1: Use RunScriptLine with MessageEx if console interface is available
    if (m_consoleInterface) {
        // Format the command correctly - MessageEx requires quotes around the message
        std::string scriptCmd = "MessageEx \"" + message + "\"";

        // Log the command for debugging
        QUESTSYNC_LOG_DEBUG("Running script command: %s", scriptCmd.c_str());

        // Run the script command
        m_consoleInterface->RunScriptLine(scriptCmd.c_str(), nullptr);
    } else {
        QUESTSYNC_LOG_DEBUG("Console interface is null, cannot use RunScriptLine for toast notification");
    }

    // Method 2: Use QueueUIMessage directly as a fallback
    QUESTSYNC_LOG_DEBUG("Using direct QueueUIMessage call for toast notification");
    // Parameters: message, emotion (0=happy), ddsPath, soundName, msgTime, maybeNextToDisplay
    QueueUIMessage(message.c_str(), 0, NULL, NULL, 2.0f, false);
}

// Connection status changed callback
void NetworkManager::OnConnectionStatusChanged(NetworkClient* client, bool connected) {
    if (connected) {
        QUESTSYNC_LOG_INFO("Connected to Quest Sync server");

        // Show notification in game
        ShowNotification("Connected to Quest Sync server!");
    }
    else {
        QUESTSYNC_LOG_INFO("Disconnected from Quest Sync server");

        // Show notification in game
        ShowNotification("Disconnected from Quest Sync server");
    }
}









