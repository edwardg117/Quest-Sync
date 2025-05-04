#include "MockClasses.h"
#include <fstream>
#include <random>
#include <cstdio>
#include <direct.h>
#include <Windows.h>

// TestUtils implementation
namespace TestUtils {
    std::string CreateTempFile(const std::string& content) {
        // Create a temporary file with a unique name
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);

        std::string tempFilename = std::string(tempPath) + "qsync_test_" +
            std::to_string(std::random_device()()) + ".tmp";

        // Write content to the file
        std::ofstream file(tempFilename);
        if (file.is_open()) {
            file << content;
            file.close();
            return tempFilename;
        }

        return "";
    }

    void DeleteTempFile(const std::string& filename) {
        if (!filename.empty()) {
            remove(filename.c_str());
        }
    }

    int GetRandomPort() {
        // Generate a random port number between 10000 and 65535
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(10000, 65535);
        return distrib(gen);
    }
}
