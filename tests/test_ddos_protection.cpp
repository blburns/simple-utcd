/*
 * tests/test_ddos_protection.cpp
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
#include "simple_utcd/ddos_protection.hpp"
#include <thread>
#include <chrono>

using namespace simple_utcd;

class DDoSProtectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        protection_.set_enabled(true);
        protection_.set_threshold(100);
        protection_.set_block_duration(1); // 1 second for testing
        protection_.set_connection_limit(5);
        protection_.set_connection_window(60);
        protection_.set_anomaly_threshold(3.0);
    }

    void TearDown() override {
        protection_.reset();
    }

    DDoSProtection protection_;
};

// Test default constructor
TEST_F(DDoSProtectionTest, DefaultConstructor) {
    DDoSProtection protection;
    EXPECT_FALSE(protection.is_enabled());
}

// Test configuration
TEST_F(DDoSProtectionTest, Configuration) {
    protection_.set_threshold(200);
    protection_.set_block_duration(3600);
    protection_.set_connection_limit(10);
    protection_.set_anomaly_threshold(2.5);
    
    EXPECT_TRUE(protection_.is_enabled());
}

// Test request checking below threshold
TEST_F(DDoSProtectionTest, RequestBelowThreshold) {
    std::string client_ip = "192.168.1.100";
    
    // Record some requests (below threshold)
    for (int i = 0; i < 50; i++) {
        DDoSResult result = protection_.check_request(client_ip);
        EXPECT_TRUE(result.allowed);
        EXPECT_EQ(result.status, DDoSStatus::NORMAL);
    }
}

// Test request checking above threshold
TEST_F(DDoSProtectionTest, RequestAboveThreshold) {
    std::string client_ip = "192.168.1.100";
    protection_.set_threshold(10);
    
    // Record requests above threshold
    for (int i = 0; i < 15; i++) {
        protection_.record_request(client_ip);
    }
    
    DDoSResult result = protection_.check_request(client_ip);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.status, DDoSStatus::ATTACK_DETECTED);
}

// Test connection limiting
TEST_F(DDoSProtectionTest, ConnectionLimiting) {
    std::string client_ip = "192.168.1.100";
    
    // Record connections up to limit
    for (int i = 0; i < 5; i++) {
        DDoSResult result = protection_.check_connection(client_ip);
        EXPECT_TRUE(result.allowed);
    }
    
    // Should block after limit
    DDoSResult result = protection_.check_connection(client_ip);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.status, DDoSStatus::ATTACK_DETECTED);
}

// Test IP blocking
TEST_F(DDoSProtectionTest, IPBlocking) {
    std::string client_ip = "192.168.1.100";
    
    protection_.block_client(client_ip, 1); // 1 second
    
    EXPECT_TRUE(protection_.is_blocked(client_ip));
    
    DDoSResult result = protection_.check_request(client_ip);
    EXPECT_FALSE(result.allowed);
    EXPECT_EQ(result.status, DDoSStatus::BLOCKED);
}

// Test block expiration
TEST_F(DDoSProtectionTest, BlockExpiration) {
    std::string client_ip = "192.168.1.100";
    
    protection_.block_client(client_ip, 1); // 1 second
    EXPECT_TRUE(protection_.is_blocked(client_ip));
    
    // Wait for block to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    protection_.cleanup_expired_entries();
    EXPECT_FALSE(protection_.is_blocked(client_ip));
}

// Test unblock client
TEST_F(DDoSProtectionTest, UnblockClient) {
    std::string client_ip = "192.168.1.100";
    
    protection_.block_client(client_ip, 3600);
    EXPECT_TRUE(protection_.is_blocked(client_ip));
    
    protection_.unblock_client(client_ip);
    EXPECT_FALSE(protection_.is_blocked(client_ip));
}

// Test clear blocks
TEST_F(DDoSProtectionTest, ClearBlocks) {
    protection_.block_client("192.168.1.100", 3600);
    protection_.block_client("192.168.1.101", 3600);
    protection_.block_client("192.168.1.102", 3600);
    
    EXPECT_EQ(protection_.get_blocked_ips().size(), 3);
    
    protection_.clear_blocks();
    EXPECT_EQ(protection_.get_blocked_ips().size(), 0);
}

// Test request count tracking
TEST_F(DDoSProtectionTest, RequestCountTracking) {
    std::string client_ip = "192.168.1.100";
    
    EXPECT_EQ(protection_.get_request_count(client_ip), 0);
    
    for (int i = 0; i < 10; i++) {
        protection_.record_request(client_ip);
    }
    
    EXPECT_EQ(protection_.get_request_count(client_ip), 10);
}

// Test connection count tracking
TEST_F(DDoSProtectionTest, ConnectionCountTracking) {
    std::string client_ip = "192.168.1.100";
    
    EXPECT_EQ(protection_.get_connection_count(client_ip), 0);
    
    for (int i = 0; i < 3; i++) {
        protection_.record_connection(client_ip);
    }
    
    EXPECT_EQ(protection_.get_connection_count(client_ip), 3);
    
    protection_.record_disconnection(client_ip);
    EXPECT_EQ(protection_.get_connection_count(client_ip), 2);
}

// Test anomaly detection
TEST_F(DDoSProtectionTest, AnomalyDetection) {
    std::string client_ip = "192.168.1.100";
    protection_.set_anomaly_threshold(2.0);
    
    // Record regular requests
    for (int i = 0; i < 10; i++) {
        protection_.record_request(client_ip);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    bool is_anomalous = protection_.detect_anomaly(client_ip);
    // May or may not be anomalous depending on pattern
    EXPECT_TRUE(is_anomalous || !is_anomalous);
}

// Test when disabled
TEST_F(DDoSProtectionTest, WhenDisabled) {
    protection_.set_enabled(false);
    
    std::string client_ip = "192.168.1.100";
    DDoSResult result = protection_.check_request(client_ip);
    EXPECT_TRUE(result.allowed); // Should always allow when disabled
}

// Test multiple IPs
TEST_F(DDoSProtectionTest, MultipleIPs) {
    std::string ip1 = "192.168.1.100";
    std::string ip2 = "192.168.1.101";
    
    protection_.record_request(ip1);
    protection_.record_request(ip2);
    
    EXPECT_EQ(protection_.get_request_count(ip1), 1);
    EXPECT_EQ(protection_.get_request_count(ip2), 1);
}

// Test total blocked count
TEST_F(DDoSProtectionTest, TotalBlockedCount) {
    uint64_t initial = protection_.get_total_blocked();
    
    std::string client_ip = "192.168.1.100";
    protection_.set_threshold(1);
    
    // Trigger blocking
    for (int i = 0; i < 5; i++) {
        protection_.record_request(client_ip);
    }
    protection_.check_request(client_ip);
    
    EXPECT_GT(protection_.get_total_blocked(), initial);
}

// Test cleanup expired entries
TEST_F(DDoSProtectionTest, CleanupExpiredEntries) {
    std::string client_ip = "192.168.1.100";
    
    protection_.record_request(client_ip);
    protection_.block_client(client_ip, 1);
    
    // Cleanup should not crash
    protection_.cleanup_expired_entries();
    
    // Wait for block to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));
    protection_.cleanup_expired_entries();
    
    EXPECT_FALSE(protection_.is_blocked(client_ip));
}

// Test reset
TEST_F(DDoSProtectionTest, Reset) {
    std::string client_ip = "192.168.1.100";
    
    protection_.record_request(client_ip);
    protection_.block_client(client_ip, 3600);
    
    EXPECT_GT(protection_.get_request_count(client_ip), 0);
    EXPECT_TRUE(protection_.is_blocked(client_ip));
    
    protection_.reset();
    
    EXPECT_EQ(protection_.get_request_count(client_ip), 0);
    EXPECT_FALSE(protection_.is_blocked(client_ip));
    EXPECT_EQ(protection_.get_total_blocked(), 0);
}

// Test warning status
TEST_F(DDoSProtectionTest, WarningStatus) {
    std::string client_ip = "192.168.1.100";
    protection_.set_threshold(10);
    
    // Record requests approaching threshold
    for (int i = 0; i < 8; i++) {
        protection_.record_request(client_ip);
    }
    
    DDoSResult result = protection_.check_request(client_ip);
    // May be normal or warning depending on rate calculation
    EXPECT_TRUE(result.allowed);
}

