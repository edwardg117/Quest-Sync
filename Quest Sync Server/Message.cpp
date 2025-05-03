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

// Serialize message to binary format
std::vector<uint8_t> Message::Serialize() const {
    // Calculate total size: header + payload
    size_t totalSize = sizeof(MessageHeader) + m_payload.size();
    std::vector<uint8_t> result(totalSize);

    // Copy header
    std::memcpy(result.data(), &m_header, sizeof(MessageHeader));

    // Copy payload
    if (!m_payload.empty()) {
        std::memcpy(result.data() + sizeof(MessageHeader), m_payload.data(), m_payload.size());
    }

    return result;
}

// Deserialize from binary data (vector)
std::unique_ptr<Message> Message::Deserialize(const std::vector<uint8_t>& data) {
    return Deserialize(data.data(), data.size());
}

// Deserialize from binary data (pointer)
std::unique_ptr<Message> Message::Deserialize(const uint8_t* data, size_t size) {
    // Check if we have enough data for the header
    if (size < sizeof(MessageHeader)) {
        throw std::runtime_error("Not enough data to deserialize message header");
    }

    // Extract header
    const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);

    // Check if we have enough data for the payload
    if (size < sizeof(MessageHeader) + header->payloadSize) {
        throw std::runtime_error("Not enough data to deserialize message payload");
    }

    // Create message
    auto message = std::make_unique<Message>(header->type);

    // Extract payload if present
    if (header->payloadSize > 0) {
        message->m_payload.resize(header->payloadSize);
        std::memcpy(message->m_payload.data(), data + sizeof(MessageHeader), header->payloadSize);
    }

    return message;
}

// HandshakeRequest implementation

// Serialize to binary
std::vector<uint8_t> HandshakeRequest::Serialize() const {
    std::vector<uint8_t> result(sizeof(int) * 2);

    // Store version components (network byte order)
    int major = clientVersion[0];
    int minor = clientVersion[1];

    std::memcpy(result.data(), &major, sizeof(int));
    std::memcpy(result.data() + sizeof(int), &minor, sizeof(int));

    return result;
}

// Deserialize from binary
HandshakeRequest HandshakeRequest::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(int) * 2) {
        throw std::runtime_error("Not enough data to deserialize HandshakeRequest");
    }

    // Extract version components
    int major, minor;
    std::memcpy(&major, data.data(), sizeof(int));
    std::memcpy(&minor, data.data() + sizeof(int), sizeof(int));

    HandshakeRequest request(std::array<int, 2>{major, minor});
    return request;
}

// HandshakeResponse implementation

// Serialize to binary
std::vector<uint8_t> HandshakeResponse::Serialize() const {
    // Calculate size: bool + string length + string data
    size_t totalSize = sizeof(bool) + sizeof(uint32_t) + message.size();
    std::vector<uint8_t> result(totalSize);

    // Store accepted flag
    std::memcpy(result.data(), &accepted, sizeof(bool));

    // Store message length
    uint32_t messageLength = static_cast<uint32_t>(message.size());
    std::memcpy(result.data() + sizeof(bool), &messageLength, sizeof(uint32_t));

    // Store message content
    if (!message.empty()) {
        std::copy(message.begin(), message.end(), result.begin() + sizeof(bool) + sizeof(uint32_t));
    }

    return result;
}

// Deserialize from binary
HandshakeResponse HandshakeResponse::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(bool) + sizeof(uint32_t)) {
        throw std::runtime_error("Not enough data to deserialize HandshakeResponse");
    }

    // Extract accepted flag
    bool accepted;
    std::memcpy(&accepted, data.data(), sizeof(bool));

    // Extract message length
    uint32_t messageLength;
    std::memcpy(&messageLength, data.data() + sizeof(bool), sizeof(uint32_t));

    // Check if we have enough data for the message
    if (data.size() < sizeof(bool) + sizeof(uint32_t) + messageLength) {
        throw std::runtime_error("Not enough data to deserialize HandshakeResponse message");
    }

    // Extract message content
    std::string message;
    if (messageLength > 0) {
        // Create a string from the data directly
        message = std::string(reinterpret_cast<const char*>(data.data() + sizeof(bool) + sizeof(uint32_t)), messageLength);
    }

    HandshakeResponse response(accepted, message);

    return response;
}
