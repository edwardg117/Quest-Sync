#pragma once
#ifndef VERSION_H
#define VERSION_H

#include <string>
#include <array>

namespace Version {
    // Current server version (major, minor)
    constexpr std::array<int, 2> ServerVersion = { 1, 0 };
    
    // Minimum compatible client version
    constexpr std::array<int, 2> MinClientVersion = { 1, 0 };
    
    // Maximum compatible client version
    constexpr std::array<int, 2> MaxClientVersion = { 1, 9 };
    
    // Convert version to string
    inline std::string VersionToString(const std::array<int, 2>& version) {
        return std::to_string(version[0]) + "." + std::to_string(version[1]);
    }
    
    // Check if client version is compatible with server
    inline bool IsCompatible(const std::array<int, 2>& clientVersion) {
        // Major version must match
        if (clientVersion[0] != ServerVersion[0]) {
            return false;
        }
        
        // Minor version must be within range
        return clientVersion[1] >= MinClientVersion[1] && clientVersion[1] <= MaxClientVersion[1];
    }
}

#endif // VERSION_H
