#include "NetworkManager.h"
#include "Config.h"
#include "QuestStateTracker.h"
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
      m_questStateTracker(nullptr) {
    m_client = std::make_unique<NetworkClient>();
}

// Initialize the network manager
bool NetworkManager::Initialize(const void* nvseInterface, NVSEConsoleInterface* consoleInterface) {
    _MESSAGE("NetworkManager::Initialize - Called");

    if (m_initialized) {
        _MESSAGE("NetworkManager::Initialize - Already initialized");
        return true;
    }

    try {
        // Store console interface
        m_consoleInterface = consoleInterface;

        // Load configuration
        Config& config = Config::GetInstance();

        // Try to load the config file, if it fails, continue with defaults
        if (!config.Load(nvseInterface, "QuestSync.ini")) {
            _MESSAGE("NetworkManager::Initialize - Failed to load config file, using defaults");
        }

        std::string serverAddress = config.GetString("Network.ServerAddress", "127.0.0.1");
        int serverPort = config.GetInt("Network.ServerPort", 25575);

        _MESSAGE("NetworkManager::Initialize - Server address: %s, port: %d", serverAddress.c_str(), serverPort);

        // Initialize quest state tracker first to ensure it's available even if networking fails
        m_questStateTracker = &QuestStateTracker::GetInstance();
        if (!m_questStateTracker->Initialize(consoleInterface)) {
            _MESSAGE("NetworkManager::Initialize - Failed to initialize quest state tracker");
            return false;
        }

        // Set the network manager in the quest state tracker
        m_questStateTracker->SetNetworkManager(this);
        _MESSAGE("NetworkManager::Initialize - Quest state tracker initialized and linked");

        // Create network client
        m_client = std::make_unique<NetworkClient>(serverAddress, serverPort);

        // Initialize client
        if (!m_client->Initialize()) {
            _MESSAGE("NetworkManager::Initialize - Failed to initialize network client");

            // Continue without network functionality rather than failing completely
            _MESSAGE("NetworkManager::Initialize - Continuing without network functionality");
            ShowNotification("Quest Sync network functionality disabled");

            m_initialized = true;
            return true;
        }

        // Get debug mode setting
        bool debugMode = config.GetBool("Network.DebugMode", false); // Default to false for release
        if (debugMode) {
            _MESSAGE("NetworkManager::Initialize - Debug mode enabled");
            m_client->SetDebugMode(true);
        } else {
            _MESSAGE("NetworkManager::Initialize - Debug mode disabled");
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
        _MESSAGE("NetworkManager::Initialize - Initialized successfully");

        return true;
    }
    catch (const std::exception& e) {
        _MESSAGE("NetworkManager::Initialize - Exception: %s", e.what());

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
                    _MESSAGE("NetworkManager::Initialize - Quest state tracker initialized in fallback mode");
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
            _MESSAGE("NetworkManager::Initialize - Failed to initialize quest state tracker in fallback mode: %s", e.what());
        }
        catch (...) {
            _MESSAGE("NetworkManager::Initialize - Failed to initialize quest state tracker in fallback mode");
        }

        return false;
    }
    catch (...) {
        _MESSAGE("NetworkManager::Initialize - Unknown exception");

        // Show a notification to the user
        if (consoleInterface) {
            ShowNotification("Quest Sync initialization error: Unknown exception");
        }

        // Still initialize the quest state tracker to allow the game to run without network functionality
        try {
            if (!m_questStateTracker) {
                m_questStateTracker = &QuestStateTracker::GetInstance();
                if (m_questStateTracker->Initialize(consoleInterface)) {
                    _MESSAGE("NetworkManager::Initialize - Quest state tracker initialized in fallback mode");
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
            _MESSAGE("NetworkManager::Initialize - Failed to initialize quest state tracker in fallback mode");
        }

        return false;
    }
}

// Connect to the server
bool NetworkManager::Connect() {
    _MESSAGE("NetworkManager::Connect - Called");

    if (!m_initialized) {
        _MESSAGE("NetworkManager::Connect - Not initialized");
        return false;
    }

    // Check if client is valid
    if (!m_client) {
        _MESSAGE("NetworkManager::Connect - Client is null");
        return false;
    }

    // Check if client is initialized
    if (!m_client->IsInitialized()) {
        _MESSAGE("NetworkManager::Connect - Client is not initialized");
        return false;
    }

    try {
        _MESSAGE("NetworkManager::Connect - Attempting to connect to server");
        bool result = m_client->Connect();
        _MESSAGE("NetworkManager::Connect - Connection attempt result: %s", result ? "success" : "failure");

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
        _MESSAGE("NetworkManager::Connect - Exception during connect: %s", e.what());

        // Show a notification to the user
        std::string errorMsg = "Quest Sync connection error: " + std::string(e.what());
        ShowNotification(errorMsg);

        return false;
    }
    catch (...) {
        _MESSAGE("NetworkManager::Connect - Unknown exception during connect");

        // Show a notification to the user
        ShowNotification("Quest Sync connection error: Unknown exception");

        return false;
    }
}

// Disconnect from the server
void NetworkManager::Disconnect() {
    _MESSAGE("NetworkManager::Disconnect - Called");

    // Clear message queue first to avoid sending messages during disconnect
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<std::pair<MessageType, std::string>> empty;
        std::swap(m_messageQueue, empty);
        _MESSAGE("NetworkManager::Disconnect - Message queue cleared");
    }

    // Disconnect client
    if (m_client) {
        _MESSAGE("NetworkManager::Disconnect - Disconnecting client");
        try {
            m_client->Disconnect();
            _MESSAGE("NetworkManager::Disconnect - Client disconnected successfully");
        }
        catch (const std::exception& e) {
            _MESSAGE("NetworkManager::Disconnect - Exception during client disconnect: %s", e.what());
        }
        catch (...) {
            _MESSAGE("NetworkManager::Disconnect - Unknown exception during client disconnect");
        }
    } else {
        _MESSAGE("NetworkManager::Disconnect - Client is null");
    }
}

// Check if connected to the server
bool NetworkManager::IsConnected() const {
    bool connected = m_client && m_client->IsConnected();

    // Only log occasionally to avoid filling the log file
    static int logCounter = 0;
    if (logCounter++ % 1000 == 0) {
        _MESSAGE("NetworkManager::IsConnected - Status: %s", connected ? "connected" : "not connected");
    }

    return connected;
}

// Process network messages
void NetworkManager::ProcessMessages() {
    static int counter = 0;
    counter++;

    // Only log every 1000 frames to avoid log spam
    bool shouldLog = (counter % 1000 == 0);

    if (!m_initialized) {
        if (shouldLog) {
            _MESSAGE("NetworkManager::ProcessMessages - Not initialized");
        }
        return;
    }

    if (!m_client) {
        if (shouldLog) {
            _MESSAGE("NetworkManager::ProcessMessages - Client is null");
        }
        return;
    }

    // Check if client is initialized
    if (!m_client->IsInitialized()) {
        if (shouldLog) {
            _MESSAGE("NetworkManager::ProcessMessages - Client is not initialized");
        }
        return;
    }

    try {
        // Check connection status
        bool isConnected = m_client->IsConnected();

        // Only log connection status changes or occasionally
        static bool lastConnectionStatus = false;
        if (lastConnectionStatus != isConnected || shouldLog) {
            _MESSAGE("NetworkManager::ProcessMessages - Connection status: %s", isConnected ? "connected" : "not connected");
            lastConnectionStatus = isConnected;
        }

        if (!isConnected) {
            // Try to reconnect if needed
            static std::chrono::steady_clock::time_point lastReconnectAttempt = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastReconnectAttempt).count();

            // Get reconnect interval from config
            Config& config = Config::GetInstance();
            int reconnectInterval = config.GetInt("Network.ReconnectInterval", 60);

            // Try to reconnect based on the configured interval
            if (elapsed >= reconnectInterval) {
                _MESSAGE("NetworkManager::ProcessMessages - Attempting to reconnect (interval: %d seconds)", reconnectInterval);
                lastReconnectAttempt = now;

                // Only show notification on first reconnect attempt
                static bool firstReconnectAttempt = true;
                if (firstReconnectAttempt) {
                    ShowNotification("Attempting to connect to Quest Sync server...");
                    firstReconnectAttempt = false;
                }

                if (Connect()) {
                    _MESSAGE("NetworkManager::ProcessMessages - Reconnected successfully");
                    firstReconnectAttempt = true; // Reset for next time
                } else {
                    _MESSAGE("NetworkManager::ProcessMessages - Reconnection failed");
                }
            }

            return;
        }

        // Let the client process its messages
        m_client->ProcessMessages();

        // Process outgoing messages from our queue
        ProcessQueuedMessages();
    }
    catch (const std::exception& e) {
        _MESSAGE("NetworkManager::ProcessMessages - Exception: %s", e.what());

        // Only show notification occasionally to avoid spamming the user
        static int errorCounter = 0;
        if (errorCounter++ % 100 == 0) {
            ShowNotification("Quest Sync error: " + std::string(e.what()));
        }
    }
    catch (...) {
        _MESSAGE("NetworkManager::ProcessMessages - Unknown exception");

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
        // Only log disconnected state occasionally
        if (logCounter++ % 1000 == 0) {
            _MESSAGE("NetworkManager::ProcessQueuedMessages - Not connected, cannot process messages");
        }
        return;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);

    // Only log when there are messages or occasionally
    if (!m_messageQueue.empty() || logCounter++ % 1000 == 0) {
        shouldLog = true;
    }

    if (shouldLog && !m_messageQueue.empty()) {
        _MESSAGE("NetworkManager: Queue size: %d", m_messageQueue.size());
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
            _MESSAGE("NetworkManager: Processing message type %d", static_cast<int>(type));
        }

        // Send the message
        bool success = false;
        try {
            success = SendMessage(type, payload);
            if (shouldLog) {
                _MESSAGE("NetworkManager: Message send %s", success ? "successful" : "failed");
            }
        }
        catch (const std::exception& e) {
            _MESSAGE("NetworkManager: Exception during send: %s", e.what());
            success = false;
        }
        catch (...) {
            _MESSAGE("NetworkManager: Unknown exception during send");
            success = false;
        }

        // Only remove the message from the queue if it was sent successfully
        if (success) {
            m_messageQueue.pop();
            messageCount++;
        } else {
            // Check if we're still connected
            if (!IsConnected()) {
                _MESSAGE("NetworkManager: Connection lost during message processing");
                connectionLost = true;

                // Don't remove the message from the queue so we can retry after reconnecting
                // But limit how many messages we keep to avoid memory issues
                if (m_messageQueue.size() > 20) {
                    _MESSAGE("NetworkManager: Queue too large, removing oldest message");
                    m_messageQueue.pop();
                }

                break;
            } else {
                // If we're still connected but send failed, remove the message to avoid getting stuck
                _MESSAGE("NetworkManager: Removing failed message from queue");
                m_messageQueue.pop();
                messageCount++;
            }
        }

        // Add a larger delay between messages to avoid overwhelming the server
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Log if there are still messages in the queue
    if (shouldLog && !m_messageQueue.empty()) {
        _MESSAGE("NetworkManager: %d messages still in queue", m_messageQueue.size());
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
    // Only log important message types or occasionally
    static int logCounter = 0;
    bool shouldLog = (type == MessageType::ERROR_MESSAGE ||
                     type == MessageType::HANDSHAKE_REQUEST ||
                     type == MessageType::HANDSHAKE_RESPONSE ||
                     logCounter++ % 100 == 0);

    if (shouldLog) {
        _MESSAGE("NetworkManager::QueueMessage - Type: %d, Payload: %s", static_cast<int>(type), payload.c_str());
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_messageQueue.push(std::make_pair(type, payload));

    if (shouldLog) {
        _MESSAGE("NetworkManager: Queue size after adding message: %d", m_messageQueue.size());
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

    _MESSAGE("NetworkManager: Reset completed");
}

// Handle save game loading
void NetworkManager::HandleSaveGameLoaded() {
    _MESSAGE("NetworkManager::HandleSaveGameLoaded - Called");

    // Always disconnect and reconnect to ensure a clean connection state
    _MESSAGE("NetworkManager::HandleSaveGameLoaded - Disconnecting to reset connection state");
    Disconnect();

    // Add a delay before reconnecting to allow the server to clean up the old connection
    _MESSAGE("NetworkManager::HandleSaveGameLoaded - Waiting before reconnecting");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Clear any pending messages to start fresh
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        if (!m_messageQueue.empty()) {
            _MESSAGE("NetworkManager::HandleSaveGameLoaded - Clearing %d pending messages", m_messageQueue.size());
            std::queue<std::pair<MessageType, std::string>> empty;
            std::swap(m_messageQueue, empty);
        }
    }

    // Try to reconnect
    _MESSAGE("NetworkManager::HandleSaveGameLoaded - Attempting to reconnect");
    int reconnectAttempts = 0;
    const int maxReconnectAttempts = 3;

    while (reconnectAttempts < maxReconnectAttempts) {
        if (Connect()) {
            _MESSAGE("NetworkManager::HandleSaveGameLoaded - Reconnected successfully on attempt %d", reconnectAttempts + 1);
            ShowNotification("Connected to Quest Sync server");

            // The Connect method now handles the handshake, so we don't need to verify with a heartbeat
            // Just add a delay to ensure stability
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // Connection is good, we're done
            return;
        } else {
            _MESSAGE("NetworkManager::HandleSaveGameLoaded - Reconnect attempt %d failed", reconnectAttempts + 1);
        }

        reconnectAttempts++;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // If we get here, all reconnect attempts failed
    _MESSAGE("NetworkManager::HandleSaveGameLoaded - Failed to establish a stable connection after %d attempts", maxReconnectAttempts);
    ShowNotification("Failed to connect to Quest Sync server. Will try again later.");
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
                    _MESSAGE("Successfully processed quest state message of type %d",
                             static_cast<int>(message.GetType()));
                } else {
                    _MESSAGE("Failed to process quest state message of type %d",
                             static_cast<int>(message.GetType()));
                }
            } else {
                _MESSAGE("Cannot process quest state message: QuestStateTracker is NULL");
            }
            break;

        case MessageType::ERROR_MESSAGE:
            // Handle error message
            {
                std::string errorMsg = message.GetPayloadAsString();
                _MESSAGE("Error from server: %s", errorMsg.c_str());
                ShowNotification("Error from server: " + errorMsg);
            }
            break;

        default:
            // Unknown message type
            _MESSAGE("Received message of type %d", static_cast<int>(message.GetType()));
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
        _MESSAGE("Running script command: %s", scriptCmd.c_str());

        // Run the script command
        m_consoleInterface->RunScriptLine(scriptCmd.c_str(), nullptr);
    } else {
        _MESSAGE("Console interface is null, cannot use RunScriptLine for toast notification");
    }

    // Method 2: Use QueueUIMessage directly as a fallback
    _MESSAGE("Using direct QueueUIMessage call for toast notification");
    // Parameters: message, emotion (0=happy), ddsPath, soundName, msgTime, maybeNextToDisplay
    QueueUIMessage(message.c_str(), 0, NULL, NULL, 2.0f, false);
}

// Connection status changed callback
void NetworkManager::OnConnectionStatusChanged(NetworkClient* client, bool connected) {
    if (connected) {
        _MESSAGE("Connected to Quest Sync server");

        // Show notification in game
        ShowNotification("Connected to Quest Sync server!");
    }
    else {
        _MESSAGE("Disconnected from Quest Sync server");

        // Show notification in game
        ShowNotification("Disconnected from Quest Sync server");
    }
}







