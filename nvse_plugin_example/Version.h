#pragma once
#ifndef CLIENT_VERSION_H
#define CLIENT_VERSION_H

#include <string>
#include <array>

namespace ClientVersion {
    // Current client version (major, minor)
    // This version is sent to the server during handshake
    constexpr std::array<int, 2> Version = { 2, 0 };

    // Convert version to string for logging/display
    inline std::string VersionToString(const std::array<int, 2>& version) {
        return std::to_string(version[0]) + "." + std::to_string(version[1]);
    }

    // Get current client version as string
    inline std::string GetVersionString() {
        return VersionToString(Version);
    }
}

#endif // CLIENT_VERSION_H
