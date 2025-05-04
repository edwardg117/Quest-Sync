#include <gtest/gtest.h>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

// Custom listener to capture test results
class TestResultListener : public ::testing::EmptyTestEventListener {
private:
    std::ofstream m_outputFile;
    bool m_xmlOutput;
    int m_testCount = 0;
    int m_failureCount = 0;
    std::string m_currentTestCase;
    std::string m_currentTest;

public:
    TestResultListener(const std::string& outputPath, bool xmlOutput) : m_xmlOutput(xmlOutput) {
        m_outputFile.open(outputPath);

        if (m_xmlOutput) {
            m_outputFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
            m_outputFile << "<testsuites>" << std::endl;
        } else {
            m_outputFile << "{" << std::endl;
            m_outputFile << "  \"testsuites\": [" << std::endl;
        }
    }

    ~TestResultListener() {
        if (m_xmlOutput) {
            m_outputFile << "</testsuites>" << std::endl;
        } else {
            m_outputFile << "  ]" << std::endl;
            m_outputFile << "}" << std::endl;
        }

        m_outputFile.close();
    }

    void OnTestProgramStart(const ::testing::UnitTest& unit_test) override {
        // Program start
    }

    void OnTestSuiteStart(const ::testing::TestSuite& test_suite) override {
        m_currentTestCase = test_suite.name();

        if (m_xmlOutput) {
            m_outputFile << "  <testsuite name=\"" << m_currentTestCase << "\">" << std::endl;
        } else {
            if (m_testCount > 0) {
                m_outputFile << "    }," << std::endl;
            }
            m_outputFile << "    {" << std::endl;
            m_outputFile << "      \"name\": \"" << m_currentTestCase << "\"," << std::endl;
            m_outputFile << "      \"tests\": [" << std::endl;
        }
    }

    void OnTestStart(const ::testing::TestInfo& test_info) override {
        m_currentTest = test_info.name();
    }

    void OnTestPartResult(const ::testing::TestPartResult& test_part_result) override {
        // Test part result
    }

    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        m_testCount++;
        bool passed = test_info.result()->Passed();

        if (!passed) {
            m_failureCount++;
        }

        if (m_xmlOutput) {
            m_outputFile << "    <testcase name=\"" << test_info.name() << "\" status=\"" << (passed ? "pass" : "fail") << "\"";
            if (!passed) {
                m_outputFile << ">" << std::endl;
                m_outputFile << "      <failure message=\"Test failed\"></failure>" << std::endl;
                m_outputFile << "    </testcase>" << std::endl;
            } else {
                m_outputFile << " />" << std::endl;
            }
        } else {
            if (m_testCount > 1 && test_info.test_suite_name() == m_currentTestCase) {
                m_outputFile << "        }," << std::endl;
            }
            m_outputFile << "        {" << std::endl;
            m_outputFile << "          \"name\": \"" << test_info.name() << "\"," << std::endl;
            m_outputFile << "          \"status\": \"" << (passed ? "pass" : "fail") << "\"" << std::endl;
        }
    }

    void OnTestSuiteEnd(const ::testing::TestSuite& test_suite) override {
        if (m_xmlOutput) {
            m_outputFile << "  </testsuite>" << std::endl;
        } else {
            m_outputFile << "        }" << std::endl;
            m_outputFile << "      ]" << std::endl;
        }
    }

    void OnTestProgramEnd(const ::testing::UnitTest& unit_test) override {
        std::cout << "Total tests: " << m_testCount << ", Failures: " << m_failureCount << std::endl;
    }
};

// Simple test to verify Google Test is working
TEST(SanityCheck, GoogleTestWorking) {
    EXPECT_TRUE(true);
    EXPECT_EQ(2 + 2, 4);
    std::cout << "Google Test is working correctly!" << std::endl;
}

void PrintUsage() {
    std::cout << "Usage: QuestSyncServerTests [gtest options] [--output-xml=FILE | --output-json=FILE]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --output-xml=FILE   Save test results to XML file" << std::endl;
    std::cout << "  --output-json=FILE  Save test results to JSON file" << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "Starting Quest Sync Server Tests..." << std::endl;

    // Parse command line arguments for output file
    std::string outputPath;
    bool xmlOutput = true;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg.find("--output-xml=") == 0) {
            outputPath = arg.substr(13);
            xmlOutput = true;
        } else if (arg.find("--output-json=") == 0) {
            outputPath = arg.substr(14);
            xmlOutput = false;
        } else if (arg == "--help") {
            PrintUsage();
            return 0;
        }
    }

    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // Add custom listener if output file is specified
    ::testing::TestEventListener* defaultListener = nullptr;
    if (!outputPath.empty()) {
        // Create the output directory if it doesn't exist
        std::filesystem::path outputFilePath(outputPath);
        std::filesystem::create_directories(outputFilePath.parent_path());

        // Remove default XML listener if we're using our own
        auto& listeners = ::testing::UnitTest::GetInstance()->listeners();
        defaultListener = listeners.Release(listeners.default_xml_generator());

        // Add our custom listener
        listeners.Append(new TestResultListener(outputPath, xmlOutput));

        std::cout << "Test results will be saved to " << outputPath << std::endl;
    }

    // Run the tests
    int result = RUN_ALL_TESTS();

    // Clean up
    if (defaultListener) {
        delete defaultListener;
    }

    std::cout << "Tests completed with result: " << result << std::endl;
    return result;
}
