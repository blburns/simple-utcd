/*
 * src/core/health_check.cpp
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

#include "simple_utcd/health_check.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cmath>

namespace simple_utcd {

HealthChecker::HealthChecker()
    : current_status_(HealthStatus::HEALTHY)
    , last_check_(std::chrono::system_clock::now())
{
}

HealthChecker::~HealthChecker() {
}

HealthCheckResult HealthChecker::check_health() const {
    HealthCheckResult result;
    {
        std::lock_guard<std::mutex> lock(status_mutex_);
        result.status = current_status_.load();
        result.message = status_message_;
    }
    result.timestamp = std::chrono::system_clock::now();
    
    // Add UTC-specific health check
    auto utc_result = check_utc_health();
    result.details["utc"] = utc_result.message;
    
    // Add dependency checks
    auto deps_result = check_dependencies();
    result.details["dependencies"] = deps_result.message;
    
    // Overall status is worst of all checks
    if (utc_result.status == HealthStatus::UNHEALTHY || deps_result.status == HealthStatus::UNHEALTHY) {
        result.status = HealthStatus::UNHEALTHY;
    } else if (utc_result.status == HealthStatus::DEGRADED || deps_result.status == HealthStatus::DEGRADED) {
        if (result.status != HealthStatus::UNHEALTHY) {
            result.status = HealthStatus::DEGRADED;
        }
    }
    
    return result;
}

HealthCheckResult HealthChecker::check_utc_health() const {
    HealthCheckResult result;
    
    // Check if system time is available
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    if (time_t == -1) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "System time unavailable";
    } else {
        result.status = HealthStatus::HEALTHY;
        result.message = "UTC time service operational";
        
        // Check time is reasonable (not too far in past/future)
    auto current_time = std::time(nullptr);
    long time_diff = static_cast<long>(time_t) - static_cast<long>(current_time);
    if (std::abs(time_diff) > 3600) {
        result.status = HealthStatus::DEGRADED;
        result.message = "Time synchronization may be off";
    }
    }
    
    result.timestamp = std::chrono::system_clock::now();
    return result;
}

HealthCheckResult HealthChecker::check_dependencies() const {
    HealthCheckResult result;
    
    std::lock_guard<std::mutex> lock(dependencies_mutex_);
    
    if (dependencies_.empty()) {
        result.status = HealthStatus::HEALTHY;
        result.message = "No dependencies registered";
        result.timestamp = std::chrono::system_clock::now();
        return result;
    }
    
    // Check all dependencies
    bool has_degraded = false;
    bool has_unhealthy = false;
    
    for (const auto& pair : dependencies_) {
        const auto& dep = pair.second;
        result.details[dep.name] = status_to_string(dep.status);
        
        if (dep.status == HealthStatus::UNHEALTHY) {
            has_unhealthy = true;
        } else if (dep.status == HealthStatus::DEGRADED) {
            has_degraded = true;
        }
    }
    
    if (has_unhealthy) {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Some dependencies are unhealthy";
    } else if (has_degraded) {
        result.status = HealthStatus::DEGRADED;
        result.message = "Some dependencies are degraded";
    } else {
        result.status = HealthStatus::HEALTHY;
        result.message = "All dependencies operational";
    }
    
    result.timestamp = std::chrono::system_clock::now();
    return result;
}

void HealthChecker::register_dependency(const std::string& name, bool required) {
    std::lock_guard<std::mutex> lock(dependencies_mutex_);
    dependencies_[name] = DependencyInfo(name, required);
}

void HealthChecker::unregister_dependency(const std::string& name) {
    std::lock_guard<std::mutex> lock(dependencies_mutex_);
    dependencies_.erase(name);
}

void HealthChecker::update_dependency_status(const std::string& name, HealthStatus status, const std::string& message) {
    std::lock_guard<std::mutex> lock(dependencies_mutex_);
    auto it = dependencies_.find(name);
    if (it != dependencies_.end()) {
        it->second.status = status;
        it->second.message = message;
        it->second.last_update = std::chrono::system_clock::now();
    }
}

HealthCheckResult HealthChecker::check_dependency(const std::string& name) const {
    HealthCheckResult result;
    
    std::lock_guard<std::mutex> lock(dependencies_mutex_);
    auto it = dependencies_.find(name);
    if (it != dependencies_.end()) {
        result.status = it->second.status;
        result.message = it->second.message;
    } else {
        result.status = HealthStatus::UNHEALTHY;
        result.message = "Dependency not found";
    }
    
    result.timestamp = std::chrono::system_clock::now();
    return result;
}

std::map<std::string, HealthStatus> HealthChecker::get_all_dependency_status() const {
    std::lock_guard<std::mutex> lock(dependencies_mutex_);
    std::map<std::string, HealthStatus> result;
    
    for (const auto& pair : dependencies_) {
        result[pair.first] = pair.second.status;
    }
    
    return result;
}

HealthStatus HealthChecker::aggregate_health_status() const {
    HealthCheckResult main_health = check_health();
    HealthCheckResult deps_health = check_dependencies();
    
    // Worst status wins
    if (main_health.status == HealthStatus::UNHEALTHY || 
        deps_health.status == HealthStatus::UNHEALTHY) {
        return HealthStatus::UNHEALTHY;
    } else if (main_health.status == HealthStatus::DEGRADED ||
               deps_health.status == HealthStatus::DEGRADED) {
        return HealthStatus::DEGRADED;
    }
    
    return HealthStatus::HEALTHY;
}

void HealthChecker::set_status(HealthStatus status, const std::string& message) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    current_status_ = status;
    status_message_ = message;
    last_check_ = std::chrono::system_clock::now();
}

std::string HealthChecker::export_json() const {
    auto result = check_health();
    
    auto time_t = std::chrono::system_clock::to_time_t(result.timestamp);
    std::tm* tm = std::gmtime(&time_t);
    
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"status\": \"" << status_to_string(result.status) << "\",\n";
    ss << "  \"message\": \"" << result.message << "\",\n";
    
    if (tm) {
        ss << "  \"timestamp\": \"";
        ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%SZ");
        ss << "\",\n";
    } else {
        ss << "  \"timestamp\": \"unknown\",\n";
    }
    
    ss << "  \"details\": {\n";
    
    bool first = true;
    for (const auto& pair : result.details) {
        if (!first) ss << ",\n";
        ss << "    \"" << pair.first << "\": \"" << pair.second << "\"";
        first = false;
    }
    
    ss << "\n  }\n";
    ss << "}\n";
    
    return ss.str();
}

std::string HealthChecker::export_http() const {
    auto result = check_health();
    
    std::ostringstream ss;
    ss << "HTTP/1.1 ";
    
    switch (result.status) {
        case HealthStatus::UNKNOWN:
            ss << "503 Service Unavailable";
            break;
        case HealthStatus::HEALTHY:
            ss << "200 OK";
            break;
        case HealthStatus::DEGRADED:
            ss << "200 OK"; // Still OK, but degraded
            break;
        case HealthStatus::UNHEALTHY:
            ss << "503 Service Unavailable";
            break;
    }
    
    ss << "\r\n";
    ss << "Content-Type: application/json\r\n";
    ss << "Content-Length: " << export_json().length() << "\r\n";
    ss << "\r\n";
    ss << export_json();
    
    return ss.str();
}

std::string HealthChecker::status_to_string(HealthStatus status) const {
    switch (status) {
        case HealthStatus::HEALTHY:
            return "healthy";
        case HealthStatus::DEGRADED:
            return "degraded";
        case HealthStatus::UNHEALTHY:
            return "unhealthy";
        default:
            return "unknown";
    }
}

} // namespace simple_utcd

