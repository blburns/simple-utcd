/*
 * src/core/metrics.cpp
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

#include "simple_utcd/metrics.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <memory>

namespace simple_utcd {

Metrics::Metrics() {
}

Metrics::~Metrics() {
}

void Metrics::increment_counter(const std::string& name, const std::map<std::string, std::string>& labels) {
    std::string key = make_metric_key(name, labels);
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    if (metrics_.find(key) == metrics_.end()) {
        metrics_[key] = std::make_unique<MetricValue>();
    }
    
    metrics_[key]->counter++;
}

void Metrics::add_counter(const std::string& name, double value, const std::map<std::string, std::string>& labels) {
    std::string key = make_metric_key(name, labels);
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    if (metrics_.find(key) == metrics_.end()) {
        metrics_[key] = std::make_unique<MetricValue>();
    }
    
    metrics_[key]->counter += static_cast<uint64_t>(value);
}

uint64_t Metrics::get_counter(const std::string& name, const std::map<std::string, std::string>& labels) const {
    std::string key = make_metric_key(name, labels);
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(key);
    if (it != metrics_.end()) {
        return it->second->counter.load();
    }
    return 0;
}

void Metrics::set_gauge(const std::string& name, double value, const std::map<std::string, std::string>& labels) {
    std::string key = make_metric_key(name, labels);
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    if (metrics_.find(key) == metrics_.end()) {
        metrics_[key] = std::make_unique<MetricValue>();
    }
    
    metrics_[key]->gauge.store(value);
}

void Metrics::add_gauge(const std::string& name, double value, const std::map<std::string, std::string>& labels) {
    std::string key = make_metric_key(name, labels);
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    if (metrics_.find(key) == metrics_.end()) {
        metrics_[key] = std::make_unique<MetricValue>();
    }
    
    double current = metrics_[key]->gauge.load();
    metrics_[key]->gauge.store(current + value);
}

double Metrics::get_gauge(const std::string& name, const std::map<std::string, std::string>& labels) const {
    std::string key = make_metric_key(name, labels);
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    auto it = metrics_.find(key);
    if (it != metrics_.end()) {
        return it->second->gauge.load();
    }
    return 0.0;
}

void Metrics::observe_histogram(const std::string& name, double value, const std::map<std::string, std::string>& labels) {
    std::string key = make_metric_key(name, labels);
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    if (metrics_.find(key) == metrics_.end()) {
        metrics_[key] = std::make_unique<MetricValue>();
    }
    
    std::lock_guard<std::mutex> value_lock(metrics_[key]->mutex);
    metrics_[key]->histogram_values.push_back(value);
    
    // Keep only last 1000 values
    if (metrics_[key]->histogram_values.size() > 1000) {
        metrics_[key]->histogram_values.erase(metrics_[key]->histogram_values.begin());
    }
}

std::string Metrics::export_prometheus() const {
    std::ostringstream ss;
    
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    
    for (const auto& pair : metrics_) {
        const auto& key = pair.first;
        const auto& metric = pair.second;
        // Extract metric name and labels from key
        size_t label_pos = key.find('{');
        std::string metric_name = (label_pos != std::string::npos) ? key.substr(0, label_pos) : key;
        
        // Export counter
        if (metric->counter.load() > 0) {
            ss << "# TYPE " << metric_name << " counter\n";
            ss << metric_name;
            if (label_pos != std::string::npos) {
                ss << key.substr(label_pos);
            }
            ss << " " << metric->counter.load() << "\n";
        }
        
        // Export gauge
        double gauge_val = metric->gauge.load();
        if (gauge_val != 0.0 || metric->counter.load() == 0) {
            ss << "# TYPE " << metric_name << "_gauge gauge\n";
            ss << metric_name << "_gauge";
            if (label_pos != std::string::npos) {
                ss << key.substr(label_pos);
            }
            ss << " " << std::fixed << std::setprecision(2) << gauge_val << "\n";
        }
    }
    
    return ss.str();
}

void Metrics::reset() {
    std::lock_guard<std::mutex> lock(metrics_mutex_);
    metrics_.clear();
}

std::string Metrics::make_metric_key(const std::string& name, const std::map<std::string, std::string>& labels) const {
    std::string key = name;
    
    if (!labels.empty()) {
        key += "{";
        bool first = true;
        for (const auto& pair : labels) {
            if (!first) key += ",";
            key += pair.first + "=\"" + pair.second + "\"";
            first = false;
        }
        key += "}";
    }
    
    return key;
}

PerformanceMetrics::PerformanceMetrics()
    : total_requests_(0)
    , total_responses_(0)
    , total_errors_(0)
    , total_response_time_us_(0)
    , active_connections_(0)
    , total_connections_(0)
{
}

PerformanceMetrics::~PerformanceMetrics() {
}

void PerformanceMetrics::record_request() {
    total_requests_++;
}

void PerformanceMetrics::record_response(uint64_t response_time_us) {
    total_responses_++;
    total_response_time_us_ += response_time_us;
    
    // Track recent response times
    std::lock_guard<std::mutex> lock(response_times_mutex_);
    recent_response_times_.push_back(response_time_us);
    if (recent_response_times_.size() > 100) {
        recent_response_times_.erase(recent_response_times_.begin());
    }
}

void PerformanceMetrics::record_error() {
    total_errors_++;
}

void PerformanceMetrics::update_active_connections(int count) {
    active_connections_ = count;
}

void PerformanceMetrics::update_total_connections(int count) {
    total_connections_ = count;
}

double PerformanceMetrics::get_average_response_time() const {
    uint64_t responses = total_responses_.load();
    if (responses == 0) {
        return 0.0;
    }
    return static_cast<double>(total_response_time_us_.load()) / responses / 1000.0; // Convert to milliseconds
}

std::string PerformanceMetrics::export_prometheus() const {
    std::ostringstream ss;
    
    ss << "# TYPE simple_utcd_requests_total counter\n";
    ss << "simple_utcd_requests_total " << total_requests_.load() << "\n";
    
    ss << "# TYPE simple_utcd_responses_total counter\n";
    ss << "simple_utcd_responses_total " << total_responses_.load() << "\n";
    
    ss << "# TYPE simple_utcd_errors_total counter\n";
    ss << "simple_utcd_errors_total " << total_errors_.load() << "\n";
    
    ss << "# TYPE simple_utcd_response_time_ms gauge\n";
    ss << "simple_utcd_response_time_ms " << std::fixed << std::setprecision(2) << get_average_response_time() << "\n";
    
    ss << "# TYPE simple_utcd_active_connections gauge\n";
    ss << "simple_utcd_active_connections " << active_connections_.load() << "\n";
    
    ss << "# TYPE simple_utcd_total_connections counter\n";
    ss << "simple_utcd_total_connections " << total_connections_.load() << "\n";
    
    return ss.str();
}

} // namespace simple_utcd

