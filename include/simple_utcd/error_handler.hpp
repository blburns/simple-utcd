/*
 * includes/simple_utcd/error_handler.hpp
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
#include <exception>
#include <memory>
#include <vector>

namespace simple_utcd {

/**
 * @brief UTC Daemon specific exception types
 */
class UTCException : public std::exception {
public:
    explicit UTCException(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};

class ConfigurationError : public UTCException {
public:
    explicit ConfigurationError(const std::string& message) : UTCException("Configuration Error: " + message) {}
};

class NetworkError : public UTCException {
public:
    explicit NetworkError(const std::string& message) : UTCException("Network Error: " + message) {}
};

class PacketError : public UTCException {
public:
    explicit PacketError(const std::string& message) : UTCException("Packet Error: " + message) {}
};

class SystemError : public UTCException {
public:
    explicit SystemError(const std::string& message) : UTCException("System Error: " + message) {}
};

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    INFO,       // Informational
    WARNING,    // Warning - non-fatal
    ERROR,      // Error - may be recoverable
    CRITICAL    // Critical - fatal error
};

/**
 * @brief Error context information
 */
struct ErrorContext {
    std::string component;      // Component where error occurred
    std::string function;       // Function where error occurred
    std::string file;          // Source file
    int line;                  // Line number
    std::string description;   // Error description
    ErrorSeverity severity;    // Error severity
    std::string timestamp;     // When error occurred

    ErrorContext(const std::string& comp, const std::string& func,
                 const std::string& f, int l, const std::string& desc,
                 ErrorSeverity sev);
};

/**
 * @brief Error handler interface
 */
class ErrorHandler {
public:
    virtual ~ErrorHandler() = default;

    /**
     * @brief Handle an error
     * @param context Error context information
     * @param exception Optional exception pointer
     * @return true if error was handled/recovered, false otherwise
     */
    virtual bool handle_error(const ErrorContext& context,
                             const std::exception* exception = nullptr) = 0;
    
    /**
     * @brief Attempt to recover from an error
     * @param context Error context information
     * @return true if recovery was successful
     */
    virtual bool attempt_recovery(const ErrorContext& context) = 0;

    /**
     * @brief Check if error should be logged
     * @param severity Error severity
     * @return true if should be logged
     */
    virtual bool should_log(ErrorSeverity severity) const = 0;

    /**
     * @brief Get error statistics
     * @return Map of error counts by severity
     */
    virtual std::vector<std::pair<ErrorSeverity, size_t>> get_error_stats() const = 0;

    /**
     * @brief Reset error statistics
     */
    virtual void reset_stats() = 0;
};

/**
 * @brief Default error handler implementation
 */
class DefaultErrorHandler : public ErrorHandler {
public:
    explicit DefaultErrorHandler(bool enable_logging = true,
                                ErrorSeverity min_log_level = ErrorSeverity::WARNING);

    bool handle_error(const ErrorContext& context,
                     const std::exception* exception = nullptr) override;
    
    bool attempt_recovery(const ErrorContext& context) override;

    bool should_log(ErrorSeverity severity) const override;

    std::vector<std::pair<ErrorSeverity, size_t>> get_error_stats() const override;

    void reset_stats() override;

    /**
     * @brief Set minimum logging level
     * @param level Minimum severity level to log
     */
    void set_min_log_level(ErrorSeverity level);

    /**
     * @brief Enable or disable logging
     * @param enable true to enable logging
     */
    void set_logging_enabled(bool enable);

private:
    bool logging_enabled_;
    ErrorSeverity min_log_level_;
    std::vector<size_t> error_counts_;

    void log_error(const ErrorContext& context, const std::exception* exception);
    std::string severity_to_string(ErrorSeverity severity) const;
};

/**
 * @brief Global error handler instance
 */
class ErrorHandlerManager {
public:
    /**
     * @brief Set the global error handler
     * @param handler Error handler instance
     */
    static void set_handler(std::unique_ptr<ErrorHandler> handler);

    /**
     * @brief Get the global error handler
     * @return Reference to error handler
     */
    static ErrorHandler& get_handler();

    /**
     * @brief Initialize with default error handler
     */
    static void initialize_default();

private:
    static std::unique_ptr<ErrorHandler> handler_;
};

/**
 * @brief Convenience macros for error handling
 */
#define UTC_ERROR(component, description) \
    simple_utcd::ErrorHandlerManager::get_handler().handle_error( \
        simple_utcd::ErrorContext(component, __func__, __FILE__, __LINE__, \
                                 description, simple_utcd::ErrorSeverity::ERROR))

#define UTC_WARNING(component, description) \
    simple_utcd::ErrorHandlerManager::get_handler().handle_error( \
        simple_utcd::ErrorContext(component, __func__, __FILE__, __LINE__, \
                                 description, simple_utcd::ErrorSeverity::WARNING))

#define UTC_CRITICAL(component, description) \
    simple_utcd::ErrorHandlerManager::get_handler().handle_error( \
        simple_utcd::ErrorContext(component, __func__, __FILE__, __LINE__, \
                                 description, simple_utcd::ErrorSeverity::CRITICAL))

#define UTC_INFO(component, description) \
    simple_utcd::ErrorHandlerManager::get_handler().handle_error( \
        simple_utcd::ErrorContext(component, __func__, __FILE__, __LINE__, \
                                 description, simple_utcd::ErrorSeverity::INFO))

#define UTC_THROW_ERROR(component, description) \
    do { \
        UTC_ERROR(component, description); \
        throw simple_utcd::UTCException(description); \
    } while(0)

#define UTC_THROW_NETWORK_ERROR(component, description) \
    do { \
        UTC_ERROR(component, description); \
        throw simple_utcd::NetworkError(description); \
    } while(0)

#define UTC_THROW_CONFIG_ERROR(component, description) \
    do { \
        UTC_ERROR(component, description); \
        throw simple_utcd::ConfigurationError(description); \
    } while(0)

#define UTC_THROW_PACKET_ERROR(component, description) \
    do { \
        UTC_ERROR(component, description); \
        throw simple_utcd::PacketError(description); \
    } while(0)

#define UTC_THROW_SYSTEM_ERROR(component, description) \
    do { \
        UTC_ERROR(component, description); \
        throw simple_utcd::SystemError(description); \
    } while(0)

} // namespace simple_utcd
