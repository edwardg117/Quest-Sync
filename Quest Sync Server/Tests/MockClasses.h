#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <WS2tcpip.h>
#include <gtest/gtest.h>

#include "Message.h"

// Simple mock file system for testing file operations
class MockFileSystem {
public:
    bool FileExists(const std::string& filename) { return true; }
    bool ReadFile(const std::string& filename, std::string& content) { content = "test"; return true; }
    bool WriteFile(const std::string& filename, const std::string& content) { return true; }
    bool CreateDirectory(const std::string& path) { return true; }

    static MockFileSystem& GetInstance() {
        static MockFileSystem instance;
        return instance;
    }
};

// Simple mock socket for testing network operations
class MockSocket {
public:
    SOCKET Socket(int af, int type, int protocol) { return 1; }
    int Bind(SOCKET s, const struct sockaddr* name, int namelen) { return 0; }
    int Listen(SOCKET s, int backlog) { return 0; }
    SOCKET Accept(SOCKET s, struct sockaddr* addr, int* addrlen) { return 2; }
    int Connect(SOCKET s, const struct sockaddr* name, int namelen) { return 0; }
    int Send(SOCKET s, const char* buf, int len, int flags) { return len; }
    int Recv(SOCKET s, char* buf, int len, int flags) { return len; }
    int Closesocket(SOCKET s) { return 0; }
    int Select(int nfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds, struct timeval* timeout) { return 0; }
    int IoctlSocket(SOCKET s, long cmd, u_long* argp) { return 0; }

    static MockSocket& GetInstance() {
        static MockSocket instance;
        return instance;
    }
};

// Simple mock console for testing command processor
class MockConsole {
public:
    void Print(const std::string& message) {}
    std::string ReadLine() { return "test"; }
    bool IsAvailable() { return true; }

    static MockConsole& GetInstance() {
        static MockConsole instance;
        return instance;
    }
};

// Simple mock TCP server for testing command processor
class MockTCPServer {
public:
    bool Initialize() { return true; }
    bool Start() { return true; }
    void Stop() {}
    bool IsRunning() { return true; }
    size_t GetClientCount() { return 5; }
    void BroadcastMessage(const Message& message) {}
    void BroadcastText(const std::string& text, SOCKET excludeSocket = INVALID_SOCKET) {}
    void SendMessage(SOCKET clientSocket, const Message& message) {}
    bool KickClient(SOCKET clientSocket) { return clientSocket == 1; }

    std::vector<std::tuple<SOCKET, std::string, bool>> GetClientInfo() {
        std::vector<std::tuple<SOCKET, std::string, bool>> clients;
        clients.emplace_back(1, "192.168.1.1:12345", true);
        clients.emplace_back(2, "192.168.1.2:54321", false);
        return clients;
    }

    static MockTCPServer& GetInstance() {
        static MockTCPServer instance;
        return instance;
    }
};

// Utility functions for testing
namespace TestUtils {
    // Create a temporary file with the given content
    std::string CreateTempFile(const std::string& content);

    // Delete a temporary file
    void DeleteTempFile(const std::string& filename);

    // Get a random port number for testing
    int GetRandomPort();
}
