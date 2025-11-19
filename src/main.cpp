#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include "simple_utcd/utc_server.hpp"
#include "simple_utcd/utc_config.hpp"
#include "simple_utcd/logger.hpp"
#include "simple_utcd/error_handler.hpp"

// Global variables for signal handling
static std::atomic<simple_utcd::UTCServer*> g_server_ptr{nullptr};
static std::atomic<simple_utcd::UTCConfig*> g_config_ptr{nullptr};
static std::atomic<std::string*> g_config_file_ptr{nullptr};
static std::atomic<bool> g_reload_requested{false};
static std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int sig) {
    if (sig == SIGHUP) {
        // Request configuration reload
        g_reload_requested = true;
    } else if (sig == SIGINT || sig == SIGTERM) {
        // Request graceful shutdown
        g_shutdown_requested = true;
        auto* server = g_server_ptr.load();
        if (server) {
            server->stop();
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        // Initialize error handler
        simple_utcd::ErrorHandlerManager::initialize_default();

        // Initialize logger
        auto logger = std::make_unique<simple_utcd::Logger>();
        logger->info("Simple UTC Daemon starting...");

        // Determine config file path
        std::string config_file = "config/simple-utcd.conf";
        if (argc > 1) {
            config_file = argv[1];
        } else {
            // Check environment variable
            const char* env_config = std::getenv("SIMPLE_UTCD_CONFIG");
            if (env_config) {
                config_file = env_config;
            }
        }
        
        // Load configuration
        auto config = std::make_unique<simple_utcd::UTCConfig>();
        if (!config->load(config_file)) {
            logger->error("Failed to load configuration file: {}", config_file);
            return 1;
        }
        
        // Load environment variables (override config file values)
        config->load_from_environment();
        
        // Validate configuration
        if (!config->validate()) {
            logger->error("Configuration validation failed:");
            for (const auto& error : config->get_validation_errors()) {
                logger->error("  - {}", error);
            }
            return 1;
        }

        // Create and start UTC server
        auto server = std::make_unique<simple_utcd::UTCServer>(config.get(), logger.get());
        
        // Set up signal handlers
        g_server_ptr = server.get();
        g_config_ptr = config.get();
        g_config_file_ptr = &config_file;
        
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGHUP, signal_handler);

        logger->info("UTC Daemon initialized successfully");
        logger->info("Listening on {}:{}", config->get_listen_address(), config->get_listen_port());
        logger->info("Send SIGHUP to reload configuration");

        // Start the server
        if (!server->start()) {
            logger->error("Failed to start UTC server");
            return 1;
        }

        // Keep the server running
        logger->info("UTC Daemon is running. Press Ctrl+C to stop.");

        // Main loop with config reload support
        while (server->is_running() && !g_shutdown_requested.load()) {
            // Check for reload request
            if (g_reload_requested.load()) {
                g_reload_requested = false;
                logger->info("Received SIGHUP, reloading configuration...");
                if (server->reload_config(config_file)) {
                    logger->info("Configuration reloaded successfully");
                } else {
                    logger->error("Configuration reload failed, using previous configuration");
                }
            }
            
            // Check for config file changes (if file watching is enabled)
            if (config->is_file_watching_enabled() && config->check_config_file_changed()) {
                logger->info("Configuration file changed, reloading...");
                if (server->reload_config(config_file)) {
                    logger->info("Configuration reloaded from file change");
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        logger->info("UTC Daemon shutting down...");
        server->stop();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
