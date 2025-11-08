/*
 * tests/test_logger.cpp
 *
 * Copyright 2024 SimpleDaemons
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include "simple_utcd/logger.hpp"
#include <fstream>
#include <cstdio>
#include <thread>
#include <chrono>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#include <sys/stat.h>
#endif

using namespace simple_utcd;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_log_file_ = "/tmp/test_simple_utcd.log";
    }

    void TearDown() override {
        // Clean up test log file
#if __has_include(<filesystem>) || __has_include(<experimental/filesystem>)
        if (fs::exists(test_log_file_)) {
            std::remove(test_log_file_.c_str());
        }
#else
        struct stat buffer;
        if (stat(test_log_file_.c_str(), &buffer) == 0) {
            std::remove(test_log_file_.c_str());
        }
#endif
    }

    std::string test_log_file_;
};

// Test default constructor
TEST_F(LoggerTest, DefaultConstructor) {
    Logger logger;
    // Should not crash
    logger.info("Test message");
}

// Test log levels
TEST_F(LoggerTest, LogLevels) {
    Logger logger;
    logger.set_level(LogLevel::DEBUG);
    
    // All these should not crash
    logger.debug("Debug message");
    logger.info("Info message");
    logger.warn("Warning message");
    logger.error("Error message");
}

// Test file logging
TEST_F(LoggerTest, FileLogging) {
    Logger logger;
    logger.set_log_file(test_log_file_);
    logger.info("Test log message");
    
    // Give it a moment to write
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check if file was created
#if __has_include(<filesystem>) || __has_include(<experimental/filesystem>)
    EXPECT_TRUE(fs::exists(test_log_file_));
#else
    struct stat buffer;
    EXPECT_EQ(stat(test_log_file_.c_str(), &buffer), 0);
#endif
}

// Test console logging
TEST_F(LoggerTest, ConsoleLogging) {
    Logger logger;
    logger.enable_console(true);
    logger.info("Console test message");
    // Should not crash
}

// Test set_level
TEST_F(LoggerTest, SetLevel) {
    Logger logger;
    logger.set_level(LogLevel::WARN);
    logger.debug("This should be filtered");
    logger.warn("This should be logged");
    // Should not crash
}

// Test template methods
TEST_F(LoggerTest, TemplateMethods) {
    Logger logger;
    logger.debug("Debug with format: {}", "test");
    logger.info("Info with format: {}", "test");
    logger.warn("Warn with format: {}", "test");
    logger.error("Error with format: {}", "test");
    // Should not crash
}

