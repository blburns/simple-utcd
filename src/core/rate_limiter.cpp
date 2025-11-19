/*
 * src/core/rate_limiter.cpp
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

#include "simple_utcd/rate_limiter.hpp"
#include <algorithm>
#include <cmath>

namespace simple_utcd {

RateLimiter::RateLimiter()
    : enabled_(false)
    , default_rate_(100)
    , default_burst_(20)
    , window_seconds_(60)
    , global_rate_(1000)
    , global_burst_(200)
    , global_tokens_(200)
{
    global_last_refill_ = now();
}

RateLimiter::~RateLimiter() {
}

void RateLimiter::set_rate(uint64_t requests_per_second) {
    default_rate_ = requests_per_second;
}

void RateLimiter::set_burst_size(uint64_t burst) {
    default_burst_ = burst;
}

void RateLimiter::set_window_seconds(uint64_t window) {
    window_seconds_ = window;
}

void RateLimiter::set_global_rate(uint64_t requests_per_second) {
    std::lock_guard<std::mutex> lock(global_mutex_);
    global_rate_ = requests_per_second;
}

void RateLimiter::set_global_burst(uint64_t burst) {
    std::lock_guard<std::mutex> lock(global_mutex_);
    global_burst_ = burst;
    global_tokens_ = burst;
}

RateLimitResult RateLimiter::check_limit(const std::string& client_id) {
    return check_limit(client_id, default_rate_, default_burst_);
}

RateLimitResult RateLimiter::check_limit(const std::string& client_id, 
                                        uint64_t requests_per_second, 
                                        uint64_t burst) {
    RateLimitResult result;
    
    if (!enabled_) {
        result.allowed = true;
        return result;
    }
    
    // Check global limit first
    auto global_result = check_global_limit();
    if (!global_result.allowed) {
        return global_result;
    }
    
    // Check per-client limit
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto& state = clients_[client_id];
    
    if (state.rate != requests_per_second || state.burst != burst) {
        state.rate = requests_per_second;
        state.burst = burst;
        state.tokens = burst;
        state.last_refill = now();
    }
    
    if (!refill_tokens(state, requests_per_second, burst)) {
        result.allowed = false;
        result.message = "Rate limit exceeded";
        result.remaining = 0;
        result.reset_after_seconds = window_seconds_;
        return result;
    }
    
    if (!consume_token(state)) {
        result.allowed = false;
        result.message = "Rate limit exceeded";
        result.remaining = state.tokens;
        result.reset_after_seconds = window_seconds_;
        return result;
    }
    
    result.allowed = true;
    result.remaining = state.tokens;
    result.reset_after_seconds = window_seconds_;
    return result;
}

RateLimitResult RateLimiter::check_global_limit() {
    RateLimitResult result;
    
    if (!enabled_) {
        result.allowed = true;
        return result;
    }
    
    std::lock_guard<std::mutex> lock(global_mutex_);
    
    // Refill global tokens
    uint64_t elapsed = seconds_since(global_last_refill_);
    if (elapsed > 0) {
        uint64_t tokens_to_add = global_rate_ * elapsed;
        global_tokens_ = std::min(global_burst_, global_tokens_ + tokens_to_add);
        global_last_refill_ = now();
    }
    
    if (global_tokens_ > 0) {
        global_tokens_--;
        result.allowed = true;
        result.remaining = global_tokens_;
    } else {
        result.allowed = false;
        result.message = "Global rate limit exceeded";
        result.remaining = 0;
    }
    
    result.reset_after_seconds = window_seconds_;
    return result;
}

bool RateLimiter::check_connection_limit(const std::string& client_id, uint64_t max_connections) {
    if (!enabled_) {
        return true;
    }
    
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto& state = clients_[client_id];
    
    return state.active_connections.load() < max_connections;
}

void RateLimiter::record_connection(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_[client_id].active_connections++;
}

void RateLimiter::release_connection(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = clients_.find(client_id);
    if (it != clients_.end() && it->second.active_connections.load() > 0) {
        it->second.active_connections--;
    }
}

uint64_t RateLimiter::get_active_connections(const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = clients_.find(client_id);
    if (it != clients_.end()) {
        return it->second.active_connections.load();
    }
    return 0;
}

void RateLimiter::cleanup_expired_entries() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    auto it = clients_.begin();
    while (it != clients_.end()) {
        // Remove entries that haven't been used in 2x window
        if (seconds_since(it->second.last_refill) > window_seconds_ * 2) {
            it = clients_.erase(it);
        } else {
            ++it;
        }
    }
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    clients_.clear();
    
    std::lock_guard<std::mutex> global_lock(global_mutex_);
    global_tokens_ = global_burst_;
    global_last_refill_ = now();
}

bool RateLimiter::refill_tokens(ClientState& state, uint64_t rate, uint64_t burst) {
    uint64_t elapsed = seconds_since(state.last_refill);
    if (elapsed > 0) {
        uint64_t tokens_to_add = rate * elapsed;
        state.tokens = std::min(burst, state.tokens + tokens_to_add);
        state.last_refill = now();
    }
    return true;
}

bool RateLimiter::consume_token(ClientState& state) {
    if (state.tokens > 0) {
        state.tokens--;
        return true;
    }
    return false;
}

uint64_t RateLimiter::calculate_tokens(ClientState& state, uint64_t rate, uint64_t burst) {
    refill_tokens(state, rate, burst);
    return state.tokens;
}

std::chrono::system_clock::time_point RateLimiter::now() const {
    return std::chrono::system_clock::now();
}

uint64_t RateLimiter::seconds_since(const std::chrono::system_clock::time_point& time) const {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now() - time).count();
    return static_cast<uint64_t>(std::max(static_cast<decltype(elapsed)>(0), elapsed));
}

} // namespace simple_utcd

