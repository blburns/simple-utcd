/*
 * src/core/ddos_protection.cpp
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

#include "simple_utcd/ddos_protection.hpp"
#include <algorithm>
#include <cmath>

namespace simple_utcd {

DDoSProtection::DDoSProtection()
    : enabled_(false)
    , threshold_(1000)
    , block_duration_seconds_(3600)
    , connection_limit_(10)
    , connection_window_seconds_(60)
    , anomaly_threshold_(3.0)
    , total_blocked_(0)
{
}

DDoSProtection::~DDoSProtection() {
}

void DDoSProtection::set_threshold(uint64_t requests_per_second) {
    threshold_ = requests_per_second;
}

void DDoSProtection::set_block_duration(uint64_t seconds) {
    block_duration_seconds_ = seconds;
}

void DDoSProtection::set_connection_limit(uint64_t max_connections_per_ip) {
    connection_limit_ = max_connections_per_ip;
}

void DDoSProtection::set_connection_window(uint64_t window_seconds) {
    connection_window_seconds_ = window_seconds;
}

void DDoSProtection::set_anomaly_threshold(double threshold) {
    anomaly_threshold_ = threshold;
}

DDoSResult DDoSProtection::check_request(const std::string& client_ip) {
    DDoSResult result;
    
    if (!enabled_) {
        result.allowed = true;
        return result;
    }
    
    // Check if already blocked
    if (is_blocked(client_ip)) {
        result.allowed = false;
        result.status = DDoSStatus::BLOCKED;
        result.reason = "IP address is blocked";
        return result;
    }
    
    // Record request
    record_request(client_ip);
    
    // Check rate
    uint64_t rate = get_request_rate_for_ip(client_ip);
    if (rate > threshold_) {
        block_client(client_ip, block_duration_seconds_);
        result.allowed = false;
        result.status = DDoSStatus::ATTACK_DETECTED;
        result.reason = "Request rate exceeded threshold";
        result.block_duration_seconds = block_duration_seconds_;
        total_blocked_++;
        return result;
    }
    
    // Check for anomalies
    if (detect_anomaly(client_ip)) {
        block_client(client_ip, block_duration_seconds_);
        result.allowed = false;
        result.status = DDoSStatus::ATTACK_DETECTED;
        result.reason = "Anomalous traffic pattern detected";
        result.block_duration_seconds = block_duration_seconds_;
        total_blocked_++;
        return result;
    }
    
    // Warning if approaching threshold
    if (rate > threshold_ * 0.8) {
        result.status = DDoSStatus::WARNING;
        result.reason = "Request rate approaching threshold";
    } else {
        result.status = DDoSStatus::NORMAL;
    }
    
    result.allowed = true;
    return result;
}

DDoSResult DDoSProtection::check_connection(const std::string& client_ip) {
    DDoSResult result;
    
    if (!enabled_) {
        result.allowed = true;
        return result;
    }
    
    // Check if already blocked
    if (is_blocked(client_ip)) {
        result.allowed = false;
        result.status = DDoSStatus::BLOCKED;
        result.reason = "IP address is blocked";
        return result;
    }
    
    // Check connection limit
    uint64_t connections = get_connection_count(client_ip);
    if (connections >= connection_limit_) {
        block_client(client_ip, block_duration_seconds_);
        result.allowed = false;
        result.status = DDoSStatus::ATTACK_DETECTED;
        result.reason = "Connection limit exceeded";
        result.block_duration_seconds = block_duration_seconds_;
        total_blocked_++;
        return result;
    }
    
    // Record connection
    record_connection(client_ip);
    
    result.allowed = true;
    result.status = DDoSStatus::NORMAL;
    return result;
}

bool DDoSProtection::detect_anomaly(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = client_stats_.find(client_ip);
    if (it == client_stats_.end()) {
        return false;
    }
    
    auto& stats = it->second;
    stats.anomaly_score = calculate_anomaly_score(stats);
    
    return stats.anomaly_score > anomaly_threshold_ || is_anomalous_pattern(stats);
}

void DDoSProtection::record_request(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto& stats = client_stats_[client_ip];
    
    auto current_time = now();
    stats.request_times.push_back(current_time);
    stats.total_requests++;
    stats.last_seen = current_time;
    
    if (stats.total_requests == 1) {
        stats.first_seen = current_time;
    }
    
    cleanup_old_entries(stats);
}

void DDoSProtection::record_connection(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto& stats = client_stats_[client_ip];
    
    auto current_time = now();
    stats.connection_times.push_back(current_time);
    stats.total_connections++;
    stats.last_seen = current_time;
    
    if (stats.total_connections == 1) {
        stats.first_seen = current_time;
    }
    
    cleanup_old_entries(stats);
}

void DDoSProtection::record_disconnection(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = client_stats_.find(client_ip);
    if (it != client_stats_.end() && it->second.total_connections > 0) {
        it->second.total_connections--;
    }
}

bool DDoSProtection::is_blocked(const std::string& client_ip) const {
    std::lock_guard<std::mutex> lock(blocks_mutex_);
    auto it = blocked_clients_.find(client_ip);
    if (it == blocked_clients_.end()) {
        return false;
    }
    
    if (is_expired(it->second.expires_at)) {
        return false;
    }
    
    return true;
}

void DDoSProtection::block_client(const std::string& client_ip, uint64_t duration_seconds) {
    std::lock_guard<std::mutex> lock(blocks_mutex_);
    BlockEntry entry;
    entry.blocked_at = now();
    entry.expires_at = entry.blocked_at + std::chrono::seconds(duration_seconds);
    entry.reason = "DDoS protection triggered";
    blocked_clients_[client_ip] = entry;
}

void DDoSProtection::unblock_client(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(blocks_mutex_);
    blocked_clients_.erase(client_ip);
}

void DDoSProtection::clear_blocks() {
    std::lock_guard<std::mutex> lock(blocks_mutex_);
    blocked_clients_.clear();
}

uint64_t DDoSProtection::get_request_count(const std::string& client_ip) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = client_stats_.find(client_ip);
    if (it != client_stats_.end()) {
        return it->second.total_requests;
    }
    return 0;
}

uint64_t DDoSProtection::get_connection_count(const std::string& client_ip) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = client_stats_.find(client_ip);
    if (it != client_stats_.end()) {
        return it->second.total_connections;
    }
    return 0;
}

std::vector<std::string> DDoSProtection::get_blocked_ips() const {
    std::lock_guard<std::mutex> lock(blocks_mutex_);
    std::vector<std::string> blocked;
    
    for (const auto& pair : blocked_clients_) {
        if (!is_expired(pair.second.expires_at)) {
            blocked.push_back(pair.first);
        }
    }
    
    return blocked;
}

void DDoSProtection::cleanup_expired_entries() {
    // Cleanup expired blocks
    {
        std::lock_guard<std::mutex> lock(blocks_mutex_);
        auto it = blocked_clients_.begin();
        while (it != blocked_clients_.end()) {
            if (is_expired(it->second.expires_at)) {
                it = blocked_clients_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Cleanup old stats
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        for (auto& pair : client_stats_) {
            cleanup_old_entries(pair.second);
        }
    }
}

void DDoSProtection::reset() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    client_stats_.clear();
    
    std::lock_guard<std::mutex> blocks_lock(blocks_mutex_);
    blocked_clients_.clear();
    
    total_blocked_ = 0;
}

double DDoSProtection::calculate_anomaly_score(const ClientStats& stats) const {
    if (stats.total_requests == 0) {
        return 0.0;
    }
    
    // Calculate request rate
    uint64_t rate = calculate_request_rate(stats);
    
    // Calculate variance in request timing
    if (stats.request_times.size() < 2) {
        return 0.0;
    }
    
    double mean_interval = 0.0;
    for (size_t i = 1; i < stats.request_times.size(); i++) {
        auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
            stats.request_times[i] - stats.request_times[i-1]).count();
        mean_interval += interval;
    }
    mean_interval /= (stats.request_times.size() - 1);
    
    // Calculate standard deviation
    double variance = 0.0;
    for (size_t i = 1; i < stats.request_times.size(); i++) {
        auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
            stats.request_times[i] - stats.request_times[i-1]).count();
        double diff = interval - mean_interval;
        variance += diff * diff;
    }
    variance /= (stats.request_times.size() - 1);
    double stddev = std::sqrt(variance);
    
    // Low variance (regular pattern) with high rate = potential attack
    double score = 0.0;
    if (mean_interval > 0) {
        score = (rate / mean_interval) * (1.0 / (1.0 + stddev));
    }
    
    return score;
}

bool DDoSProtection::is_anomalous_pattern(const ClientStats& stats) const {
    // Check for very regular request patterns (bot-like behavior)
    if (stats.request_times.size() < 10) {
        return false;
    }
    
    // Check if requests are too regular (low variance)
    std::vector<int64_t> intervals;
    for (size_t i = 1; i < stats.request_times.size(); i++) {
        auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
            stats.request_times[i] - stats.request_times[i-1]).count();
        intervals.push_back(interval);
    }
    
    if (intervals.empty()) {
        return false;
    }
    
    double mean = 0.0;
    for (auto interval : intervals) {
        mean += interval;
    }
    mean /= intervals.size();
    
    double variance = 0.0;
    for (auto interval : intervals) {
        double diff = interval - mean;
        variance += diff * diff;
    }
    variance /= intervals.size();
    double stddev = std::sqrt(variance);
    
    // Very low standard deviation indicates regular pattern (potential bot)
    return stddev < mean * 0.1 && mean < 100;  // Less than 10% variance and < 100ms intervals
}

uint64_t DDoSProtection::get_request_rate_for_ip(const std::string& client_ip) const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    auto it = client_stats_.find(client_ip);
    if (it == client_stats_.end()) {
        return 0;
    }
    return calculate_request_rate(it->second);
}

uint64_t DDoSProtection::calculate_request_rate(const ClientStats& stats) const {
    if (stats.request_times.empty()) {
        return 0;
    }
    
    auto window_start = now() - std::chrono::seconds(connection_window_seconds_);
    uint64_t count = 0;
    
    for (const auto& time : stats.request_times) {
        if (time >= window_start) {
            count++;
        }
    }
    
    return count;
}

uint64_t DDoSProtection::calculate_connection_rate(const ClientStats& stats) const {
    if (stats.connection_times.empty()) {
        return 0;
    }
    
    auto window_start = now() - std::chrono::seconds(connection_window_seconds_);
    uint64_t count = 0;
    
    for (const auto& time : stats.connection_times) {
        if (time >= window_start) {
            count++;
        }
    }
    
    return count;
}

std::chrono::system_clock::time_point DDoSProtection::now() const {
    return std::chrono::system_clock::now();
}

bool DDoSProtection::is_expired(const std::chrono::system_clock::time_point& time) const {
    return time < now();
}

void DDoSProtection::cleanup_old_entries(ClientStats& stats) {
    auto cutoff = now() - std::chrono::seconds(connection_window_seconds_ * 2);
    
    // Remove old request times
    stats.request_times.erase(
        std::remove_if(stats.request_times.begin(), stats.request_times.end(),
            [cutoff](const std::chrono::system_clock::time_point& t) { return t < cutoff; }),
        stats.request_times.end()
    );
    
    // Remove old connection times
    stats.connection_times.erase(
        std::remove_if(stats.connection_times.begin(), stats.connection_times.end(),
            [cutoff](const std::chrono::system_clock::time_point& t) { return t < cutoff; }),
        stats.connection_times.end()
    );
}

} // namespace simple_utcd

