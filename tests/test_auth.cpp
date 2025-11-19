/*
 * tests/test_auth.cpp
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
#include "simple_utcd/auth.hpp"
#include <thread>
#include <chrono>

using namespace simple_utcd;

class AuthenticatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        auth_.set_key("test-secret-key");
        auth_.set_algorithm(AuthAlgorithm::SHA256);
        auth_.set_enabled(true);
    }

    void TearDown() override {
        auth_.clear_keys();
    }

    Authenticator auth_;
};

// Test default constructor
TEST_F(AuthenticatorTest, DefaultConstructor) {
    Authenticator auth;
    EXPECT_FALSE(auth.is_enabled());
}

// Test algorithm configuration
TEST_F(AuthenticatorTest, AlgorithmConfiguration) {
    auth_.set_algorithm(AuthAlgorithm::MD5);
    std::string sig1 = auth_.generate_signature("test-data", "key");
    
    auth_.set_algorithm(AuthAlgorithm::SHA1);
    std::string sig2 = auth_.generate_signature("test-data", "key");
    
    auth_.set_algorithm(AuthAlgorithm::SHA256);
    std::string sig3 = auth_.generate_signature("test-data", "key");
    
    // All should be different
    EXPECT_NE(sig1, sig2);
    EXPECT_NE(sig2, sig3);
    EXPECT_NE(sig1, sig3);
}

// Test signature generation and verification
TEST_F(AuthenticatorTest, SignatureGeneration) {
    std::string data = "test-data";
    std::string key = "test-key";
    
    std::string signature = auth_.generate_signature(data, key);
    EXPECT_FALSE(signature.empty());
    
    bool verified = auth_.verify_signature(data, signature, key);
    EXPECT_TRUE(verified);
}

// Test signature verification with wrong key
TEST_F(AuthenticatorTest, SignatureVerificationWrongKey) {
    std::string data = "test-data";
    std::string key1 = "key1";
    std::string key2 = "key2";
    
    std::string signature = auth_.generate_signature(data, key1);
    bool verified = auth_.verify_signature(data, signature, key2);
    EXPECT_FALSE(verified);
}

// Test authentication with valid credentials
TEST_F(AuthenticatorTest, AuthenticationValid) {
    auth_.add_key("key1", "secret-key-1");
    
    std::string key_id = "key1";
    std::string timestamp = "1234567890";
    std::string data = key_id + timestamp;
    std::string signature = auth_.generate_signature(data, "secret-key-1");
    
    AuthResult result = auth_.authenticate(key_id, signature, timestamp);
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.session_id.empty());
}

// Test authentication with invalid signature
TEST_F(AuthenticatorTest, AuthenticationInvalidSignature) {
    auth_.add_key("key1", "secret-key-1");
    
    std::string key_id = "key1";
    std::string timestamp = "1234567890";
    std::string wrong_signature = "invalid-signature";
    
    AuthResult result = auth_.authenticate(key_id, wrong_signature, timestamp);
    EXPECT_FALSE(result.success);
}

// Test authentication when disabled
TEST_F(AuthenticatorTest, AuthenticationDisabled) {
    auth_.set_enabled(false);
    
    AuthResult result = auth_.authenticate("key1", "signature", "timestamp");
    EXPECT_TRUE(result.success); // Should succeed when disabled
}

// Test key management
TEST_F(AuthenticatorTest, KeyManagement) {
    EXPECT_TRUE(auth_.add_key("key1", "secret1"));
    EXPECT_TRUE(auth_.has_key("key1"));
    
    EXPECT_TRUE(auth_.add_key("key2", "secret2"));
    EXPECT_TRUE(auth_.has_key("key2"));
    
    EXPECT_TRUE(auth_.remove_key("key1"));
    EXPECT_FALSE(auth_.has_key("key1"));
    EXPECT_TRUE(auth_.has_key("key2"));
    
    auth_.clear_keys();
    EXPECT_FALSE(auth_.has_key("key2"));
}

// Test session management
TEST_F(AuthenticatorTest, SessionManagement) {
    auth_.add_key("key1", "secret-key");
    
    std::string key_id = "key1";
    std::string timestamp = "1234567890";
    std::string data = key_id + timestamp;
    std::string signature = auth_.generate_signature(data, "secret-key");
    
    AuthResult result = auth_.authenticate(key_id, signature, timestamp);
    EXPECT_TRUE(result.success);
    
    std::string session_id = result.session_id;
    EXPECT_TRUE(auth_.is_session_valid(session_id));
    
    auth_.invalidate_session(session_id);
    EXPECT_FALSE(auth_.is_session_valid(session_id));
}

// Test failed attempt tracking
TEST_F(AuthenticatorTest, FailedAttemptTracking) {
    auth_.set_max_failed_attempts(3);
    auth_.set_lockout_duration(1); // 1 second for testing
    
    std::string key_id = "test-key";
    
    // Record failed attempts
    auth_.record_failed_attempt(key_id);
    EXPECT_FALSE(auth_.is_locked_out(key_id));
    
    auth_.record_failed_attempt(key_id);
    EXPECT_FALSE(auth_.is_locked_out(key_id));
    
    auth_.record_failed_attempt(key_id);
    EXPECT_TRUE(auth_.is_locked_out(key_id));
    
    // Wait for lockout to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));
    EXPECT_FALSE(auth_.is_locked_out(key_id));
}

// Test successful attempt clears failed attempts
TEST_F(AuthenticatorTest, SuccessfulAttemptClearsFailures) {
    auth_.set_max_failed_attempts(3);
    std::string key_id = "test-key";
    
    auth_.record_failed_attempt(key_id);
    auth_.record_failed_attempt(key_id);
    
    auth_.record_successful_attempt(key_id);
    EXPECT_FALSE(auth_.is_locked_out(key_id));
}

// Test authentication with locked out key
TEST_F(AuthenticatorTest, AuthenticationLockedOut) {
    auth_.add_key("key1", "secret-key");
    auth_.set_max_failed_attempts(2);
    auth_.set_lockout_duration(10);
    
    std::string key_id = "key1";
    
    // Lock out the key
    auth_.record_failed_attempt(key_id);
    auth_.record_failed_attempt(key_id);
    
    // Try to authenticate
    AuthResult result = auth_.authenticate(key_id, "signature", "timestamp");
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.message.find("locked"), std::string::npos);
}

// Test session expiration
TEST_F(AuthenticatorTest, SessionExpiration) {
    auth_.add_key("key1", "secret-key");
    auth_.set_session_timeout(1); // 1 second for testing
    
    std::string key_id = "key1";
    std::string timestamp = "1234567890";
    std::string data = key_id + timestamp;
    std::string signature = auth_.generate_signature(data, "secret-key");
    
    AuthResult result = auth_.authenticate(key_id, signature, timestamp);
    std::string session_id = result.session_id;
    
    EXPECT_TRUE(auth_.is_session_valid(session_id));
    
    // Wait for session to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    auth_.cleanup_expired_sessions();
    EXPECT_FALSE(auth_.is_session_valid(session_id));
}

// Test MD5 algorithm
TEST_F(AuthenticatorTest, MD5Algorithm) {
    auth_.set_algorithm(AuthAlgorithm::MD5);
    std::string sig = auth_.generate_signature("test", "key");
    EXPECT_FALSE(sig.empty());
    EXPECT_EQ(sig.length(), 32); // MD5 produces 32 hex chars
}

// Test SHA1 algorithm
TEST_F(AuthenticatorTest, SHA1Algorithm) {
    auth_.set_algorithm(AuthAlgorithm::SHA1);
    std::string sig = auth_.generate_signature("test", "key");
    EXPECT_FALSE(sig.empty());
    EXPECT_EQ(sig.length(), 40); // SHA1 produces 40 hex chars
}

// Test SHA256 algorithm
TEST_F(AuthenticatorTest, SHA256Algorithm) {
    auth_.set_algorithm(AuthAlgorithm::SHA256);
    std::string sig = auth_.generate_signature("test", "key");
    EXPECT_FALSE(sig.empty());
    EXPECT_EQ(sig.length(), 64); // SHA256 produces 64 hex chars
}

// Test timeout configuration
TEST_F(AuthenticatorTest, TimeoutConfiguration) {
    auth_.set_timeout(5000);
    auth_.set_session_timeout(1800);
    auth_.set_max_failed_attempts(5);
    auth_.set_lockout_duration(600);
    
    // Configuration should be set (no direct getters, but should not crash)
    EXPECT_TRUE(true);
}

// Test multiple keys
TEST_F(AuthenticatorTest, MultipleKeys) {
    auth_.add_key("key1", "secret1");
    auth_.add_key("key2", "secret2");
    auth_.add_key("key3", "secret3");
    
    EXPECT_TRUE(auth_.has_key("key1"));
    EXPECT_TRUE(auth_.has_key("key2"));
    EXPECT_TRUE(auth_.has_key("key3"));
    
    // Test authentication with each key
    for (int i = 1; i <= 3; i++) {
        std::string key_id = "key" + std::to_string(i);
        std::string secret = "secret" + std::to_string(i);
        std::string timestamp = "1234567890";
        std::string data = key_id + timestamp;
        std::string signature = auth_.generate_signature(data, secret);
        
        AuthResult result = auth_.authenticate(key_id, signature, timestamp);
        EXPECT_TRUE(result.success) << "Failed for " << key_id;
    }
}

