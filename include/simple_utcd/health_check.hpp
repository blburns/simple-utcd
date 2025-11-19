/*
 * includes/simple_utcd/health_check.hpp
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
#include <atomic>
#include <mutex>

namespace simple_utcd {

/**
 * @brief Health check status
 */
enum class HealthStatus {
    UNKNOWN,
    HEALTHY,
    DEGRADED,
    UNHEALTHY
};

/**
 * @brief Health check result
 */
struct HealthCheckResult {
    HealthStatus status;
    std::string message;
    std::map<std::string, std::string> details;
    std::chrono::system_clock::time_point timestamp;
    
    HealthCheckResult() : status(HealthStatus::HEALTHY), timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Health check manager
 */
class HealthChecker {
public:
    HealthChecker();
    ~HealthChecker();

    // Perform health checks
    HealthCheckResult check_health() const;
    HealthCheckResult check_utc_health() const;
    HealthCheckResult check_dependencies() const;
    
    // Update health status
    void set_status(HealthStatus status, const std::string& message = "");
    
    // Get current status
    HealthStatus get_status() const { return current_status_; }
    
    // Export health status as JSON
    std::string export_json() const;
    
    // Export health status for HTTP endpoint
    std::string export_http() const;
    
    // Dependency monitoring (v0.3.1)
    void register_dependency(const std::string& name, bool required = true);
    void unregister_dependency(const std::string& name);
    void update_dependency_status(const std::string& name, HealthStatus status, const std::string& message = "");
    HealthCheckResult check_dependency(const std::string& name) const;
    std::map<std::string, HealthStatus> get_all_dependency_status() const;
    
    // Health status aggregation
    HealthStatus aggregate_health_status() const;

private:
    std::atomic<HealthStatus> current_status_;
    std::string status_message_;
    std::chrono::system_clock::time_point last_check_;
    mutable std::mutex status_mutex_;
    
    // Dependency tracking (v0.3.1)
    struct DependencyInfo {
        std::string name;
        bool required;
        HealthStatus status;
        std::string message;
        std::chrono::system_clock::time_point last_update;
        
        DependencyInfo() : required(true), status(HealthStatus::UNKNOWN) {}
        DependencyInfo(const std::string& n, bool req) 
            : name(n), required(req), status(HealthStatus::UNKNOWN) {}
    };
    std::map<std::string, DependencyInfo> dependencies_;
    mutable std::mutex dependencies_mutex_;
    
    std::string status_to_string(HealthStatus status) const;
};

} // namespace simple_utcd

