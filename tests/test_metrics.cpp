/*
 * tests/test_metrics.cpp
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

#include <gtest/gtest.h>
#include "simple_utcd/metrics.hpp"
#include <map>

using namespace simple_utcd;

class MetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test fixtures if needed
    }

    void TearDown() override {
        metrics_.reset();
    }

    Metrics metrics_;
};

// Test default constructor
TEST_F(MetricsTest, DefaultConstructor) {
    Metrics metrics;
    // Should not crash
}

// Test counter increment
TEST_F(MetricsTest, CounterIncrement) {
    std::string name = "test_counter";
    
    metrics_.increment_counter(name);
    EXPECT_EQ(metrics_.get_counter(name), 1);
    
    metrics_.increment_counter(name);
    EXPECT_EQ(metrics_.get_counter(name), 2);
}

// Test counter add
TEST_F(MetricsTest, CounterAdd) {
    std::string name = "test_counter";
    
    metrics_.add_counter(name, 5.0);
    EXPECT_EQ(metrics_.get_counter(name), 5);
    
    metrics_.add_counter(name, 3.0);
    EXPECT_EQ(metrics_.get_counter(name), 8);
}

// Test counter with labels
TEST_F(MetricsTest, CounterWithLabels) {
    std::string name = "test_counter";
    std::map<std::string, std::string> labels = {{"method", "GET"}, {"status", "200"}};
    
    metrics_.increment_counter(name, labels);
    EXPECT_EQ(metrics_.get_counter(name, labels), 1);
    
    metrics_.increment_counter(name, labels);
    EXPECT_EQ(metrics_.get_counter(name, labels), 2);
}

// Test gauge set
TEST_F(MetricsTest, GaugeSet) {
    std::string name = "test_gauge";
    
    metrics_.set_gauge(name, 10.5);
    EXPECT_DOUBLE_EQ(metrics_.get_gauge(name), 10.5);
    
    metrics_.set_gauge(name, 20.0);
    EXPECT_DOUBLE_EQ(metrics_.get_gauge(name), 20.0);
}

// Test gauge add
TEST_F(MetricsTest, GaugeAdd) {
    std::string name = "test_gauge";
    
    metrics_.set_gauge(name, 10.0);
    metrics_.add_gauge(name, 5.0);
    EXPECT_DOUBLE_EQ(metrics_.get_gauge(name), 15.0);
    
    metrics_.add_gauge(name, -3.0);
    EXPECT_DOUBLE_EQ(metrics_.get_gauge(name), 12.0);
}

// Test gauge with labels
TEST_F(MetricsTest, GaugeWithLabels) {
    std::string name = "test_gauge";
    std::map<std::string, std::string> labels = {{"instance", "server1"}};
    
    metrics_.set_gauge(name, 100.0, labels);
    EXPECT_DOUBLE_EQ(metrics_.get_gauge(name, labels), 100.0);
}

// Test histogram observe
TEST_F(MetricsTest, HistogramObserve) {
    std::string name = "test_histogram";
    
    metrics_.observe_histogram(name, 1.0);
    metrics_.observe_histogram(name, 2.0);
    metrics_.observe_histogram(name, 3.0);
    
    // Histogram values are stored internally
    // No direct getter, but should not crash
    EXPECT_TRUE(true);
}

// Test histogram with labels
TEST_F(MetricsTest, HistogramWithLabels) {
    std::string name = "test_histogram";
    std::map<std::string, std::string> labels = {{"method", "POST"}};
    
    metrics_.observe_histogram(name, 1.5, labels);
    metrics_.observe_histogram(name, 2.5, labels);
    
    // Should not crash
    EXPECT_TRUE(true);
}

// Test Prometheus export
TEST_F(MetricsTest, PrometheusExport) {
    metrics_.increment_counter("requests_total");
    metrics_.set_gauge("active_connections", 10.0);
    
    std::string prometheus = metrics_.export_prometheus();
    EXPECT_FALSE(prometheus.empty());
    EXPECT_NE(prometheus.find("requests_total"), std::string::npos);
}

// Test reset
TEST_F(MetricsTest, Reset) {
    metrics_.increment_counter("test_counter");
    metrics_.set_gauge("test_gauge", 10.0);
    
    EXPECT_GT(metrics_.get_counter("test_counter"), 0);
    
    metrics_.reset();
    
    EXPECT_EQ(metrics_.get_counter("test_counter"), 0);
    EXPECT_DOUBLE_EQ(metrics_.get_gauge("test_gauge"), 0.0);
}

// Test multiple metrics
TEST_F(MetricsTest, MultipleMetrics) {
    metrics_.increment_counter("counter1");
    metrics_.increment_counter("counter2");
    metrics_.set_gauge("gauge1", 1.0);
    metrics_.set_gauge("gauge2", 2.0);
    
    EXPECT_EQ(metrics_.get_counter("counter1"), 1);
    EXPECT_EQ(metrics_.get_counter("counter2"), 1);
    EXPECT_DOUBLE_EQ(metrics_.get_gauge("gauge1"), 1.0);
    EXPECT_DOUBLE_EQ(metrics_.get_gauge("gauge2"), 2.0);
}

// Test PerformanceMetrics default constructor
TEST_F(MetricsTest, PerformanceMetricsConstructor) {
    PerformanceMetrics perf;
    
    EXPECT_EQ(perf.get_total_requests(), 0);
    EXPECT_EQ(perf.get_total_responses(), 0);
    EXPECT_EQ(perf.get_total_errors(), 0);
    EXPECT_EQ(perf.get_active_connections(), 0);
    EXPECT_EQ(perf.get_total_connections(), 0);
}

// Test PerformanceMetrics request tracking
TEST_F(MetricsTest, PerformanceMetricsRequestTracking) {
    PerformanceMetrics perf;
    
    perf.record_request();
    EXPECT_EQ(perf.get_total_requests(), 1);
    
    perf.record_request();
    EXPECT_EQ(perf.get_total_requests(), 2);
}

// Test PerformanceMetrics response tracking
TEST_F(MetricsTest, PerformanceMetricsResponseTracking) {
    PerformanceMetrics perf;
    
    perf.record_response(1000); // 1ms
    EXPECT_EQ(perf.get_total_responses(), 1);
    
    perf.record_response(2000); // 2ms
    EXPECT_EQ(perf.get_total_responses(), 2);
    
    // Average should be calculated
    double avg = perf.get_average_response_time();
    EXPECT_GT(avg, 0.0);
}

// Test PerformanceMetrics error tracking
TEST_F(MetricsTest, PerformanceMetricsErrorTracking) {
    PerformanceMetrics perf;
    
    perf.record_error();
    EXPECT_EQ(perf.get_total_errors(), 1);
    
    perf.record_error();
    EXPECT_EQ(perf.get_total_errors(), 2);
}

// Test PerformanceMetrics connection tracking
TEST_F(MetricsTest, PerformanceMetricsConnectionTracking) {
    PerformanceMetrics perf;
    
    perf.update_active_connections(5);
    EXPECT_EQ(perf.get_active_connections(), 5);
    
    perf.update_total_connections(10);
    EXPECT_EQ(perf.get_total_connections(), 10);
}

// Test PerformanceMetrics average response time
TEST_F(MetricsTest, PerformanceMetricsAverageResponseTime) {
    PerformanceMetrics perf;
    
    perf.record_response(1000); // 1ms
    perf.record_response(2000); // 2ms
    perf.record_response(3000); // 3ms
    
    double avg = perf.get_average_response_time();
    EXPECT_NEAR(avg, 2.0, 0.1); // Should be approximately 2ms
}

// Test PerformanceMetrics Prometheus export
TEST_F(MetricsTest, PerformanceMetricsPrometheusExport) {
    PerformanceMetrics perf;
    
    perf.record_request();
    perf.record_response(1000);
    perf.update_active_connections(5);
    
    std::string prometheus = perf.export_prometheus();
    EXPECT_FALSE(prometheus.empty());
    EXPECT_NE(prometheus.find("simple_utcd_requests_total"), std::string::npos);
    EXPECT_NE(prometheus.find("simple_utcd_active_connections"), std::string::npos);
}

