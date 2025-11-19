/*
 * includes/simple_utcd/tls_manager.hpp
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
#include <vector>
#include <memory>
#include <mutex>
#include <cstdint>

#ifdef ENABLE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#endif

namespace simple_utcd {

/**
 * @brief TLS protocol version
 */
enum class TLSVersion {
    TLS_1_2,
    TLS_1_3,
    TLS_AUTO
};

/**
 * @brief TLS configuration
 */
struct TLSConfig {
    bool enabled;
    std::string certificate_path;
    std::string private_key_path;
    std::string ca_certificate_path;
    std::string ca_certificate_directory;
    std::vector<std::string> cipher_suites;
    std::vector<TLSVersion> protocols;
    bool verify_peer;
    bool require_client_certificate;
    bool check_certificate_revocation;
    std::string crl_path;
    uint64_t session_cache_size;
    uint64_t session_timeout;
    
    TLSConfig() 
        : enabled(false)
        , verify_peer(true)
        , require_client_certificate(false)
        , check_certificate_revocation(false)
        , session_cache_size(10000)
        , session_timeout(3600)
    {
        protocols.push_back(TLSVersion::TLS_1_2);
        protocols.push_back(TLSVersion::TLS_1_3);
    }
};

/**
 * @brief Certificate information
 */
struct CertificateInfo {
    std::string subject;
    std::string issuer;
    std::string serial_number;
    std::string fingerprint;
    std::string common_name;
    std::vector<std::string> subject_alternative_names;
    std::chrono::system_clock::time_point not_before;
    std::chrono::system_clock::time_point not_after;
    bool is_valid;
    bool is_revoked;
    
    CertificateInfo() : is_valid(false), is_revoked(false) {}
};

/**
 * @brief TLS/SSL manager for secure connections
 */
class TLSManager {
public:
    TLSManager();
    ~TLSManager();

    // Configuration
    bool configure(const TLSConfig& config);
    TLSConfig get_config() const { return config_; }
    
    // Server context
    bool create_server_context();
    bool load_certificates();
    void destroy_context();
    
    // Client context (for upstream connections)
    bool create_client_context();
    
    // Certificate validation
    bool validate_certificate(const std::string& certificate_path) const;
    bool validate_certificate_chain(const std::string& certificate_path) const;
    bool check_certificate_revocation(const std::string& certificate_path) const;
    CertificateInfo get_certificate_info(const std::string& certificate_path) const;
    
    // SSL context access (for OpenSSL integration)
#ifdef ENABLE_SSL
    SSL_CTX* get_server_context() const { return server_ctx_; }
    SSL_CTX* get_client_context() const { return client_ctx_; }
#endif
    
    // Status
    bool is_configured() const { return configured_; }
    bool is_enabled() const { return config_.enabled && configured_; }

private:
    TLSConfig config_;
    bool configured_;
    
#ifdef ENABLE_SSL
    SSL_CTX* server_ctx_;
    SSL_CTX* client_ctx_;
    
    // Certificate store for validation
    X509_STORE* cert_store_;
    
    // Certificate revocation list
    X509_CRL* crl_;
    
    // Helper methods
    bool setup_certificate_store();
    bool load_crl();
    bool set_cipher_suites(SSL_CTX* ctx);
    bool set_protocols(SSL_CTX* ctx);
    std::string get_openssl_error() const;
    CertificateInfo parse_certificate(X509* cert) const;
#endif
    
    mutable std::mutex config_mutex_;
};

/**
 * @brief TLS connection wrapper
 */
class TLSConnection {
public:
    TLSConnection();
    ~TLSConnection();

    // Connection management
    bool accept(int socket_fd, TLSManager* tls_manager);
    bool connect(int socket_fd, TLSManager* tls_manager, const std::string& hostname);
    void close();
    
    // I/O operations
    int read(void* buffer, size_t size);
    int write(const void* buffer, size_t size);
    
    // Certificate information
    CertificateInfo get_peer_certificate_info() const;
    std::string get_peer_certificate_subject() const;
    std::string get_peer_certificate_common_name() const;
    
    // Status
    bool is_connected() const { return connected_; }
    int get_socket() const { return socket_fd_; }
    
    // SSL access (for advanced operations)
#ifdef ENABLE_SSL
    SSL* get_ssl() const { return ssl_; }
#endif

private:
    int socket_fd_;
    bool connected_;
    
#ifdef ENABLE_SSL
    SSL* ssl_;
#endif
    
    CertificateInfo peer_cert_info_;
    mutable std::mutex connection_mutex_;
    
    bool load_peer_certificate();
};

} // namespace simple_utcd

