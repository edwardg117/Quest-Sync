#include "Message.h"
#include <cstring>
#include <stdexcept>

// Default constructor
Message::Message() : m_header() {}

// Constructor with type
Message::Message(MessageType type) : m_header(type, 0) {}

// Constructor with type and payload
Message::Message(MessageType type, const std::vector<uint8_t>& payload)
    : m_header(type, static_cast<uint32_t>(payload.size())), m_payload(payload) {}

// Constructor with type and string payload
Message::Message(MessageType type, const std::string& payload)
    : m_header(type, static_cast<uint32_t>(payload.size())) {
    m_payload.resize(payload.size());
    if (!payload.empty()) {
        std::copy(payload.begin(), payload.end(), m_payload.begin());
    }
}

// Constructor with type and session token
Message::Message(MessageType type, const std::string& sessionToken, bool isSessionToken)
    : m_header(type, 0, static_cast<uint32_t>(sessionToken.size())), m_sessionToken(sessionToken) {
    // isSessionToken parameter is used to distinguish this constructor from the string payload constructor
}

// Get payload as string
std::string Message::GetPayloadAsString() const {
    return std::string(reinterpret_cast<const char*>(m_payload.data()), m_payload.size());
}

// Set payload from vector
void Message::SetPayload(const std::vector<uint8_t>& payload) {
    m_payload = payload;
    m_header.payloadSize = static_cast<uint32_t>(payload.size());
}

// Set payload from string
void Message::SetPayload(const std::string& payload) {
    m_payload.resize(payload.size());
    if (!payload.empty()) {
        std::copy(payload.begin(), payload.end(), m_payload.begin());
    }
    m_header.payloadSize = static_cast<uint32_t>(payload.size());
}

// Set session token
void Message::SetSessionToken(const std::string& token) {
    m_sessionToken = token;
    m_header.sessionTokenLength = static_cast<uint32_t>(token.size());
}

// Clear session token
void Message::ClearSessionToken() {
    m_sessionToken.clear();
    m_header.sessionTokenLength = 0;
}

// Serialize message to binary format
std::vector<uint8_t> Message::Serialize() const {
    // Calculate total size: header + session token + payload
    size_t totalSize = sizeof(MessageHeader) + m_sessionToken.size() + m_payload.size();
    std::vector<uint8_t> result(totalSize);

    size_t offset = 0;

    // Copy header
    std::memcpy(result.data() + offset, &m_header, sizeof(MessageHeader));
    offset += sizeof(MessageHeader);

    // Copy session token if present
    if (!m_sessionToken.empty()) {
        std::memcpy(result.data() + offset, m_sessionToken.data(), m_sessionToken.size());
        offset += m_sessionToken.size();
    }

    // Copy payload
    if (!m_payload.empty()) {
        std::memcpy(result.data() + offset, m_payload.data(), m_payload.size());
    }

    return result;
}

// Deserialize from binary data (vector)
std::unique_ptr<Message> Message::Deserialize(const std::vector<uint8_t>& data) {
    return Deserialize(data.data(), data.size());
}

// Deserialize from binary data (pointer)
std::unique_ptr<Message> Message::Deserialize(const uint8_t* data, size_t size) {
    // Check if data is large enough to contain a header
    if (size < sizeof(MessageHeader)) {
        throw std::runtime_error("Data too small to contain a message header");
    }

    // Extract header
    const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);

    // Check if we have enough data for session token and payload
    size_t requiredSize = sizeof(MessageHeader) + header->sessionTokenLength + header->payloadSize;
    if (size < requiredSize) {
        throw std::runtime_error("Not enough data to deserialize message session token and payload");
    }

    // Create message
    auto message = std::make_unique<Message>(header->type);
    message->m_header = *header;

    size_t offset = sizeof(MessageHeader);

    // Extract session token if present
    if (header->sessionTokenLength > 0) {
        message->m_sessionToken = std::string(reinterpret_cast<const char*>(data + offset), header->sessionTokenLength);
        offset += header->sessionTokenLength;
    }

    // Extract payload if present
    if (header->payloadSize > 0) {
        message->m_payload.resize(header->payloadSize);
        std::memcpy(message->m_payload.data(), data + offset, header->payloadSize);
    }

    return message;
}

// HandshakeRequest implementation

