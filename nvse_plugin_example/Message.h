#pragma once
#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <vector>
#include <memory>
#include <array>
#include <cstdint>

// Message types
enum class MessageType : uint16_t {
    // System messages
    HANDSHAKE_REQUEST = 0,        // Client -> Server: Initial connection with version info
    HANDSHAKE_RESPONSE = 1,       // Server -> Client: Accept/reject connection
    HEARTBEAT = 2,                // Both ways: Keep connection alive
    DISCONNECT = 3,               // Both ways: Graceful disconnect

    // Quest-related messages
    QUEST_UPDATED = 100,          // Client -> Server: Quest progress updated
    QUEST_COMPLETED = 101,        // Client -> Server: Quest completed
    QUEST_FAILED = 102,           // Client -> Server: Quest failed
    QUEST_INACTIVE = 103,         // Client -> Server: Quest inactive
    NEW_QUEST = 104,              // Client -> Server: New quest acquired
    OBJECTIVE_COMPLETED = 105,    // Client -> Server: Objective completed

    // Server commands
    UPDATE_QUEST = 200,           // Server -> Client: Update quest
    COMPLETE_QUEST = 201,         // Server -> Client: Complete quest
    FAIL_QUEST = 202,             // Server -> Client: Fail quest
    INACTIVE_QUEST = 203,         // Server -> Client: Inactive quest
    START_QUEST = 204,            // Server -> Client: Start quest
    COMPLETE_OBJECTIVE = 205,     // Server -> Client: Complete objective

    // Query messages
    REQUEST_ALL_QUEST_STATES = 300,       // Client -> Server: Request all quest states
    ALL_QUEST_STATES = 301,               // Server -> Client: All quest states
    REQUEST_CURRENT_QUESTS = 302,         // Client -> Server: Request current quests
    CURRENT_ACTIVE_QUESTS = 303,          // Server -> Client: Current active quests
    REQUEST_ACTIVE_QUESTS = 304,          // Either -> Either: Request active quests
    ACTIVE_QUESTS = 305,                  // Either -> Either: Active quests

    // Error messages
    ERROR_MESSAGE = 900,          // Both ways: Error notification

    // Reserved
    RESERVED = 65535              // Reserved for future use
};

// Message header structure
struct MessageHeader {
    MessageType type;             // Message type
    uint32_t payloadSize;         // Size of the payload in bytes

    MessageHeader() : type(MessageType::RESERVED), payloadSize(0) {}
    MessageHeader(MessageType t, uint32_t size) : type(t), payloadSize(size) {}
};

// Message class
class Message {
public:
    // Default constructor
    Message();

    // Constructor with type
    explicit Message(MessageType type);

    // Constructor with type and payload
    Message(MessageType type, const std::vector<uint8_t>& payload);

    // Constructor with type and string payload
    Message(MessageType type, const std::string& payload);

    // Getters
    MessageType GetType() const { return m_header.type; }
    uint32_t GetPayloadSize() const { return m_header.payloadSize; }
    const std::vector<uint8_t>& GetPayload() const { return m_payload; }

    // Get payload as string
    std::string GetPayloadAsString() const;

    // Set payload
    void SetPayload(const std::vector<uint8_t>& payload);
    void SetPayload(const std::string& payload);

    // Serialize message to binary format
    std::vector<uint8_t> Serialize() const;

    // Deserialize from binary data
    static std::unique_ptr<Message> Deserialize(const std::vector<uint8_t>& data);
    static std::unique_ptr<Message> Deserialize(const uint8_t* data, size_t size);

private:
    MessageHeader m_header;
    std::vector<uint8_t> m_payload;
};

// Handshake request payload structure
struct HandshakeRequest {
    std::array<int, 2> clientVersion;

    HandshakeRequest() : clientVersion{0, 0} {}
    HandshakeRequest(const std::array<int, 2>& version) : clientVersion(version) {}

    // Serialize to binary
    std::vector<uint8_t> Serialize() const;

    // Deserialize from binary
    static HandshakeRequest Deserialize(const std::vector<uint8_t>& data);
};

// Handshake response payload structure
struct HandshakeResponse {
    bool accepted;
    std::string message;

    HandshakeResponse() : accepted(false), message("") {}
    HandshakeResponse(bool isAccepted, const std::string& msg) : accepted(isAccepted), message(msg) {}

    // Serialize to binary
    std::vector<uint8_t> Serialize() const;

    // Deserialize from binary
    static HandshakeResponse Deserialize(const std::vector<uint8_t>& data);
};

#endif // MESSAGE_H
