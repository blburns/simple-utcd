/*
 * tests/test_utc_config.cpp
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
#include "simple_utcd/utc_config.hpp"
#include <fstream>
#include <cstdio>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
// Fallback for older compilers
#include <sys/stat.h>
#endif

using namespace simple_utcd;

class UTCConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary config file for testing
        test_config_file_ = "/tmp/test_simple_utcd.conf";
    }

    void TearDown() override {
        // Clean up test config file
#if __has_include(<filesystem>) || __has_include(<experimental/filesystem>)
        if (fs::exists(test_config_file_)) {
            std::remove(test_config_file_.c_str());
        }
#else
        struct stat buffer;
        if (stat(test_config_file_.c_str(), &buffer) == 0) {
            std::remove(test_config_file_.c_str());
        }
#endif
    }

    std::string test_config_file_;
};

// Test default constructor
TEST_F(UTCConfigTest, DefaultConstructor) {
    UTCConfig config;
    EXPECT_EQ(config.get_listen_port(), 37); // Default UTC port
    EXPECT_FALSE(config.get_listen_address().empty());
    EXPECT_GT(config.get_max_connections(), 0);
}

// Test getters and setters for network configuration
TEST_F(UTCConfigTest, NetworkConfiguration) {
    UTCConfig config;
    
    config.set_listen_address("127.0.0.1");
    EXPECT_EQ(config.get_listen_address(), "127.0.0.1");
    
    config.set_listen_port(1234);
    EXPECT_EQ(config.get_listen_port(), 1234);
    
    config.set_ipv6_enabled(true);
    EXPECT_TRUE(config.is_ipv6_enabled());
    
    config.set_max_connections(500);
    EXPECT_EQ(config.get_max_connections(), 500);
}

// Test UTC server configuration
TEST_F(UTCConfigTest, UTCServerConfiguration) {
    UTCConfig config;
    
    config.set_stratum(3);
    EXPECT_EQ(config.get_stratum(), 3);
    
    config.set_reference_id("TEST");
    EXPECT_EQ(config.get_reference_id(), "TEST");
    
    config.set_reference_clock("LOCAL");
    EXPECT_EQ(config.get_reference_clock(), "LOCAL");
    
    std::vector<std::string> servers = {"time.nist.gov", "time.google.com"};
    config.set_upstream_servers(servers);
    EXPECT_EQ(config.get_upstream_servers().size(), 2);
    
    config.set_sync_interval(128);
    EXPECT_EQ(config.get_sync_interval(), 128);
    
    config.set_timeout(2000);
    EXPECT_EQ(config.get_timeout(), 2000);
}

// Test logging configuration
TEST_F(UTCConfigTest, LoggingConfiguration) {
    UTCConfig config;
    
    config.set_log_file("/tmp/test.log");
    EXPECT_EQ(config.get_log_file(), "/tmp/test.log");
    
    config.set_log_level("DEBUG");
    EXPECT_EQ(config.get_log_level(), "DEBUG");
    
    config.set_console_logging_enabled(true);
    EXPECT_TRUE(config.is_console_logging_enabled());
    
    config.set_syslog_enabled(true);
    EXPECT_TRUE(config.is_syslog_enabled());
}

// Test security configuration
TEST_F(UTCConfigTest, SecurityConfiguration) {
    UTCConfig config;
    
    config.set_authentication_enabled(true);
    EXPECT_TRUE(config.is_authentication_enabled());
    
    config.set_authentication_key("secret-key");
    EXPECT_EQ(config.get_authentication_key(), "secret-key");
    
    config.set_query_restriction_enabled(true);
    EXPECT_TRUE(config.is_query_restriction_enabled());
    
    std::vector<std::string> allowed = {"192.168.1.0/24"};
    config.set_allowed_clients(allowed);
    EXPECT_EQ(config.get_allowed_clients().size(), 1);
}

// Test performance configuration
TEST_F(UTCConfigTest, PerformanceConfiguration) {
    UTCConfig config;
    
    config.set_worker_threads(4);
    EXPECT_EQ(config.get_worker_threads(), 4);
    
    config.set_max_packet_size(1024);
    EXPECT_EQ(config.get_max_packet_size(), 1024);
    
    config.set_statistics_enabled(true);
    EXPECT_TRUE(config.is_statistics_enabled());
    
    config.set_stats_interval(60);
    EXPECT_EQ(config.get_stats_interval(), 60);
}

// Test loading a simple config file
TEST_F(UTCConfigTest, LoadConfigFile) {
    // Create a simple config file
    std::ofstream config_file(test_config_file_);
    config_file << "listen_address = 127.0.0.1\n";
    config_file << "listen_port = 1234\n";
    config_file << "log_level = DEBUG\n";
    config_file.close();
    
    UTCConfig config;
    bool loaded = config.load(test_config_file_);
    EXPECT_TRUE(loaded);
    EXPECT_EQ(config.get_listen_address(), "127.0.0.1");
    EXPECT_EQ(config.get_listen_port(), 1234);
    EXPECT_EQ(config.get_log_level(), "DEBUG");
}

// Test loading non-existent file
TEST_F(UTCConfigTest, LoadNonExistentFile) {
    UTCConfig config;
    bool loaded = config.load("/nonexistent/file.conf");
    EXPECT_FALSE(loaded);
}