// Serialize to binary
std::vector<uint8_t> HandshakeRequest::Serialize() const {
    // Calculate size: version (2 ints) + password length + password data
    size_t totalSize = sizeof(int) * 2 + sizeof(uint32_t) + password.size();
    std::vector<uint8_t> result(totalSize);

    // Store version components (match server implementation)
    int major = clientVersion[0];
    int minor = clientVersion[1];

    std::memcpy(result.data(), &major, sizeof(int));
    std::memcpy(result.data() + sizeof(int), &minor, sizeof(int));

    // Store password length
    uint32_t passwordLength = static_cast<uint32_t>(password.size());
    std::memcpy(result.data() + sizeof(int) * 2, &passwordLength, sizeof(uint32_t));

    // Store password content
    if (!password.empty()) {
        std::copy(password.begin(), password.end(), result.begin() + sizeof(int) * 2 + sizeof(uint32_t));
    }

    return result;
}

// Deserialize from binary
HandshakeRequest HandshakeRequest::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(int) * 2 + sizeof(uint32_t)) {
        // For backward compatibility, if we only have version data, create request without password
        if (data.size() >= sizeof(int) * 2) {
            int major, minor;
            std::memcpy(&major, data.data(), sizeof(int));
            std::memcpy(&minor, data.data() + sizeof(int), sizeof(int));
            HandshakeRequest request(std::array<int, 2>{major, minor});
            return request;
        }
        throw std::runtime_error("Not enough data to deserialize HandshakeRequest");
    }

    // Extract version components
    int major, minor;
    std::memcpy(&major, data.data(), sizeof(int));
    std::memcpy(&minor, data.data() + sizeof(int), sizeof(int));

    // Extract password length
    uint32_t passwordLength;
    std::memcpy(&passwordLength, data.data() + sizeof(int) * 2, sizeof(uint32_t));

    // Check if we have enough data for the password
    if (data.size() < sizeof(int) * 2 + sizeof(uint32_t) + passwordLength) {
        throw std::runtime_error("Not enough data to deserialize HandshakeRequest password");
    }

    // Extract password content
    std::string password;
    if (passwordLength > 0) {
        password = std::string(reinterpret_cast<const char*>(data.data() + sizeof(int) * 2 + sizeof(uint32_t)), passwordLength);
    }

    HandshakeRequest request(std::array<int, 2>{major, minor}, password);
    return request;
}

// HandshakeResponse implementation

