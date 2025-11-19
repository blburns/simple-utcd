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

private:
    std::atomic<HealthStatus> current_status_;
    std::string status_message_;
    std::chrono::system_clock::time_point last_check_;
    mutable std::mutex status_mutex_;
    
    std::string status_to_string(HealthStatus status) const;
};

} // namespace simple_utcd

