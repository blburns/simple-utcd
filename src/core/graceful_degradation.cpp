/*
 * src/core/graceful_degradation.cpp
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

#include "simple_utcd/graceful_degradation.hpp"
#include <algorithm>

namespace simple_utcd {

GracefulDegradation::GracefulDegradation()
    : current_level_(DegradationLevel::NORMAL)
    , max_memory_mb_(1024)
    , max_cpu_percent_(80.0)
    , max_connections_(1000)
    , min_health_score_(0.5)
    , current_memory_mb_(0)
    , current_cpu_percent_(0.0)
    , current_connections_(0)
    , current_health_score_(1.0)
{
}

GracefulDegradation::~GracefulDegradation() {
}

void GracefulDegradation::set_degradation_level(DegradationLevel level) {
    current_level_ = level;
    apply_degradation_level(level);
}

void GracefulDegradation::set_resource_thresholds(uint64_t max_memory_mb, 
                                                   uint64_t max_cpu_percent,
                                                   uint64_t max_connections) {
    max_memory_mb_ = max_memory_mb;
    max_cpu_percent_ = max_cpu_percent;
    max_connections_ = max_connections;
}

void GracefulDegradation::set_health_threshold(double min_health_score) {
    min_health_score_ = min_health_score;
}

bool GracefulDegradation::register_feature(const std::string& name, 
                                          ServicePriority priority, 
                                          bool required) {
    std::lock_guard<std::mutex> lock(features_mutex_);
    
    ServiceFeature feature(name, priority, required);
    features_[name] = feature;
    return true;
}

bool GracefulDegradation::unregister_feature(const std::string& name) {
    std::lock_guard<std::mutex> lock(features_mutex_);
    return features_.erase(name) > 0;
}

bool GracefulDegradation::is_feature_enabled(const std::string& name) const {
    std::lock_guard<std::mutex> lock(features_mutex_);
    auto it = features_.find(name);
    if (it != features_.end()) {
        return it->second.enabled && !should_disable_by_priority(it->second.priority, current_level_);
    }
    return false;
}

void GracefulDegradation::enable_feature(const std::string& name) {
    std::lock_guard<std::mutex> lock(features_mutex_);
    auto it = features_.find(name);
    if (it != features_.end()) {
        it->second.enabled = true;
    }
}

void GracefulDegradation::disable_feature(const std::string& name) {
    std::lock_guard<std::mutex> lock(features_mutex_);
    auto it = features_.find(name);
    if (it != features_.end() && !it->second.required) {
        it->second.enabled = false;
    }
}

void GracefulDegradation::update_resource_usage(uint64_t memory_mb, 
                                                double cpu_percent, 
                                                uint64_t connections) {
    current_memory_mb_ = memory_mb;
    current_cpu_percent_ = cpu_percent;
    current_connections_ = connections;
    
    // Re-evaluate degradation level
    DegradationLevel new_level = evaluate_degradation_level();
    if (new_level != current_level_) {
        set_degradation_level(new_level);
    }
}

void GracefulDegradation::update_health_score(double health_score) {
    current_health_score_ = health_score;
    
    // Re-evaluate degradation level
    DegradationLevel new_level = evaluate_degradation_level();
    if (new_level != current_level_) {
        set_degradation_level(new_level);
    }
}

DegradationLevel GracefulDegradation::evaluate_degradation_level() {
    DegradationLevel calculated = calculate_degradation_level();
    
    if (calculated != current_level_) {
        degradation_reason_ = "Resource constraints or health degradation detected";
    }
    
    return calculated;
}

bool GracefulDegradation::should_disable_feature(const std::string& name) const {
    std::lock_guard<std::mutex> lock(features_mutex_);
    auto it = features_.find(name);
    if (it != features_.end()) {
        if (it->second.required) {
            return false;  // Never disable required features
        }
        return should_disable_by_priority(it->second.priority, current_level_);
    }
    return false;
}

std::set<std::string> GracefulDegradation::get_enabled_features() const {
    std::lock_guard<std::mutex> lock(features_mutex_);
    std::set<std::string> enabled;
    
    for (const auto& pair : features_) {
        if (is_feature_enabled(pair.first)) {
            enabled.insert(pair.first);
        }
    }
    
    return enabled;
}

std::set<std::string> GracefulDegradation::get_disabled_features() const {
    std::lock_guard<std::mutex> lock(features_mutex_);
    std::set<std::string> disabled;
    
    for (const auto& pair : features_) {
        if (!is_feature_enabled(pair.first)) {
            disabled.insert(pair.first);
        }
    }
    
    return disabled;
}

DegradationLevel GracefulDegradation::calculate_degradation_level() const {
    // Check resource constraints
    bool memory_high = current_memory_mb_ > max_memory_mb_ * 0.9;
    bool cpu_high = current_cpu_percent_ > max_cpu_percent_ * 0.9;
    bool connections_high = current_connections_ > max_connections_ * 0.9;
    bool health_low = current_health_score_ < min_health_score_;
    
    // Emergency: Critical resource exhaustion
    if (memory_high && cpu_high && connections_high) {
        return DegradationLevel::EMERGENCY;
    }
    
    // Limited: Multiple constraints
    if ((memory_high || cpu_high || connections_high) && health_low) {
        return DegradationLevel::LIMITED;
    }
    
    // Degraded: Single constraint or health issue
    if (memory_high || cpu_high || connections_high || health_low) {
        return DegradationLevel::DEGRADED;
    }
    
    return DegradationLevel::NORMAL;
}

void GracefulDegradation::apply_degradation_level(DegradationLevel level) {
    std::lock_guard<std::mutex> lock(features_mutex_);
    
    for (auto& pair : features_) {
        if (pair.second.required) {
            pair.second.enabled = true;  // Always enable required features
        } else {
            pair.second.enabled = !should_disable_by_priority(pair.second.priority, level);
        }
    }
}

bool GracefulDegradation::should_disable_by_priority(ServicePriority priority, 
                                                     DegradationLevel level) const {
    switch (level) {
        case DegradationLevel::NORMAL:
            return false;  // Nothing disabled
        case DegradationLevel::DEGRADED:
            return priority == ServicePriority::LOW;
        case DegradationLevel::LIMITED:
            return priority == ServicePriority::LOW || priority == ServicePriority::NORMAL;
        case DegradationLevel::EMERGENCY:
            return priority != ServicePriority::CRITICAL;
        default:
            return false;
    }
}

} // namespace simple_utcd

