/*
 * includes/simple_utcd/auth.hpp
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

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>

namespace simple_utcd {

/**
 * @brief Authentication algorithm types
 */
enum class AuthAlgorithm {
    MD5,
    SHA1,
    SHA256
};

/**
 * @brief Authentication result
 */
struct AuthResult {
    bool success;
    std::string message;
    std::string session_id;
    std::chrono::system_clock::time_point expires_at;
    
    AuthResult() : success(false), expires_at(std::chrono::system_clock::now()) {}
};

/**
 * @brief Authentication manager for UTC daemon
 */
class Authenticator {
public:
    Authenticator();
    ~Authenticator();

    // Configuration
    void set_algorithm(AuthAlgorithm algorithm);
    void set_key(const std::string& key);
    void set_timeout(int timeout_ms);
    void set_session_timeout(int timeout_seconds);
    void set_max_failed_attempts(int max_attempts);
    void set_lockout_duration(int duration_seconds);
    
    // Authentication operations
    AuthResult authenticate(const std::string& key_id, const std::string& signature, 
                          const std::string& timestamp);
    bool verify_signature(const std::string& data, const std::string& signature, 
                         const std::string& key);
    
    // Session management
    bool is_session_valid(const std::string& session_id) const;
    void invalidate_session(const std::string& session_id);
    void cleanup_expired_sessions();
    
    // Key management
    bool add_key(const std::string& key_id, const std::string& key);
    bool remove_key(const std::string& key_id);
    bool has_key(const std::string& key_id) const;
    void clear_keys();
    
    // Security operations
    bool is_locked_out(const std::string& key_id) const;
    void record_failed_attempt(const std::string& key_id);
    void record_successful_attempt(const std::string& key_id);
    
    // Status
    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    
    // Generate signature for testing
    std::string generate_signature(const std::string& data, const std::string& key) const;

private:
    bool enabled_;
    AuthAlgorithm algorithm_;
    std::string default_key_;
    int timeout_ms_;
    int session_timeout_seconds_;
    int max_failed_attempts_;
    int lockout_duration_seconds_;
    
    // Key storage
    std::map<std::string, std::string> keys_;
    mutable std::mutex keys_mutex_;
    
    // Session storage
    struct Session {
        std::string key_id;
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point expires_at;
    };
    std::map<std::string, Session> sessions_;
    mutable std::mutex sessions_mutex_;
    
    // Failed attempt tracking
    struct FailedAttempt {
        int count;
        std::chrono::system_clock::time_point first_attempt;
        std::chrono::system_clock::time_point locked_until;
    };
    std::map<std::string, FailedAttempt> failed_attempts_;
    mutable std::mutex failed_attempts_mutex_;
    
    // Hash computation
    std::string compute_md5(const std::string& data) const;
    std::string compute_sha1(const std::string& data) const;
    std::string compute_sha256(const std::string& data) const;
    std::string compute_hash(const std::string& data) const;
    
    // Session ID generation
    std::string generate_session_id() const;
    
    // Time utilities
    bool is_expired(const std::chrono::system_clock::time_point& time) const;
};

} // namespace simple_utcd

