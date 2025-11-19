/*
 * includes/simple_utcd/utc_server.hpp
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

#include <memory>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include "utc_config.hpp"
#include "logger.hpp"
#include "metrics.hpp"
#include "health_check.hpp"
#include "async_io.hpp"

namespace simple_utcd {

class UTCConnection;
class UTCPacket;

class UTCServer {
public:
    UTCServer(UTCConfig* config, Logger* logger);
    ~UTCServer();

    bool start();
    void stop();
    bool is_running() const { return running_; }

    // Server statistics
    int get_active_connections() const { return active_connections_; }
    int get_total_connections() const { return total_connections_; }
    int get_packets_sent() const { return packets_sent_; }
    int get_packets_received() const { return packets_received_; }
    
    // Metrics and health
    class PerformanceMetrics* get_performance_metrics() const { return performance_metrics_.get(); }
    class HealthChecker* get_health_checker() const { return health_checker_.get(); }

    // Configuration access
    UTCConfig* get_config() const { return config_; }
    Logger* get_logger() const { return logger_; }
    
    // Dynamic configuration reloading
    bool reload_config(const std::string& config_file);

private:
    UTCConfig* config_;
    Logger* logger_;

    std::atomic<bool> running_;
    std::vector<std::unique_ptr<UTCConnection>> connections_;
    std::vector<std::thread> worker_threads_;
    std::mutex connections_mutex_;

    // Statistics
    std::atomic<int> active_connections_;
    std::atomic<int> total_connections_;
    std::atomic<int> packets_sent_;
    std::atomic<int> packets_received_;

    // Server socket
    int server_socket_;
    
    // Metrics and health checking
    std::unique_ptr<PerformanceMetrics> performance_metrics_;
    std::unique_ptr<HealthChecker> health_checker_;
    
    // Async I/O support
    std::unique_ptr<AsyncIOManager> async_io_manager_;

    void accept_connections();
    void handle_connection(std::unique_ptr<UTCConnection> connection);
    void worker_thread_main();
    bool create_server_socket();
    void close_server_socket();

    // UTC time handling
    uint32_t get_utc_timestamp();
    void update_reference_time();
};

} // namespace simple_utcd
