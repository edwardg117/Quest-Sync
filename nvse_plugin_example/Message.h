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
    ERROR_MESSAGE = 4,            // Both ways: Error message

    // Quest sync messages
    UPDATE_QUEST = 5,             // Server -> Client: Update quest stage
    COMPLETE_QUEST = 6,           // Server -> Client: Complete quest
    FAIL_QUEST = 7,               // Server -> Client: Fail quest
    START_QUEST = 8,              // Server -> Client: Start quest
    COMPLETE_OBJECTIVE = 9,       // Server -> Client: Complete objective

    // Session management messages
    SESSION_TOKEN_REQUEST = 10,   // Client -> Server: Request new session token
    SESSION_TOKEN_RESPONSE = 11,  // Server -> Client: Session token response

    // New message types
    OBJECTIVE_UPDATE = 20,        // Client -> Server -> Other Clients: Objective state update

    // Reserved for future use
    RESERVED = 65535              // Reserved for future use
};

// Message header structure
struct MessageHeader {
    MessageType type;             // Message type
    uint32_t payloadSize;         // Size of the payload in bytes
    uint32_t sessionTokenLength;  // Length of session token (0 if no token)
    // Session token data follows immediately after header if sessionTokenLength > 0

    MessageHeader() : type(MessageType::RESERVED), payloadSize(0), sessionTokenLength(0) {}
    MessageHeader(MessageType t, uint32_t size) : type(t), payloadSize(size), sessionTokenLength(0) {}
    MessageHeader(MessageType t, uint32_t size, uint32_t tokenLen) : type(t), payloadSize(size), sessionTokenLength(tokenLen) {}
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

    // Constructor with type and session token
    Message(MessageType type, const std::string& sessionToken, bool isSessionToken);

    // Getters
    MessageType GetType() const { return m_header.type; }
    uint32_t GetPayloadSize() const { return m_header.payloadSize; }
    const std::vector<uint8_t>& GetPayload() const { return m_payload; }
    std::string GetSessionToken() const { return m_sessionToken; }
    bool HasSessionToken() const { return !m_sessionToken.empty(); }

    // Get payload as string
    std::string GetPayloadAsString() const;

    // Set payload
    void SetPayload(const std::vector<uint8_t>& payload);
    void SetPayload(const std::string& payload);

    // Session token methods
    void SetSessionToken(const std::string& token);
    void ClearSessionToken();

    // Serialize message to binary format
    std::vector<uint8_t> Serialize() const;

    // Deserialize from binary data
    static std::unique_ptr<Message> Deserialize(const std::vector<uint8_t>& data);
    static std::unique_ptr<Message> Deserialize(const uint8_t* data, size_t size);

private:
    MessageHeader m_header;
    std::vector<uint8_t> m_payload;
    std::string m_sessionToken;
};

// Handshake request payload structure
struct HandshakeRequest {
    std::array<int, 2> clientVersion;
    std::string password;

    HandshakeRequest() : clientVersion{0, 0}, password("") {}
    HandshakeRequest(const std::array<int, 2>& version) : clientVersion(version), password("") {}
    HandshakeRequest(const std::array<int, 2>& version, const std::string& pwd) : clientVersion(version), password(pwd) {}

    // Serialize to binary
    std::vector<uint8_t> Serialize() const;

    // Deserialize from binary
    static HandshakeRequest Deserialize(const std::vector<uint8_t>& data);
};

// Handshake response payload structure
struct HandshakeResponse {
    bool accepted;
    std::string message;
    std::string sessionToken;
    int expirySeconds;

    HandshakeResponse() : accepted(false), message(""), sessionToken(""), expirySeconds(0) {}
    HandshakeResponse(bool isAccepted, const std::string& msg) : accepted(isAccepted), message(msg), sessionToken(""), expirySeconds(0) {}
    HandshakeResponse(bool isAccepted, const std::string& msg, const std::string& token)
        : accepted(isAccepted), message(msg), sessionToken(token), expirySeconds(0) {}
    HandshakeResponse(bool isAccepted, const std::string& msg, const std::string& token, int expiry)
        : accepted(isAccepted), message(msg), sessionToken(token), expirySeconds(expiry) {}

    // Serialize to binary
    std::vector<uint8_t> Serialize() const;

    // Deserialize from binary
    static HandshakeResponse Deserialize(const std::vector<uint8_t>& data);
};

// Session token request payload structure
struct SessionTokenRequest {
    std::string currentToken;

    SessionTokenRequest() : currentToken("") {}
    SessionTokenRequest(const std::string& token) : currentToken(token) {}

    // Serialize to binary
    std::vector<uint8_t> Serialize() const;

    // Deserialize from binary
    static SessionTokenRequest Deserialize(const std::vector<uint8_t>& data);
};

// Session token response payload structure
struct SessionTokenResponse {
    bool success;
    std::string message;
    std::string newToken;
    int expirySeconds;

    SessionTokenResponse() : success(false), message(""), newToken(""), expirySeconds(0) {}
    SessionTokenResponse(bool isSuccess, const std::string& msg) : success(isSuccess), message(msg), newToken(""), expirySeconds(0) {}
    SessionTokenResponse(bool isSuccess, const std::string& msg, const std::string& token)
        : success(isSuccess), message(msg), newToken(token), expirySeconds(0) {}
    SessionTokenResponse(bool isSuccess, const std::string& msg, const std::string& token, int expiry)
        : success(isSuccess), message(msg), newToken(token), expirySeconds(expiry) {}

    // Serialize to binary
    std::vector<uint8_t> Serialize() const;

    // Deserialize from binary
    static SessionTokenResponse Deserialize(const std::vector<uint8_t>& data);
};

#endif // MESSAGE_H



