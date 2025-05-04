#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <algorithm>

#include "Message.h"
#include "Version.h"

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Nothing to set up
    }

    void TearDown() override {
        // Nothing to tear down
    }

    // Helper function to compare two binary vectors
    bool CompareVectors(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
        if (a.size() != b.size()) {
            return false;
        }
        return std::equal(a.begin(), a.end(), b.begin());
    }
};

// Test default constructor
TEST_F(MessageTest, DefaultConstructor) {
    Message message;
    EXPECT_EQ(message.GetType(), MessageType::RESERVED);
    EXPECT_EQ(message.GetPayloadSize(), 0);
    EXPECT_TRUE(message.GetPayload().empty());
    EXPECT_TRUE(message.GetPayloadAsString().empty());
}

// Test constructor with type
TEST_F(MessageTest, TypeConstructor) {
    Message message(MessageType::HANDSHAKE_REQUEST);
    EXPECT_EQ(message.GetType(), MessageType::HANDSHAKE_REQUEST);
    EXPECT_EQ(message.GetPayloadSize(), 0);
    EXPECT_TRUE(message.GetPayload().empty());
    EXPECT_TRUE(message.GetPayloadAsString().empty());
}

// Test constructor with type and binary payload
TEST_F(MessageTest, BinaryPayloadConstructor) {
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    Message message(MessageType::HANDSHAKE_REQUEST, payload);
    EXPECT_EQ(message.GetType(), MessageType::HANDSHAKE_REQUEST);
    EXPECT_EQ(message.GetPayloadSize(), payload.size());
    EXPECT_TRUE(CompareVectors(message.GetPayload(), payload));
}

// Test constructor with type and string payload
TEST_F(MessageTest, StringPayloadConstructor) {
    std::string payload = "Hello, world!";
    Message message(MessageType::HANDSHAKE_REQUEST, payload);
    EXPECT_EQ(message.GetType(), MessageType::HANDSHAKE_REQUEST);
    EXPECT_EQ(message.GetPayloadSize(), payload.size());
    EXPECT_EQ(message.GetPayloadAsString(), payload);
}

// Test setting binary payload
TEST_F(MessageTest, SetBinaryPayload) {
    Message message(MessageType::HANDSHAKE_REQUEST);
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    message.SetPayload(payload);
    EXPECT_EQ(message.GetPayloadSize(), payload.size());
    EXPECT_TRUE(CompareVectors(message.GetPayload(), payload));
}

// Test setting string payload
TEST_F(MessageTest, SetStringPayload) {
    Message message(MessageType::HANDSHAKE_REQUEST);
    std::string payload = "Hello, world!";
    message.SetPayload(payload);
    EXPECT_EQ(message.GetPayloadSize(), payload.size());
    EXPECT_EQ(message.GetPayloadAsString(), payload);
}

// Test serialization and deserialization
TEST_F(MessageTest, SerializeDeserialize) {
    // Create a message with a payload
    std::string payload = "Hello, world!";
    Message originalMessage(MessageType::HANDSHAKE_REQUEST, payload);

    // Serialize the message
    std::vector<uint8_t> serialized = originalMessage.Serialize();

    // Deserialize the message
    std::unique_ptr<Message> deserializedMessage = Message::Deserialize(serialized);

    // Check if the deserialized message matches the original
    ASSERT_NE(deserializedMessage, nullptr);
    EXPECT_EQ(deserializedMessage->GetType(), originalMessage.GetType());
    EXPECT_EQ(deserializedMessage->GetPayloadSize(), originalMessage.GetPayloadSize());
    EXPECT_EQ(deserializedMessage->GetPayloadAsString(), originalMessage.GetPayloadAsString());
}

// Test serialization and deserialization with empty payload
TEST_F(MessageTest, SerializeDeserializeEmptyPayload) {
    // Create a message with an empty payload
    Message originalMessage(MessageType::HANDSHAKE_REQUEST);

    // Serialize the message
    std::vector<uint8_t> serialized = originalMessage.Serialize();

    // Deserialize the message
    std::unique_ptr<Message> deserializedMessage = Message::Deserialize(serialized);

    // Check if the deserialized message matches the original
    ASSERT_NE(deserializedMessage, nullptr);
    EXPECT_EQ(deserializedMessage->GetType(), originalMessage.GetType());
    EXPECT_EQ(deserializedMessage->GetPayloadSize(), originalMessage.GetPayloadSize());
    EXPECT_TRUE(deserializedMessage->GetPayload().empty());
}

// Test serialization and deserialization with binary payload
TEST_F(MessageTest, SerializeDeserializeBinaryPayload) {
    // Create a message with a binary payload
    std::vector<uint8_t> payload = {1, 2, 3, 4, 5};
    Message originalMessage(MessageType::HANDSHAKE_REQUEST, payload);

    // Serialize the message
    std::vector<uint8_t> serialized = originalMessage.Serialize();

    // Deserialize the message
    std::unique_ptr<Message> deserializedMessage = Message::Deserialize(serialized);

    // Check if the deserialized message matches the original
    ASSERT_NE(deserializedMessage, nullptr);
    EXPECT_EQ(deserializedMessage->GetType(), originalMessage.GetType());
    EXPECT_EQ(deserializedMessage->GetPayloadSize(), originalMessage.GetPayloadSize());
    EXPECT_TRUE(CompareVectors(deserializedMessage->GetPayload(), payload));
}

// Test deserialization with invalid data
TEST_F(MessageTest, DeserializeInvalidData) {
    // Create invalid serialized data (too short)
    std::vector<uint8_t> invalidData = {1, 2, 3};

    // Try to deserialize the invalid data
    std::unique_ptr<Message> deserializedMessage = Message::Deserialize(invalidData);

    // Check if deserialization failed
    EXPECT_EQ(deserializedMessage, nullptr);
}

// Test handshake request payload
TEST_F(MessageTest, HandshakeRequestPayload) {
    // Create a handshake request with version 1.0
    HandshakeRequest request({1, 0});

    // Serialize the request
    std::vector<uint8_t> serialized = request.Serialize();

    // Deserialize the request
    HandshakeRequest deserializedRequest = HandshakeRequest::Deserialize(serialized);

    // Check if the deserialized request matches the original
    EXPECT_EQ(deserializedRequest.clientVersion[0], 1);
    EXPECT_EQ(deserializedRequest.clientVersion[1], 0);
}

// Test handshake response payload
TEST_F(MessageTest, HandshakeResponsePayload) {
    // Create a handshake response
    HandshakeResponse response(true, "Test message");

    // Serialize the response
    std::vector<uint8_t> serialized = response.Serialize();

    // Deserialize the response
    HandshakeResponse deserializedResponse = HandshakeResponse::Deserialize(serialized);

    // Check if the deserialized response matches the original
    EXPECT_EQ(deserializedResponse.accepted, true);
    EXPECT_EQ(deserializedResponse.message, "Test message");
}
