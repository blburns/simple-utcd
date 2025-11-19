/*
 * includes/simple_utcd/upstream_manager.hpp
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
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace simple_utcd {

/**
 * @brief Upstream server status
 */
enum class ServerStatus {
    UNKNOWN,
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    FAILED
};

/**
 * @brief Server selection strategy
 */
enum class SelectionStrategy {
    ROUND_ROBIN,
    LEAST_LATENCY,
    HEALTH_BASED,
    PRIORITY
};

/**
 * @brief Upstream server information
 */
struct UpstreamServer {
    std::string address;
    int port;
    int priority;
    ServerStatus status;
    uint64_t response_time_ms;
    uint64_t success_count;
    uint64_t failure_count;
    std::chrono::system_clock::time_point last_check;
    std::chrono::system_clock::time_point last_success;
    std::chrono::system_clock::time_point last_failure;
    bool enabled;
    
    UpstreamServer() : port(37), priority(0), status(ServerStatus::UNKNOWN),
                      response_time_ms(0), success_count(0), failure_count(0),
                      enabled(true) {}
};

/**
 * @brief Upstream server manager with failover support
 */
class UpstreamManager {
public:
    UpstreamManager();
    ~UpstreamManager();

    // Configuration
    void set_selection_strategy(SelectionStrategy strategy);
    void set_health_check_interval(uint64_t interval_seconds);
    void set_failover_threshold(uint64_t consecutive_failures);
    void set_recovery_threshold(uint64_t consecutive_successes);
    void set_timeout(uint64_t timeout_ms);
    
    // Server management
    bool add_server(const std::string& address, int port = 37, int priority = 0);
    bool remove_server(const std::string& address);
    bool enable_server(const std::string& address);
    bool disable_server(const std::string& address);
    void clear_servers();
    std::vector<UpstreamServer> get_servers() const;
    
    // Server selection
    UpstreamServer* select_server();
    UpstreamServer* get_primary_server();
    UpstreamServer* get_backup_server();
    
    // Health monitoring
    bool check_server_health(const std::string& address);
    void check_all_servers_health();
    void record_success(const std::string& address, uint64_t response_time_ms);
    void record_failure(const std::string& address);
    
    // Status
    ServerStatus get_server_status(const std::string& address) const;
    uint64_t get_server_response_time(const std::string& address) const;
    bool is_server_available(const std::string& address) const;
    
    // Failover
    bool has_available_servers() const;
    size_t get_healthy_server_count() const;
    size_t get_total_server_count() const;
    
    // Automatic recovery
    void update_server_status();

private:
    SelectionStrategy strategy_;
    uint64_t health_check_interval_seconds_;
    uint64_t failover_threshold_;
    uint64_t recovery_threshold_;
    uint64_t timeout_ms_;
    
    std::map<std::string, UpstreamServer> servers_;
    mutable std::mutex servers_mutex_;
    
    std::atomic<size_t> current_round_robin_index_;
    
    // Health check
    bool perform_health_check(UpstreamServer& server);
    uint64_t measure_response_time(const std::string& address, int port);
    
    // Selection algorithms
    UpstreamServer* select_round_robin();
    UpstreamServer* select_least_latency();
    UpstreamServer* select_health_based();
    UpstreamServer* select_priority();
    
    // Status management
    void update_server_health(UpstreamServer& server);
    bool should_failover(const UpstreamServer& server) const;
    bool should_recover(const UpstreamServer& server) const;
    
    // Time utilities
    std::chrono::system_clock::time_point now() const;
    uint64_t seconds_since(const std::chrono::system_clock::time_point& time) const;
};

} // namespace simple_utcd

