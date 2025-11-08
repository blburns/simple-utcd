/*
 * src/core/utc_server.cpp
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

#include "simple_utcd/utc_server.hpp"
#include "simple_utcd/utc_connection.hpp"
#include "simple_utcd/utc_packet.hpp"
#include "simple_utcd/platform.hpp"
#include "simple_utcd/error_handler.hpp"
#include <mutex>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#endif

namespace simple_utcd {

UTCServer::UTCServer(UTCConfig* config, Logger* logger)
    : config_(config)
    , logger_(logger)
    , running_(false)
    , active_connections_(0)
    , total_connections_(0)
    , packets_sent_(0)
    , packets_received_(0)
    , server_socket_(-1)
{
    if (logger_) {
        logger_->info("UTC Server initialized");
    }
}

UTCServer::~UTCServer() {
    stop();
}

bool UTCServer::start() {
    if (running_) {
        if (logger_) {
            logger_->warn("Server is already running");
        }
        return false;
    }

    if (!config_) {
        UTC_ERROR("UTCServer", "No configuration provided");
        return false;
    }

    // Create server socket
    if (!create_server_socket()) {
        return false;
    }

    running_ = true;

    if (logger_) {
        logger_->info("Starting UTC Server on {}:{}",
                     config_->get_listen_address(), config_->get_listen_port());
    }

    // Start worker threads
    int num_threads = config_->get_worker_threads();
    for (int i = 0; i < num_threads; ++i) {
        worker_threads_.emplace_back(&UTCServer::worker_thread_main, this);
    }

    // Start accepting connections
    std::thread accept_thread(&UTCServer::accept_connections, this);
    accept_thread.detach();

    if (logger_) {
        logger_->info("UTC Server started successfully with {} worker threads", num_threads);
    }

    return true;
}

void UTCServer::stop() {
    if (!running_) {
        return;
    }

    if (logger_) {
        logger_->info("Stopping UTC Server...");
    }

    running_ = false;

    // Close server socket
    close_server_socket();

    // Close all connections
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& connection : connections_) {
            if (connection) {
                connection->close_connection();
            }
        }
        connections_.clear();
    }

    // Wait for worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    worker_threads_.clear();

    if (logger_) {
        logger_->info("UTC Server stopped");
    }
}

void UTCServer::accept_connections() {
    while (running_) {
        std::string client_address;
        int client_fd = Platform::accept_connection(server_socket_, client_address);

        if (client_fd < 0) {
            if (running_) {
                UTC_ERROR("UTCServer", "Failed to accept connection: " + Platform::get_last_error());
            }
            continue;
        }

        // Check connection limit
        if (active_connections_ >= config_->get_max_connections()) {
            if (logger_) {
                logger_->warn("Connection limit reached, rejecting connection from {}", client_address);
            }
            Platform::close_socket(client_fd);
            continue;
        }

        // Create connection object
        auto connection = std::make_unique<UTCConnection>(client_fd, client_address, config_, logger_);

        // Add to connections list
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.push_back(std::move(connection));
        }

        active_connections_++;
        total_connections_++;

        if (logger_) {
            logger_->debug("Accepted connection from {} (active: {})",
                          client_address, active_connections_);
        }
    }
}

void UTCServer::handle_connection(std::unique_ptr<UTCConnection> connection) {
    if (!connection) {
        return;
    }

    // Send current UTC time to client
    UTCPacket packet(get_utc_timestamp());

    if (connection->send_packet(packet)) {
        packets_sent_++;

        if (logger_) {
            logger_->debug("Sent UTC time to {}: {}",
                          connection->get_client_address(), packet.to_string());
        }
    } else {
        if (logger_) {
            logger_->warn("Failed to send UTC time to {}", connection->get_client_address());
        }
    }

    // Close connection after sending (UTC protocol is typically one-shot)
    connection->close_connection();

    // Remove from connections list
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = std::find_if(connections_.begin(), connections_.end(),
                              [&connection](const std::unique_ptr<UTCConnection>& conn) {
                                  return conn.get() == connection.get();
                              });
        if (it != connections_.end()) {
            connections_.erase(it);
        }
    }

    active_connections_--;
}

void UTCServer::worker_thread_main() {
    while (running_) {
        std::unique_ptr<UTCConnection> connection;

        // Get next connection to handle
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            if (!connections_.empty()) {
                connection = std::move(connections_.front());
                connections_.erase(connections_.begin());
            }
        }

        if (connection) {
            handle_connection(std::move(connection));
        } else {
            // No connections to handle, sleep briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

bool UTCServer::create_server_socket() {
    // Create socket
    server_socket_ = Platform::create_socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        UTC_ERROR("UTCServer", "Failed to create server socket: " + Platform::get_last_error());
        return false;
    }

    // Set socket options
    int reuse = 1;
    if (!Platform::set_socket_option(server_socket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse))) {
        if (logger_) {
            logger_->warn("Failed to set SO_REUSEADDR: {}", Platform::get_last_error());
        }
    }

    // Bind socket
    if (!Platform::bind_socket(server_socket_, config_->get_listen_address(), config_->get_listen_port())) {
        UTC_ERROR("UTCServer", "Failed to bind socket: " + Platform::get_last_error());
        Platform::close_socket(server_socket_);
        server_socket_ = -1;
        return false;
    }

    // Listen for connections
    if (!Platform::listen_socket(server_socket_, config_->get_max_connections())) {
        UTC_ERROR("UTCServer", "Failed to listen on socket: " + Platform::get_last_error());
        Platform::close_socket(server_socket_);
        server_socket_ = -1;
        return false;
    }

    return true;
}

void UTCServer::close_server_socket() {
    if (server_socket_ >= 0) {
        Platform::close_socket(server_socket_);
        server_socket_ = -1;
    }
}

uint32_t UTCServer::get_utc_timestamp() {
    return UTCPacket::get_current_utc_timestamp();
}

void UTCServer::update_reference_time() {
    // Version 0.1.0: Basic implementation using system time
    // This provides functional UTC time service using the local system clock.
    // 
    // Future versions (0.2.0+) will add:
    // - Upstream NTP server synchronization
    // - Time offset calculation and drift compensation
    // - Multiple upstream server support with failover
    // - Stratum management and reference clock support
    //
    // For now, the system time is sufficient for basic UTC daemon functionality.
    // The system clock should be synchronized via the OS's time service (ntpd, systemd-timesyncd, etc.)
    
    if (logger_) {
        logger_->debug("Reference time updated (using system time)");
    }
}

} // namespace simple_utcd
