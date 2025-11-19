/*
 * includes/simple_utcd/rate_limiter.hpp
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
#include <map>
#include <chrono>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace simple_utcd {

/**
 * @brief Rate limit result
 */
struct RateLimitResult {
    bool allowed;
    uint64_t remaining;
    uint64_t reset_after_seconds;
    std::string message;
    
    RateLimitResult() : allowed(true), remaining(0), reset_after_seconds(0) {}
};

/**
 * @brief Rate limiter using token bucket algorithm
 */
class RateLimiter {
public:
    RateLimiter();
    ~RateLimiter();

    // Configuration
    void set_rate(uint64_t requests_per_second);
    void set_burst_size(uint64_t burst);
    void set_window_seconds(uint64_t window);
    
    // Per-client rate limiting
    RateLimitResult check_limit(const std::string& client_id);
    RateLimitResult check_limit(const std::string& client_id, uint64_t requests_per_second, uint64_t burst);
    
    // Global rate limiting
    RateLimitResult check_global_limit();
    void set_global_rate(uint64_t requests_per_second);
    void set_global_burst(uint64_t burst);
    
    // Connection rate limiting
    bool check_connection_limit(const std::string& client_id, uint64_t max_connections);
    void record_connection(const std::string& client_id);
    void release_connection(const std::string& client_id);
    uint64_t get_active_connections(const std::string& client_id) const;
    
    // Status
    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    
    // Cleanup
    void cleanup_expired_entries();
    void reset();

private:
    bool enabled_;
    uint64_t default_rate_;
    uint64_t default_burst_;
    uint64_t window_seconds_;
    
    // Global limits
    uint64_t global_rate_;
    uint64_t global_burst_;
    std::atomic<uint64_t> global_tokens_;
    std::chrono::system_clock::time_point global_last_refill_;
    mutable std::mutex global_mutex_;
    
    // Per-client tracking
    struct ClientState {
        uint64_t tokens;
        uint64_t rate;
        uint64_t burst;
        std::chrono::system_clock::time_point last_refill;
        std::atomic<uint64_t> active_connections;
        
        ClientState() : tokens(0), rate(0), burst(0),
                       last_refill(std::chrono::system_clock::now()),
                       active_connections(0) {}
    };
    std::map<std::string, ClientState> clients_;
    mutable std::mutex clients_mutex_;
    
    // Token bucket operations
    bool refill_tokens(ClientState& state, uint64_t rate, uint64_t burst);
    bool consume_token(ClientState& state);
    uint64_t calculate_tokens(ClientState& state, uint64_t rate, uint64_t burst);
    
    // Time utilities
    std::chrono::system_clock::time_point now() const;
    uint64_t seconds_since(const std::chrono::system_clock::time_point& time) const;
};

} // namespace simple_utcd

