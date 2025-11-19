/*
 * includes/simple_utcd/metrics.hpp
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
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

namespace simple_utcd {

/**
 * @brief Metrics collector for Prometheus-compatible metrics
 */
class Metrics {
public:
    Metrics();
    ~Metrics();

    // Counter metrics
    void increment_counter(const std::string& name, const std::map<std::string, std::string>& labels = {});
    void add_counter(const std::string& name, double value, const std::map<std::string, std::string>& labels = {});
    uint64_t get_counter(const std::string& name, const std::map<std::string, std::string>& labels = {}) const;

    // Gauge metrics
    void set_gauge(const std::string& name, double value, const std::map<std::string, std::string>& labels = {});
    void add_gauge(const std::string& name, double value, const std::map<std::string, std::string>& labels = {});
    double get_gauge(const std::string& name, const std::map<std::string, std::string>& labels = {}) const;

    // Histogram metrics
    void observe_histogram(const std::string& name, double value, const std::map<std::string, std::string>& labels = {});

    // Export metrics in Prometheus format
    std::string export_prometheus() const;

    // Reset all metrics
    void reset();

private:
    struct MetricValue {
        std::atomic<uint64_t> counter{0};
        std::atomic<double> gauge{0.0};
        std::vector<double> histogram_values;
        mutable std::mutex mutex;
    };

    std::string make_metric_key(const std::string& name, const std::map<std::string, std::string>& labels) const;
    std::map<std::string, std::unique_ptr<MetricValue>> metrics_;
    mutable std::mutex metrics_mutex_;
};

/**
 * @brief Performance metrics tracker
 */
class PerformanceMetrics {
public:
    PerformanceMetrics();
    ~PerformanceMetrics();

    // Request/response tracking
    void record_request();
    void record_response(uint64_t response_time_us);
    void record_error();

    // System resource tracking
    void update_active_connections(int count);
    void update_total_connections(int count);

    // Get metrics
    uint64_t get_total_requests() const { return total_requests_; }
    uint64_t get_total_responses() const { return total_responses_; }
    uint64_t get_total_errors() const { return total_errors_; }
    double get_average_response_time() const;
    int get_active_connections() const { return active_connections_; }
    int get_total_connections() const { return total_connections_; }

    // Export to Prometheus format
    std::string export_prometheus() const;

private:
    std::atomic<uint64_t> total_requests_;
    std::atomic<uint64_t> total_responses_;
    std::atomic<uint64_t> total_errors_;
    std::atomic<uint64_t> total_response_time_us_;
    std::atomic<int> active_connections_;
    std::atomic<int> total_connections_;
    mutable std::mutex response_times_mutex_;
    std::vector<uint64_t> recent_response_times_;
};

} // namespace simple_utcd

