/*
 * src/core/async_io.cpp
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

#include "simple_utcd/async_io.hpp"
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <cstring>
#include <cstdlib>
#include <chrono>

namespace simple_utcd {

AsyncIOManager::AsyncIOManager(size_t thread_pool_size)
    : running_(false)
    , pending_operations_(0)
    , completed_operations_(0)
    , failed_operations_(0)
    , thread_pool_size_(thread_pool_size)
{
    worker_threads_.reserve(thread_pool_size);
}

AsyncIOManager::~AsyncIOManager() {
    stop();
}

void AsyncIOManager::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    
    // Start worker threads
    for (size_t i = 0; i < thread_pool_size_; ++i) {
        worker_threads_.emplace_back(&AsyncIOManager::worker_thread_main, this);
    }
}

void AsyncIOManager::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    queue_condition_.notify_all();
    
    // Wait for all worker threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
}

void AsyncIOManager::async_read(int fd, void* buffer, size_t size, AsyncIOCallback callback, std::chrono::milliseconds timeout) {
    if (!running_) {
        if (callback) {
            callback(AsyncIOResult::ERROR, 0);
        }
        return;
    }
    
    auto op = std::make_unique<AsyncIOOperation>(AsyncIOType::READ, fd, buffer, size, callback, false, timeout);
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        operation_queue_.push(std::move(op));
        pending_operations_++;
    }
    
    queue_condition_.notify_one();
}

void AsyncIOManager::async_write(int fd, const void* buffer, size_t size, AsyncIOCallback callback, std::chrono::milliseconds timeout) {
    if (!running_) {
        if (callback) {
            callback(AsyncIOResult::ERROR, 0);
        }
        return;
    }
    
    // For write, we need to copy the buffer
    void* buffer_copy = std::malloc(size);
    if (!buffer_copy) {
        if (callback) {
            callback(AsyncIOResult::ERROR, 0);
        }
        return;
    }
    std::memcpy(buffer_copy, buffer, size);
    
    auto op = std::make_unique<AsyncIOOperation>(AsyncIOType::WRITE, fd, buffer_copy, size, callback, true, timeout);
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        operation_queue_.push(std::move(op));
        pending_operations_++;
    }
    
    queue_condition_.notify_one();
}

void AsyncIOManager::worker_thread_main() {
    while (running_) {
        std::unique_ptr<AsyncIOOperation> op;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_condition_.wait(lock, [this] { return !operation_queue_.empty() || !running_; });
            
            if (!running_ && operation_queue_.empty()) {
                break;
            }
            
            if (!operation_queue_.empty()) {
                op = std::move(operation_queue_.front());
                operation_queue_.pop();
            }
        }
        
        if (op) {
            execute_operation(std::move(op));
        }
    }
}

void AsyncIOManager::execute_operation(std::unique_ptr<AsyncIOOperation> op) {
    auto start_time = std::chrono::steady_clock::now();
    ssize_t bytes_transferred = 0;
    AsyncIOResult result = AsyncIOResult::SUCCESS;
    
    // For now, perform synchronous I/O in thread pool (basic async implementation)
    // Full async I/O would use epoll/kqueue/io_uring for true non-blocking I/O
    // This provides async-like behavior by offloading I/O to worker threads
    if (op->type == AsyncIOType::READ) {
        bytes_transferred = perform_read(op->fd, op->buffer, op->size);
    } else {
        bytes_transferred = perform_write(op->fd, op->buffer, op->size);
    }
    
    if (bytes_transferred < 0) {
        result = AsyncIOResult::ERROR;
        failed_operations_++;
    } else {
        completed_operations_++;
    }
    
    // Check timeout
    auto elapsed = std::chrono::steady_clock::now() - start_time;
    if (elapsed > op->timeout) {
        result = AsyncIOResult::TIMEOUT;
        failed_operations_++;
    }
    
    pending_operations_--;
    
    // Call callback
    if (op->callback) {
        op->callback(result, (result == AsyncIOResult::SUCCESS) ? static_cast<size_t>(bytes_transferred) : 0);
    }
    
    // Buffer will be freed by AsyncIOOperation destructor if buffer_owned is true
}

ssize_t AsyncIOManager::perform_read(int fd, void* buffer, size_t size) {
    return ::read(fd, buffer, size);
}

ssize_t AsyncIOManager::perform_write(int fd, const void* buffer, size_t size) {
    return ::write(fd, buffer, size);
}

size_t AsyncIOManager::get_pending_operations() const {
    return pending_operations_.load();
}

size_t AsyncIOManager::get_completed_operations() const {
    return completed_operations_.load();
}

size_t AsyncIOManager::get_failed_operations() const {
    return failed_operations_.load();
}

} // namespace simple_utcd