// Serialize to binary
std::vector<uint8_t> HandshakeResponse::Serialize() const {
    // Calculate size: bool + message length + message data + token length + token data + expiry seconds
    size_t totalSize = sizeof(bool) + sizeof(uint32_t) + message.size() + sizeof(uint32_t) + sessionToken.size() + sizeof(int);
    std::vector<uint8_t> result(totalSize);

    size_t offset = 0;

    // Store accepted flag
    std::memcpy(result.data() + offset, &accepted, sizeof(bool));
    offset += sizeof(bool);

    // Store message length
    uint32_t messageLength = static_cast<uint32_t>(message.size());
    std::memcpy(result.data() + offset, &messageLength, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Store message content
    if (!message.empty()) {
        std::copy(message.begin(), message.end(), result.begin() + offset);
        offset += message.size();
    }

    // Store session token length
    uint32_t tokenLength = static_cast<uint32_t>(sessionToken.size());
    std::memcpy(result.data() + offset, &tokenLength, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Store session token content
    if (!sessionToken.empty()) {
        std::copy(sessionToken.begin(), sessionToken.end(), result.begin() + offset);
        offset += sessionToken.size();
    }

    // Store expiry seconds
    std::memcpy(result.data() + offset, &expirySeconds, sizeof(int));

    return result;
}

// Deserialize from binary
HandshakeResponse HandshakeResponse::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(bool) + sizeof(uint32_t)) {
        throw std::runtime_error("Not enough data to deserialize HandshakeResponse");
    }

    size_t offset = 0;

    // Extract accepted flag
    bool accepted;
    std::memcpy(&accepted, data.data() + offset, sizeof(bool));
    offset += sizeof(bool);

    // Extract message length
    uint32_t messageLength;
    std::memcpy(&messageLength, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Check if we have enough data for the message
    if (data.size() < offset + messageLength) {
        throw std::runtime_error("Not enough data to deserialize HandshakeResponse message");
    }

    // Extract message content
    std::string message;
    if (messageLength > 0) {
        message = std::string(reinterpret_cast<const char*>(data.data() + offset), messageLength);
        offset += messageLength;
    }

    // Check for session token (backward compatibility)
    std::string sessionToken;
    int expirySeconds = 0;
    if (data.size() >= offset + sizeof(uint32_t)) {
        // Extract session token length
        uint32_t tokenLength;
        std::memcpy(&tokenLength, data.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        // Check if we have enough data for the token
        if (data.size() >= offset + tokenLength) {
            if (tokenLength > 0) {
                sessionToken = std::string(reinterpret_cast<const char*>(data.data() + offset), tokenLength);
                offset += tokenLength;
            }

            // Check for expiry seconds (backward compatibility)
            if (data.size() >= offset + sizeof(int)) {
                std::memcpy(&expirySeconds, data.data() + offset, sizeof(int));
            }
        }
    }

    HandshakeResponse response(accepted, message, sessionToken, expirySeconds);
    return response;
}

// SessionTokenRequest implementation

// Serialize to binary
std::vector<uint8_t> SessionTokenRequest::Serialize() const {
    // Calculate size: token length + token data
    size_t totalSize = sizeof(uint32_t) + currentToken.size();
    std::vector<uint8_t> result(totalSize);

    size_t offset = 0;

    // Store token length
    uint32_t tokenLength = static_cast<uint32_t>(currentToken.size());
    std::memcpy(result.data() + offset, &tokenLength, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Store token data
    if (tokenLength > 0) {
        std::memcpy(result.data() + offset, currentToken.data(), tokenLength);
    }

    return result;
}

// Deserialize from binary
SessionTokenRequest SessionTokenRequest::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(uint32_t)) {
        throw std::runtime_error("Not enough data to deserialize SessionTokenRequest");
    }

    size_t offset = 0;

    // Extract token length
    uint32_t tokenLength;
    std::memcpy(&tokenLength, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Check if we have enough data for the token
    if (data.size() < offset + tokenLength) {
        throw std::runtime_error("Not enough data to deserialize SessionTokenRequest token");
    }

    // Extract token content
    std::string token;
    if (tokenLength > 0) {
        token = std::string(reinterpret_cast<const char*>(data.data() + offset), tokenLength);
    }

    return SessionTokenRequest(token);
}

// SessionTokenResponse implementation

// Serialize to binary
std::vector<uint8_t> SessionTokenResponse::Serialize() const {
    // Calculate size: bool + message length + message data + token length + token data + expiry seconds
    size_t totalSize = sizeof(bool) + sizeof(uint32_t) + message.size() + sizeof(uint32_t) + newToken.size() + sizeof(int);
    std::vector<uint8_t> result(totalSize);

    size_t offset = 0;

    // Store success flag
    std::memcpy(result.data() + offset, &success, sizeof(bool));
    offset += sizeof(bool);

    // Store message length
    uint32_t messageLength = static_cast<uint32_t>(message.size());
    std::memcpy(result.data() + offset, &messageLength, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Store message data
    if (messageLength > 0) {
        std::memcpy(result.data() + offset, message.data(), messageLength);
        offset += messageLength;
    }

    // Store token length
    uint32_t tokenLength = static_cast<uint32_t>(newToken.size());
    std::memcpy(result.data() + offset, &tokenLength, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Store token data
    if (tokenLength > 0) {
        std::memcpy(result.data() + offset, newToken.data(), tokenLength);
        offset += tokenLength;
    }

    // Store expiry seconds
    std::memcpy(result.data() + offset, &expirySeconds, sizeof(int));

    return result;
}

// Deserialize from binary
SessionTokenResponse SessionTokenResponse::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(bool) + sizeof(uint32_t) + sizeof(int)) {
        throw std::runtime_error("Not enough data to deserialize SessionTokenResponse");
    }

    size_t offset = 0;

    // Extract success flag
    bool success;
    std::memcpy(&success, data.data() + offset, sizeof(bool));
    offset += sizeof(bool);

    // Extract message length
    uint32_t messageLength;
    std::memcpy(&messageLength, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Check if we have enough data for the message
    if (data.size() < offset + messageLength) {
        throw std::runtime_error("Not enough data to deserialize SessionTokenResponse message");
    }

    // Extract message content
    std::string message;
    if (messageLength > 0) {
        message = std::string(reinterpret_cast<const char*>(data.data() + offset), messageLength);
        offset += messageLength;
    }

    // Extract token length
    if (data.size() < offset + sizeof(uint32_t)) {
        throw std::runtime_error("Not enough data to deserialize SessionTokenResponse token length");
    }

    uint32_t tokenLength;
    std::memcpy(&tokenLength, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Check if we have enough data for the token
    if (data.size() < offset + tokenLength) {
        throw std::runtime_error("Not enough data to deserialize SessionTokenResponse token");
    }

    // Extract token content
    std::string newToken;
    if (tokenLength > 0) {
        newToken = std::string(reinterpret_cast<const char*>(data.data() + offset), tokenLength);
        offset += tokenLength;
    }

    // Extract expiry seconds
    if (data.size() < offset + sizeof(int)) {
        throw std::runtime_error("Not enough data to deserialize SessionTokenResponse expiry seconds");
    }

    int expirySeconds;
    std::memcpy(&expirySeconds, data.data() + offset, sizeof(int));

    return SessionTokenResponse(success, message, newToken, expirySeconds);
}
