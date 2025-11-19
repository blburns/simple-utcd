/*
 * src/core/auth.cpp
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

#include "simple_utcd/auth.hpp"
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstring>

namespace simple_utcd {

Authenticator::Authenticator()
    : enabled_(false)
    , algorithm_(AuthAlgorithm::SHA256)
    , timeout_ms_(10000)
    , session_timeout_seconds_(3600)
    , max_failed_attempts_(3)
    , lockout_duration_seconds_(300)
{
}

Authenticator::~Authenticator() {
}

void Authenticator::set_algorithm(AuthAlgorithm algorithm) {
    algorithm_ = algorithm;
}

void Authenticator::set_key(const std::string& key) {
    default_key_ = key;
}

void Authenticator::set_timeout(int timeout_ms) {
    timeout_ms_ = timeout_ms;
}

void Authenticator::set_session_timeout(int timeout_seconds) {
    session_timeout_seconds_ = timeout_seconds;
}

void Authenticator::set_max_failed_attempts(int max_attempts) {
    max_failed_attempts_ = max_attempts;
}

void Authenticator::set_lockout_duration(int duration_seconds) {
    lockout_duration_seconds_ = duration_seconds;
}

std::string Authenticator::compute_md5(const std::string& data) const {
    unsigned char digest[MD5_DIGEST_LENGTH];
    // Use EVP interface for OpenSSL 3.0+ compatibility
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    const EVP_MD* md = EVP_md5();
    EVP_DigestInit_ex(ctx, md, nullptr);
    EVP_DigestUpdate(ctx, data.c_str(), data.length());
    EVP_DigestFinal_ex(ctx, digest, nullptr);
    EVP_MD_CTX_free(ctx);
    
    std::ostringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return ss.str();
}

std::string Authenticator::compute_sha1(const std::string& data) const {
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), digest);
    
    std::ostringstream ss;
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return ss.str();
}

std::string Authenticator::compute_sha256(const std::string& data) const {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.c_str()), data.length(), digest);
    
    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return ss.str();
}

std::string Authenticator::compute_hash(const std::string& data) const {
    switch (algorithm_) {
        case AuthAlgorithm::MD5:
            return compute_md5(data);
        case AuthAlgorithm::SHA1:
            return compute_sha1(data);
        case AuthAlgorithm::SHA256:
            return compute_sha256(data);
        default:
            return compute_sha256(data);
    }
}

std::string Authenticator::generate_signature(const std::string& data, const std::string& key) const {
    std::string combined = data + key;
    return compute_hash(combined);
}

bool Authenticator::verify_signature(const std::string& data, const std::string& signature, 
                                    const std::string& key) {
    std::string expected = generate_signature(data, key);
    return expected == signature;
}

AuthResult Authenticator::authenticate(const std::string& key_id, const std::string& signature, 
                                      const std::string& timestamp) {
    AuthResult result;
    
    if (!enabled_) {
        result.success = true;
        result.message = "Authentication disabled";
        return result;
    }
    
    // Check if locked out
    if (is_locked_out(key_id)) {
        result.success = false;
        result.message = "Account locked due to too many failed attempts";
        return result;
    }
    
    // Get key
    std::string key;
    {
        std::lock_guard<std::mutex> lock(keys_mutex_);
        if (keys_.find(key_id) != keys_.end()) {
            key = keys_[key_id];
        } else if (!default_key_.empty()) {
            key = default_key_;
        } else {
            record_failed_attempt(key_id);
            result.success = false;
            result.message = "Invalid key ID";
            return result;
        }
    }
    
    // Verify signature
    std::string data = key_id + timestamp;
    if (!verify_signature(data, signature, key)) {
        record_failed_attempt(key_id);
        result.success = false;
        result.message = "Invalid signature";
        return result;
    }
    
    // Create session
    result.success = true;
    result.message = "Authentication successful";
    result.session_id = generate_session_id();
    result.expires_at = std::chrono::system_clock::now() + 
                       std::chrono::seconds(session_timeout_seconds_);
    
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        Session session;
        session.key_id = key_id;
        session.created_at = std::chrono::system_clock::now();
        session.expires_at = result.expires_at;
        sessions_[result.session_id] = session;
    }
    
    record_successful_attempt(key_id);
    return result;
}

bool Authenticator::is_session_valid(const std::string& session_id) const {
    if (!enabled_) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return false;
    }
    
    return !is_expired(it->second.expires_at);
}

void Authenticator::invalidate_session(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(session_id);
}

void Authenticator::cleanup_expired_sessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    auto it = sessions_.begin();
    while (it != sessions_.end()) {
        if (is_expired(it->second.expires_at)) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

bool Authenticator::add_key(const std::string& key_id, const std::string& key) {
    std::lock_guard<std::mutex> lock(keys_mutex_);
    keys_[key_id] = key;
    return true;
}

bool Authenticator::remove_key(const std::string& key_id) {
    std::lock_guard<std::mutex> lock(keys_mutex_);
    return keys_.erase(key_id) > 0;
}

bool Authenticator::has_key(const std::string& key_id) const {
    std::lock_guard<std::mutex> lock(keys_mutex_);
    return keys_.find(key_id) != keys_.end();
}

void Authenticator::clear_keys() {
    std::lock_guard<std::mutex> lock(keys_mutex_);
    keys_.clear();
}

bool Authenticator::is_locked_out(const std::string& key_id) const {
    std::lock_guard<std::mutex> lock(failed_attempts_mutex_);
    auto it = failed_attempts_.find(key_id);
    if (it == failed_attempts_.end()) {
        return false;
    }
    
    if (it->second.count >= max_failed_attempts_) {
        if (it->second.locked_until > std::chrono::system_clock::now()) {
            return true;
        }
    }
    
    return false;
}

void Authenticator::record_failed_attempt(const std::string& key_id) {
    std::lock_guard<std::mutex> lock(failed_attempts_mutex_);
    auto& attempt = failed_attempts_[key_id];
    
    auto now = std::chrono::system_clock::now();
    if (attempt.count == 0 || 
        (now - attempt.first_attempt) > std::chrono::seconds(lockout_duration_seconds_)) {
        attempt.count = 1;
        attempt.first_attempt = now;
    } else {
        attempt.count++;
    }
    
    if (attempt.count >= max_failed_attempts_) {
        attempt.locked_until = now + std::chrono::seconds(lockout_duration_seconds_);
    }
}

void Authenticator::record_successful_attempt(const std::string& key_id) {
    std::lock_guard<std::mutex> lock(failed_attempts_mutex_);
    failed_attempts_.erase(key_id);
}

std::string Authenticator::generate_session_id() const {
    unsigned char buffer[16];
    if (RAND_bytes(buffer, sizeof(buffer)) != 1) {
        // Fallback to time-based ID if RAND_bytes fails
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        return std::to_string(time);
    }
    
    std::ostringstream ss;
    for (size_t i = 0; i < sizeof(buffer); i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(buffer[i]);
    }
    return ss.str();
}

bool Authenticator::is_expired(const std::chrono::system_clock::time_point& time) const {
    return time < std::chrono::system_clock::now();
}

} // namespace simple_utcd

