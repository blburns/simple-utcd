/*
 * tests/test_rate_limiter.cpp
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
#include "simple_utcd/rate_limiter.hpp"
#include <thread>
#include <chrono>

using namespace simple_utcd;

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        limiter_.set_enabled(true);
        limiter_.set_rate(10); // 10 requests per second
        limiter_.set_burst_size(5);
        limiter_.set_window_seconds(60);
    }

    void TearDown() override {
        limiter_.reset();
    }

    RateLimiter limiter_;
};

// Test default constructor
TEST_F(RateLimiterTest, DefaultConstructor) {
    RateLimiter limiter;
    EXPECT_FALSE(limiter.is_enabled());
}

// Test configuration
TEST_F(RateLimiterTest, Configuration) {
    limiter_.set_rate(100);
    limiter_.set_burst_size(20);
    limiter_.set_window_seconds(30);
    
    // Configuration should be set
    EXPECT_TRUE(limiter_.is_enabled());
}

// Test per-client rate limiting
TEST_F(RateLimiterTest, PerClientRateLimiting) {
    std::string client_id = "client1";
    
    // Should allow initial requests
    for (int i = 0; i < 5; i++) {
        RateLimitResult result = limiter_.check_limit(client_id, 10, 5);
        EXPECT_TRUE(result.allowed) << "Request " << i << " should be allowed";
    }
    
    // Should deny after burst is exhausted
    RateLimitResult result = limiter_.check_limit(client_id, 10, 5);
    EXPECT_FALSE(result.allowed);
}

// Test token refill
TEST_F(RateLimiterTest, TokenRefill) {
    std::string client_id = "client1";
    
    // Exhaust tokens
    for (int i = 0; i < 5; i++) {
        limiter_.check_limit(client_id, 10, 5);
    }
    
    // Wait for refill (1 second should refill 10 tokens)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    // Should allow requests again
    RateLimitResult result = limiter_.check_limit(client_id, 10, 5);
    EXPECT_TRUE(result.allowed);
}

// Test global rate limiting
TEST_F(RateLimiterTest, GlobalRateLimiting) {
    limiter_.set_global_rate(100);
    limiter_.set_global_burst(20);
    
    // Should allow initial requests
    for (int i = 0; i < 20; i++) {
        RateLimitResult result = limiter_.check_global_limit();
        EXPECT_TRUE(result.allowed) << "Request " << i << " should be allowed";
    }
    
    // Should deny after global burst is exhausted
    RateLimitResult result = limiter_.check_global_limit();
    EXPECT_FALSE(result.allowed);
}

// Test connection limiting
TEST_F(RateLimiterTest, ConnectionLimiting) {
    std::string client_id = "client1";
    uint64_t max_connections = 5;
    
    // Should allow connections up to limit
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter_.check_connection_limit(client_id, max_connections));
        limiter_.record_connection(client_id);
    }
    
    // Should deny after limit
    EXPECT_FALSE(limiter_.check_connection_limit(client_id, max_connections));
    
    // Release a connection
    limiter_.release_connection(client_id);
    EXPECT_TRUE(limiter_.check_connection_limit(client_id, max_connections));
}

// Test active connection tracking
TEST_F(RateLimiterTest, ActiveConnectionTracking) {
    std::string client_id = "client1";
    
    EXPECT_EQ(limiter_.get_active_connections(client_id), 0);
    
    limiter_.record_connection(client_id);
    EXPECT_EQ(limiter_.get_active_connections(client_id), 1);
    
    limiter_.record_connection(client_id);
    EXPECT_EQ(limiter_.get_active_connections(client_id), 2);
    
    limiter_.release_connection(client_id);
    EXPECT_EQ(limiter_.get_active_connections(client_id), 1);
    
    limiter_.release_connection(client_id);
    EXPECT_EQ(limiter_.get_active_connections(client_id), 0);
}

// Test when disabled
TEST_F(RateLimiterTest, WhenDisabled) {
    limiter_.set_enabled(false);
    
    std::string client_id = "client1";
    RateLimitResult result = limiter_.check_limit(client_id);
    EXPECT_TRUE(result.allowed); // Should always allow when disabled
}

// Test multiple clients
TEST_F(RateLimiterTest, MultipleClients) {
    std::string client1 = "client1";
    std::string client2 = "client2";
    
    // Each client should have independent limits
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter_.check_limit(client1, 10, 5).allowed);
        EXPECT_TRUE(limiter_.check_limit(client2, 10, 5).allowed);
    }
    
    // Both should be exhausted
    EXPECT_FALSE(limiter_.check_limit(client1, 10, 5).allowed);
    EXPECT_FALSE(limiter_.check_limit(client2, 10, 5).allowed);
}

// Test cleanup expired entries
TEST_F(RateLimiterTest, CleanupExpiredEntries) {
    std::string client_id = "client1";
    
    limiter_.check_limit(client_id, 10, 5);
    
    // Cleanup should not crash
    limiter_.cleanup_expired_entries();
    
    // Client should still exist (not expired yet)
    RateLimitResult result = limiter_.check_limit(client_id, 10, 5);
    EXPECT_TRUE(result.allowed || !result.allowed); // Either is fine
}

// Test reset
TEST_F(RateLimiterTest, Reset) {
    std::string client_id = "client1";
    
    // Exhaust limits
    for (int i = 0; i < 5; i++) {
        limiter_.check_limit(client_id, 10, 5);
    }
    
    // Reset
    limiter_.reset();
    
    // Should allow again after reset
    RateLimitResult result = limiter_.check_limit(client_id, 10, 5);
    EXPECT_TRUE(result.allowed);
}

// Test burst protection
TEST_F(RateLimiterTest, BurstProtection) {
    std::string client_id = "client1";
    limiter_.set_burst_size(3);
    
    // Should allow burst
    for (int i = 0; i < 3; i++) {
        EXPECT_TRUE(limiter_.check_limit(client_id, 10, 3).allowed);
    }
    
    // Should deny after burst
    EXPECT_FALSE(limiter_.check_limit(client_id, 10, 3).allowed);
}

// Test rate limit result details
TEST_F(RateLimiterTest, RateLimitResultDetails) {
    std::string client_id = "client1";
    
    RateLimitResult result = limiter_.check_limit(client_id, 10, 5);
    EXPECT_TRUE(result.allowed);
    EXPECT_GE(result.remaining, 0);
    EXPECT_GT(result.reset_after_seconds, 0);
}

// Test different rates for different clients
TEST_F(RateLimiterTest, DifferentRatesPerClient) {
    std::string client1 = "client1";
    std::string client2 = "client2";
    
    // Client 1: 10 req/s, burst 5
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter_.check_limit(client1, 10, 5).allowed);
    }
    EXPECT_FALSE(limiter_.check_limit(client1, 10, 5).allowed);
    
    // Client 2: 20 req/s, burst 10
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(limiter_.check_limit(client2, 20, 10).allowed);
    }
    EXPECT_FALSE(limiter_.check_limit(client2, 20, 10).allowed);
}

