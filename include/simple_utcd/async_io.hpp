/*
 * includes/simple_utcd/async_io.hpp
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

#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace simple_utcd {

/**
 * @brief Async I/O operation result
 */
enum class AsyncIOResult {
    SUCCESS,
    ERROR,
    TIMEOUT,
    CANCELLED
};

/**
 * @brief Async I/O operation callback
 */
using AsyncIOCallback = std::function<void(AsyncIOResult result, size_t bytes_transferred)>;

/**
 * @brief Async I/O operation type
 */
enum class AsyncIOType {
    READ,
    WRITE
};

/**
 * @brief Async I/O operation
 */
struct AsyncIOOperation {
    AsyncIOType type;
    int fd;
    void* buffer;
    size_t size;
    AsyncIOCallback callback;
    std::chrono::milliseconds timeout;
    bool buffer_owned; // Whether buffer should be freed
    
    AsyncIOOperation(AsyncIOType t, int f, void* buf, size_t sz, AsyncIOCallback cb, bool owned, std::chrono::milliseconds to = std::chrono::milliseconds(5000))
        : type(t), fd(f), buffer(buf), size(sz), callback(cb), timeout(to), buffer_owned(owned) {}
    
    ~AsyncIOOperation() {
        if (buffer_owned && buffer) {
            std::free(buffer);
        }
    }
};

/**
 * @brief Async I/O manager for non-blocking operations
 */
class AsyncIOManager {
public:
    AsyncIOManager(size_t thread_pool_size = 4);
    ~AsyncIOManager();

    // Async read operation
    void async_read(int fd, void* buffer, size_t size, AsyncIOCallback callback, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // Async write operation
    void async_write(int fd, const void* buffer, size_t size, AsyncIOCallback callback, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    // Start/stop the async I/O manager
    void start();
    void stop();
    bool is_running() const { return running_; }
    
    // Get statistics
    size_t get_pending_operations() const;
    size_t get_completed_operations() const;
    size_t get_failed_operations() const;

private:
    std::atomic<bool> running_;
    std::vector<std::thread> worker_threads_;
    std::queue<std::unique_ptr<AsyncIOOperation>> operation_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_condition_;
    
    std::atomic<size_t> pending_operations_;
    std::atomic<size_t> completed_operations_;
    std::atomic<size_t> failed_operations_;
    size_t thread_pool_size_;
    
    void worker_thread_main();
    void execute_operation(std::unique_ptr<AsyncIOOperation> op);
    ssize_t perform_read(int fd, void* buffer, size_t size);
    ssize_t perform_write(int fd, const void* buffer, size_t size);
};

} // namespace simple_utcd

