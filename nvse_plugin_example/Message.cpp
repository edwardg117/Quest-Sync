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
    // Check if data is large enough to contain a header
    if (size < sizeof(MessageHeader)) {
        throw std::runtime_error("Data too small to contain a message header");
    }

    // Extract header
    const MessageHeader* header = reinterpret_cast<const MessageHeader*>(data);

    // Check if data is large enough to contain the payload
    if (size < sizeof(MessageHeader) + header->payloadSize) {
        throw std::runtime_error("Data too small to contain the full message");
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

    // Store version components
    std::memcpy(result.data(), &clientVersion[0], sizeof(int));
    std::memcpy(result.data() + sizeof(int), &clientVersion[1], sizeof(int));

    return result;
}

// Deserialize from binary
HandshakeRequest HandshakeRequest::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(int) * 2) {
        throw std::runtime_error("Data too small to contain a handshake request");
    }

    HandshakeRequest request;
    std::memcpy(&request.clientVersion[0], data.data(), sizeof(int));
    std::memcpy(&request.clientVersion[1], data.data() + sizeof(int), sizeof(int));

    return request;
}

// HandshakeResponse implementation

// Serialize to binary
std::vector<uint8_t> HandshakeResponse::Serialize() const {
    // Calculate size: 1 byte for accepted + message length
    std::vector<uint8_t> result(1 + message.size() + 1); // +1 for null terminator

    // Store accepted flag
    result[0] = accepted ? 1 : 0;

    // Store message with null terminator
    if (!message.empty()) {
        std::memcpy(result.data() + 1, message.c_str(), message.size() + 1);
    }
    else {
        result[1] = '\0';
    }

    return result;
}

// Deserialize from binary
HandshakeResponse HandshakeResponse::Deserialize(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        throw std::runtime_error("Data too small to contain a handshake response");
    }

    HandshakeResponse response;
    response.accepted = data[0] != 0;

    // Extract message if present
    if (data.size() > 1) {
        response.message = reinterpret_cast<const char*>(data.data() + 1);
    }

    return response;
}
