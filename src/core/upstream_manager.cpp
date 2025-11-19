/*
 * src/core/upstream_manager.cpp
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

#include "simple_utcd/upstream_manager.hpp"
#include <algorithm>
#include <random>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>

namespace simple_utcd {

UpstreamManager::UpstreamManager()
    : strategy_(SelectionStrategy::HEALTH_BASED)
    , health_check_interval_seconds_(60)
    , failover_threshold_(3)
    , recovery_threshold_(2)
    , timeout_ms_(1000)
    , current_round_robin_index_(0)
{
}

UpstreamManager::~UpstreamManager() {
}

void UpstreamManager::set_selection_strategy(SelectionStrategy strategy) {
    strategy_ = strategy;
}

void UpstreamManager::set_health_check_interval(uint64_t interval_seconds) {
    health_check_interval_seconds_ = interval_seconds;
}

void UpstreamManager::set_failover_threshold(uint64_t consecutive_failures) {
    failover_threshold_ = consecutive_failures;
}

void UpstreamManager::set_recovery_threshold(uint64_t consecutive_successes) {
    recovery_threshold_ = consecutive_successes;
}

void UpstreamManager::set_timeout(uint64_t timeout_ms) {
    timeout_ms_ = timeout_ms;
}

bool UpstreamManager::add_server(const std::string& address, int port, int priority) {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    
    UpstreamServer server;
    server.address = address;
    server.port = port;
    server.priority = priority;
    server.status = ServerStatus::UNKNOWN;
    server.enabled = true;
    server.last_check = now();
    
    servers_[address] = server;
    return true;
}

bool UpstreamManager::remove_server(const std::string& address) {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    return servers_.erase(address) > 0;
}

bool UpstreamManager::enable_server(const std::string& address) {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    auto it = servers_.find(address);
    if (it != servers_.end()) {
        it->second.enabled = true;
        return true;
    }
    return false;
}

bool UpstreamManager::disable_server(const std::string& address) {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    auto it = servers_.find(address);
    if (it != servers_.end()) {
        it->second.enabled = false;
        return true;
    }
    return false;
}

void UpstreamManager::clear_servers() {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    servers_.clear();
}

std::vector<UpstreamServer> UpstreamManager::get_servers() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    std::vector<UpstreamServer> result;
    for (const auto& pair : servers_) {
        result.push_back(pair.second);
    }
    return result;
}

UpstreamServer* UpstreamManager::select_server() {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    
    if (servers_.empty()) {
        return nullptr;
    }
    
    switch (strategy_) {
        case SelectionStrategy::ROUND_ROBIN:
            return select_round_robin();
        case SelectionStrategy::LEAST_LATENCY:
            return select_least_latency();
        case SelectionStrategy::HEALTH_BASED:
            return select_health_based();
        case SelectionStrategy::PRIORITY:
            return select_priority();
        default:
            return select_health_based();
    }
}

UpstreamServer* UpstreamManager::get_primary_server() {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    
    UpstreamServer* best = nullptr;
    for (auto& pair : servers_) {
        if (!pair.second.enabled) continue;
        if (pair.second.status == ServerStatus::FAILED) continue;
        
        if (!best || pair.second.priority > best->priority) {
            best = &pair.second;
        } else if (pair.second.priority == best->priority &&
                   pair.second.status == ServerStatus::HEALTHY &&
                   best->status != ServerStatus::HEALTHY) {
            best = &pair.second;
        }
    }
    
    return best;
}

UpstreamServer* UpstreamManager::get_backup_server() {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    
    UpstreamServer* primary = get_primary_server();
    if (!primary) return nullptr;
    
    UpstreamServer* backup = nullptr;
    for (auto& pair : servers_) {
        if (!pair.second.enabled) continue;
        if (&pair.second == primary) continue;
        if (pair.second.status == ServerStatus::FAILED) continue;
        
        if (!backup || pair.second.priority > backup->priority) {
            backup = &pair.second;
        }
    }
    
    return backup;
}

bool UpstreamManager::check_server_health(const std::string& address) {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    auto it = servers_.find(address);
    if (it == servers_.end()) {
        return false;
    }
    
    return perform_health_check(it->second);
}

void UpstreamManager::check_all_servers_health() {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    
    for (auto& pair : servers_) {
        if (pair.second.enabled) {
            uint64_t elapsed = seconds_since(pair.second.last_check);
            if (elapsed >= health_check_interval_seconds_) {
                perform_health_check(pair.second);
            }
        }
    }
}

void UpstreamManager::record_success(const std::string& address, uint64_t response_time_ms) {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    auto it = servers_.find(address);
    if (it != servers_.end()) {
        auto& server = it->second;
        server.success_count++;
        server.last_success = now();
        server.response_time_ms = response_time_ms;
        update_server_health(server);
    }
}

void UpstreamManager::record_failure(const std::string& address) {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    auto it = servers_.find(address);
    if (it != servers_.end()) {
        auto& server = it->second;
        server.failure_count++;
        server.last_failure = now();
        update_server_health(server);
    }
}

ServerStatus UpstreamManager::get_server_status(const std::string& address) const {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    auto it = servers_.find(address);
    if (it != servers_.end()) {
        return it->second.status;
    }
    return ServerStatus::UNKNOWN;
}

uint64_t UpstreamManager::get_server_response_time(const std::string& address) const {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    auto it = servers_.find(address);
    if (it != servers_.end()) {
        return it->second.response_time_ms;
    }
    return 0;
}

bool UpstreamManager::is_server_available(const std::string& address) const {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    auto it = servers_.find(address);
    if (it != servers_.end()) {
        const auto& server = it->second;
        return server.enabled && 
               server.status != ServerStatus::FAILED &&
               server.status != ServerStatus::UNHEALTHY;
    }
    return false;
}

bool UpstreamManager::has_available_servers() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    for (const auto& pair : servers_) {
        if (is_server_available(pair.first)) {
            return true;
        }
    }
    return false;
}

size_t UpstreamManager::get_healthy_server_count() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    size_t count = 0;
    for (const auto& pair : servers_) {
        if (pair.second.status == ServerStatus::HEALTHY && pair.second.enabled) {
            count++;
        }
    }
    return count;
}

size_t UpstreamManager::get_total_server_count() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    return servers_.size();
}

void UpstreamManager::update_server_status() {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    
    for (auto& pair : servers_) {
        update_server_health(pair.second);
    }
}

bool UpstreamManager::perform_health_check(UpstreamServer& server) {
    server.last_check = now();
    
    // Simple connectivity check
    uint64_t response_time = measure_response_time(server.address, server.port);
    
    if (response_time > 0 && response_time < timeout_ms_) {
        server.response_time_ms = response_time;
        server.success_count++;
        server.last_success = now();
        update_server_health(server);
        return true;
    } else {
        server.failure_count++;
        server.last_failure = now();
        update_server_health(server);
        return false;
    }
}

uint64_t UpstreamManager::measure_response_time(const std::string& address, int port) {
    // Simple TCP connect test
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return 0;
    }
    
    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr) <= 0) {
        close(sock);
        return 0;
    }
    
    auto start = std::chrono::steady_clock::now();
    int result = connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (result == 0 || errno == EINPROGRESS) {
        // Connection successful or in progress
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        close(sock);
        return duration.count();
    }
    
    close(sock);
    return 0;
}

UpstreamServer* UpstreamManager::select_round_robin() {
    if (servers_.empty()) return nullptr;
    
    std::vector<UpstreamServer*> available;
    for (auto& pair : servers_) {
        if (is_server_available(pair.first)) {
            available.push_back(&pair.second);
        }
    }
    
    if (available.empty()) return nullptr;
    
    size_t index = current_round_robin_index_.fetch_add(1) % available.size();
    return available[index];
}

UpstreamServer* UpstreamManager::select_least_latency() {
    UpstreamServer* best = nullptr;
    uint64_t best_latency = UINT64_MAX;
    
    for (auto& pair : servers_) {
        if (!is_server_available(pair.first)) continue;
        
        if (pair.second.response_time_ms > 0 && 
            pair.second.response_time_ms < best_latency) {
            best_latency = pair.second.response_time_ms;
            best = &pair.second;
        }
    }
    
    return best ? best : select_health_based();
}

UpstreamServer* UpstreamManager::select_health_based() {
    UpstreamServer* best = nullptr;
    
    for (auto& pair : servers_) {
        if (!is_server_available(pair.first)) continue;
        
        if (!best) {
            best = &pair.second;
            continue;
        }
        
        // Prefer HEALTHY over DEGRADED over others
        if (pair.second.status == ServerStatus::HEALTHY &&
            best->status != ServerStatus::HEALTHY) {
            best = &pair.second;
        } else if (pair.second.status == best->status) {
            // If same status, prefer lower response time
            if (pair.second.response_time_ms > 0 &&
                (best->response_time_ms == 0 || 
                 pair.second.response_time_ms < best->response_time_ms)) {
                best = &pair.second;
            }
        }
    }
    
    return best;
}

UpstreamServer* UpstreamManager::select_priority() {
    UpstreamServer* best = nullptr;
    
    for (auto& pair : servers_) {
        if (!is_server_available(pair.first)) continue;
        
        if (!best) {
            best = &pair.second;
            continue;
        }
        
        if (pair.second.priority > best->priority) {
            best = &pair.second;
        } else if (pair.second.priority == best->priority) {
            // Same priority, prefer healthier server
            if (pair.second.status == ServerStatus::HEALTHY &&
                best->status != ServerStatus::HEALTHY) {
                best = &pair.second;
            }
        }
    }
    
    return best;
}

void UpstreamManager::update_server_health(UpstreamServer& server) {
    if (should_failover(server)) {
        server.status = ServerStatus::FAILED;
    } else if (should_recover(server)) {
        if (server.response_time_ms < 100) {
            server.status = ServerStatus::HEALTHY;
        } else if (server.response_time_ms < 500) {
            server.status = ServerStatus::DEGRADED;
        } else {
            server.status = ServerStatus::UNHEALTHY;
        }
    } else {
        // Update based on current metrics
        if (server.response_time_ms == 0) {
            server.status = ServerStatus::UNKNOWN;
        } else if (server.response_time_ms < 100) {
            server.status = ServerStatus::HEALTHY;
        } else if (server.response_time_ms < 500) {
            server.status = ServerStatus::DEGRADED;
        } else {
            server.status = ServerStatus::UNHEALTHY;
        }
    }
}

bool UpstreamManager::should_failover(const UpstreamServer& server) const {
    return server.failure_count >= failover_threshold_ &&
           server.success_count == 0;
}

bool UpstreamManager::should_recover(const UpstreamServer& server) const {
    if (server.status == ServerStatus::FAILED) {
        // Check if we have enough consecutive successes
        uint64_t recent_successes = 0;
        auto elapsed = seconds_since(server.last_success);
        if (elapsed < 60) {  // Within last minute
            recent_successes = server.success_count;
        }
        return recent_successes >= recovery_threshold_;
    }
    return false;
}

std::chrono::system_clock::time_point UpstreamManager::now() const {
    return std::chrono::system_clock::now();
}

uint64_t UpstreamManager::seconds_since(const std::chrono::system_clock::time_point& time) const {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now() - time).count();
    return static_cast<uint64_t>(std::max(static_cast<decltype(elapsed)>(0), elapsed));
}

} // namespace simple_utcd

