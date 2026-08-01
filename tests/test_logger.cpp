#include "gtest/gtest.h"
#include "src/core/logger.h"
#include <string>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary log file for testing
        logFile = fs::temp_directory_path() / "vrinject_test.log";
        vrinject::Logger::Init(logFile.string());
    }

    void TearDown() override {
        vrinject::Logger::Shutdown();
        // Clean up the log file
        if (fs::exists(logFile)) {
            fs::remove(logFile);
        }
    }

    fs::path logFile;
};

TEST_F(LoggerTest, LoggerInitialization) {
    // Logger should initialize without crashing
    EXPECT_TRUE(true);  // If we got here without exception, init worked
}

TEST_F(LoggerTest, LoggerLogging) {
    // Test that we can log messages
    vrinject::Logger::Log(vrinject::Logger::Level::Info, "test.cpp", 10, "Test message %d", 42);

    // Give it a moment to write
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Check that the log file exists and has content
    EXPECT_TRUE(fs::exists(logFile));
    EXPECT_GT(fs::file_size(logFile), 0);

    // Read the log file and check for our message
    std::ifstream logStream(logFile);
    std::stringstream buffer;
    buffer << logStream.rdbuf();
    std::string logContent = buffer.str();

    EXPECT_NE(logContent.find("Test message 42"), std::string::npos);
}

TEST_F(LoggerTest, LoggerDifferentLevels) {
    vrinject::Logger::Log(vrinject::Logger::Level::Debug, "test.cpp", 10, "Debug message");
    vrinject::Logger::Log(vrinject::Logger::Level::Info, "test.cpp", 10, "Info message");
    vrinject::Logger::Log(vrinject::Logger::Level::Warn, "test.cpp", 10, "Warning message");
    vrinject::Logger::Log(vrinject::Logger::Level::Error, "test.cpp", 10, "Error message");

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(fs::exists(logFile));
    EXPECT_GT(fs::file_size(logFile), 0);

    std::ifstream logStream(logFile);
    std::stringstream buffer;
    buffer << logStream.rdbuf();
    std::string logContent = buffer.str();

    EXPECT_NE(logContent.find("Debug message"), std::string::npos);
    EXPECT_NE(logContent.find("Info message"), std::string::npos);
    EXPECT_NE(logContent.find("Warning message"), std::string::npos);
    EXPECT_NE(logContent.find("Error message"), std::string::npos);
}

TEST_F(LoggerTest, LoggerPathFiltering) {
    // Test that paths are filtered out of log messages
    vrinject::Logger::Log(vrinject::Logger::Level::Info, "C:\\long\\path\\to\\file.cpp", 10, "Message with C:\\test\\path.exe in it");

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    EXPECT_TRUE(fs::exists(logFile));

    std::ifstream logStream(logFile);
    std::stringstream buffer;
    buffer << logStream.rdbuf();
    std::string logContent = buffer.str();

    // The path should be filtered, leaving just the filename
    EXPECT_NE(logContent.find("file.cpp"), std::string::npos);
    // The full path should not appear
    EXPECT_EQ(logContent.find("C:\\long\\path\\to\\file.cpp"), std::string::npos);
    // The embedded path in the message should ALSO be filtered for privacy
    EXPECT_EQ(logContent.find("C:\\test\\path.exe"), std::string::npos);
    EXPECT_NE(logContent.find("path.exe"), std::string::npos);
}