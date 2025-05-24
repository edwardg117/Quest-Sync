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

    // Compatibility check result
    enum class CompatibilityResult {
        Compatible,
        MajorVersionMismatch,
        MinorVersionTooOld,
        MinorVersionTooNew
    };

    // Get detailed compatibility result
    inline CompatibilityResult GetCompatibilityResult(const std::array<int, 2>& clientVersion) {
        // Major version must match
        if (clientVersion[0] != ServerVersion[0]) {
            return CompatibilityResult::MajorVersionMismatch;
        }

        // Check minor version range
        if (clientVersion[1] < MinClientVersion[1]) {
            return CompatibilityResult::MinorVersionTooOld;
        }

        if (clientVersion[1] > MaxClientVersion[1]) {
            return CompatibilityResult::MinorVersionTooNew;
        }

        return CompatibilityResult::Compatible;
    }

    // Check if client version is compatible with server
    inline bool IsCompatible(const std::array<int, 2>& clientVersion) {
        return GetCompatibilityResult(clientVersion) == CompatibilityResult::Compatible;
    }

    // Get human-readable compatibility error message
    inline std::string GetCompatibilityErrorMessage(const std::array<int, 2>& clientVersion) {
        CompatibilityResult result = GetCompatibilityResult(clientVersion);

        switch (result) {
            case CompatibilityResult::Compatible:
                return "Client version is compatible";

            case CompatibilityResult::MajorVersionMismatch:
                return "Major version mismatch. Client: " + VersionToString(clientVersion) +
                       ", Server: " + VersionToString(ServerVersion) +
                       ". Please update both client and server to matching versions.";

            case CompatibilityResult::MinorVersionTooOld:
                return "Client version too old. Client: " + VersionToString(clientVersion) +
                       ", Server supports: " + VersionToString(MinClientVersion) +
                       " to " + VersionToString(MaxClientVersion) +
                       ". Please update your client.";

            case CompatibilityResult::MinorVersionTooNew:
                return "Client version too new. Client: " + VersionToString(clientVersion) +
                       ", Server supports: " + VersionToString(MinClientVersion) +
                       " to " + VersionToString(MaxClientVersion) +
                       ". Please update your server.";

            default:
                return "Unknown compatibility issue";
        }
    }
}

#endif // VERSION_H
