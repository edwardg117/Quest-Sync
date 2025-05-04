#include "NetworkManager.h"
#include "Config.h"
#include "nvse/PluginAPI.h"
#include <iostream>

// Singleton instance
NetworkManager& NetworkManager::GetInstance() {
    static NetworkManager instance;
    return instance;
}

// Constructor
NetworkManager::NetworkManager() : m_initialized(false) {
    m_client = std::make_unique<NetworkClient>();
}

// Initialize the network manager
bool NetworkManager::Initialize(const void* nvseInterface) {
    if (m_initialized) {
        return true;
    }

    // Load configuration
    Config& config = Config::GetInstance();
    if (!config.Load(nvseInterface, "quest_sync.ini")) {
        std::cerr << "Failed to load configuration" << std::endl;
        return false;
    }

    // Configure network client
    m_client->SetServerAddress(config.GetString("Network.ServerAddress", "127.0.0.1"));
    m_client->SetServerPort(config.GetInt("Network.ServerPort", 25575));
    
    // Set client version
    m_client->SetClientVersion(1, 0);

    // Set callbacks
    m_client->SetMessageReceivedCallback([this](NetworkClient* client, const Message& message) {
        OnMessageReceived(client, message);
    });

    m_client->SetConnectionStatusCallback([this](NetworkClient* client, bool connected) {
        OnConnectionStatusChanged(client, connected);
    });

    // Initialize network client
    if (!m_client->Initialize()) {
        std::cerr << "Failed to initialize network client" << std::endl;
        return false;
    }

    m_initialized = true;
    return true;
}

// Connect to the server
bool NetworkManager::Connect() {
    if (!m_initialized) {
        std::cerr << "Network manager not initialized" << std::endl;
        return false;
    }

    return m_client->Connect();
}

// Disconnect from the server
void NetworkManager::Disconnect() {
    if (m_client) {
        m_client->Disconnect();
    }
}

// Check if connected to the server
bool NetworkManager::IsConnected() const {
    return m_client && m_client->IsConnected();
}

// Process network messages
void NetworkManager::ProcessMessages() {
    if (m_client) {
        m_client->ProcessMessages();
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

// Get the network client
NetworkClient* NetworkManager::GetClient() {
    return m_client.get();
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
            // Handle quest update
            // This will be implemented in QuestManager
            break;

        case MessageType::COMPLETE_QUEST:
            // Handle quest completion
            // This will be implemented in QuestManager
            break;

        case MessageType::FAIL_QUEST:
            // Handle quest failure
            // This will be implemented in QuestManager
            break;

        case MessageType::START_QUEST:
            // Handle quest start
            // This will be implemented in QuestManager
            break;

        case MessageType::COMPLETE_OBJECTIVE:
            // Handle objective completion
            // This will be implemented in QuestManager
            break;

        case MessageType::ERROR_MESSAGE:
            // Handle error message
            std::cerr << "Error from server: " << message.GetPayloadAsString() << std::endl;
            break;

        default:
            // Unknown message type
            std::cout << "Received message of type " << static_cast<int>(message.GetType()) << std::endl;
            break;
    }
}

// Connection status changed callback
void NetworkManager::OnConnectionStatusChanged(NetworkClient* client, bool connected) {
    if (connected) {
        std::cout << "Connected to Quest Sync server" << std::endl;
    }
    else {
        std::cout << "Disconnected from Quest Sync server" << std::endl;
    }
}
