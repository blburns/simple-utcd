/*
 * includes/simple_utcd/graceful_degradation.hpp
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
#include <set>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace simple_utcd {

/**
 * @brief Service degradation level
 */
enum class DegradationLevel {
    NORMAL,      // All features available
    DEGRADED,    // Some features disabled
    LIMITED,     // Only critical features
    EMERGENCY    // Minimal functionality
};

/**
 * @brief Service priority
 */
enum class ServicePriority {
    CRITICAL,    // Must always be available
    HIGH,        // Important but can be limited
    NORMAL,      // Standard priority
    LOW          // Can be disabled under stress
};

/**
 * @brief Service feature
 */
struct ServiceFeature {
    std::string name;
    ServicePriority priority;
    bool enabled;
    bool required;
    
    ServiceFeature() : priority(ServicePriority::NORMAL), enabled(true), required(false) {}
    ServiceFeature(const std::string& n, ServicePriority p, bool req = false)
        : name(n), priority(p), enabled(true), required(req) {}
};

/**
 * @brief Graceful degradation manager
 */
class GracefulDegradation {
public:
    GracefulDegradation();
    ~GracefulDegradation();

    // Configuration
    void set_degradation_level(DegradationLevel level);
    DegradationLevel get_degradation_level() const { return current_level_; }
    
    void set_resource_thresholds(uint64_t max_memory_mb, uint64_t max_cpu_percent, 
                                 uint64_t max_connections);
    void set_health_threshold(double min_health_score);
    
    // Feature management
    bool register_feature(const std::string& name, ServicePriority priority, bool required = false);
    bool unregister_feature(const std::string& name);
    bool is_feature_enabled(const std::string& name) const;
    void enable_feature(const std::string& name);
    void disable_feature(const std::string& name);
    
    // Resource monitoring
    void update_resource_usage(uint64_t memory_mb, double cpu_percent, uint64_t connections);
    void update_health_score(double health_score);
    
    // Degradation decisions
    DegradationLevel evaluate_degradation_level();
    bool should_disable_feature(const std::string& name) const;
    std::set<std::string> get_enabled_features() const;
    std::set<std::string> get_disabled_features() const;
    
    // Status
    bool is_degraded() const { return current_level_ != DegradationLevel::NORMAL; }
    std::string get_degradation_reason() const { return degradation_reason_; }

private:
    DegradationLevel current_level_;
    std::string degradation_reason_;
    
    // Resource thresholds
    uint64_t max_memory_mb_;
    uint64_t max_cpu_percent_;
    uint64_t max_connections_;
    double min_health_score_;
    
    // Current resource usage
    std::atomic<uint64_t> current_memory_mb_;
    std::atomic<double> current_cpu_percent_;
    std::atomic<uint64_t> current_connections_;
    std::atomic<double> current_health_score_;
    
    // Feature registry
    std::map<std::string, ServiceFeature> features_;
    mutable std::mutex features_mutex_;
    
    // Degradation logic
    DegradationLevel calculate_degradation_level() const;
    void apply_degradation_level(DegradationLevel level);
    bool should_disable_by_priority(ServicePriority priority, DegradationLevel level) const;
};

} // namespace simple_utcd

