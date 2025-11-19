/*
 * includes/simple_utcd/ddos_protection.hpp
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
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace simple_utcd {

/**
 * @brief DDoS protection status
 */
enum class DDoSStatus {
    NORMAL,
    WARNING,
    ATTACK_DETECTED,
    BLOCKED
};

/**
 * @brief DDoS protection result
 */
struct DDoSResult {
    bool allowed;
    DDoSStatus status;
    std::string reason;
    uint64_t block_duration_seconds;
    
    DDoSResult() : allowed(true), status(DDoSStatus::NORMAL), block_duration_seconds(0) {}
};

/**
 * @brief DDoS protection manager
 */
class DDoSProtection {
public:
    DDoSProtection();
    ~DDoSProtection();

    // Configuration
    void set_threshold(uint64_t requests_per_second);
    void set_block_duration(uint64_t seconds);
    void set_connection_limit(uint64_t max_connections_per_ip);
    void set_connection_window(uint64_t window_seconds);
    void set_anomaly_threshold(double threshold);
    
    // Request checking
    DDoSResult check_request(const std::string& client_ip);
    DDoSResult check_connection(const std::string& client_ip);
    
    // Anomaly detection
    bool detect_anomaly(const std::string& client_ip);
    void record_request(const std::string& client_ip);
    void record_connection(const std::string& client_ip);
    void record_disconnection(const std::string& client_ip);
    
    // Blocking management
    bool is_blocked(const std::string& client_ip) const;
    void block_client(const std::string& client_ip, uint64_t duration_seconds);
    void unblock_client(const std::string& client_ip);
    void clear_blocks();
    
    // Statistics
    uint64_t get_request_count(const std::string& client_ip) const;
    uint64_t get_connection_count(const std::string& client_ip) const;
    std::vector<std::string> get_blocked_ips() const;
    uint64_t get_total_blocked() const { return total_blocked_; }
    
    // Status
    bool is_enabled() const { return enabled_; }
    void set_enabled(bool enabled) { enabled_ = enabled; }
    
    // Cleanup
    void cleanup_expired_entries();
    void reset();

private:
    bool enabled_;
    uint64_t threshold_;
    uint64_t block_duration_seconds_;
    uint64_t connection_limit_;
    uint64_t connection_window_seconds_;
    double anomaly_threshold_;
    
    // Request tracking
    struct ClientStats {
        std::vector<std::chrono::system_clock::time_point> request_times;
        std::vector<std::chrono::system_clock::time_point> connection_times;
        uint64_t total_requests;
        uint64_t total_connections;
        std::chrono::system_clock::time_point first_seen;
        std::chrono::system_clock::time_point last_seen;
        double anomaly_score;
        
        ClientStats() : total_requests(0), total_connections(0),
                       first_seen(std::chrono::system_clock::now()),
                       last_seen(std::chrono::system_clock::now()),
                       anomaly_score(0.0) {}
    };
    std::map<std::string, ClientStats> client_stats_;
    mutable std::mutex stats_mutex_;
    
    // Block tracking
    struct BlockEntry {
        std::chrono::system_clock::time_point blocked_at;
        std::chrono::system_clock::time_point expires_at;
        std::string reason;
        
        BlockEntry() : blocked_at(std::chrono::system_clock::now()),
                      expires_at(std::chrono::system_clock::now()) {}
    };
    std::map<std::string, BlockEntry> blocked_clients_;
    mutable std::mutex blocks_mutex_;
    
    std::atomic<uint64_t> total_blocked_;
    
    // Anomaly detection
    double calculate_anomaly_score(const ClientStats& stats) const;
    bool is_anomalous_pattern(const ClientStats& stats) const;
    
    // Rate calculation
    uint64_t get_request_rate_for_ip(const std::string& client_ip) const;
    uint64_t calculate_request_rate(const ClientStats& stats) const;
    uint64_t calculate_connection_rate(const ClientStats& stats) const;
    
    // Time utilities
    std::chrono::system_clock::time_point now() const;
    bool is_expired(const std::chrono::system_clock::time_point& time) const;
    void cleanup_old_entries(ClientStats& stats);
};

} // namespace simple_utcd

