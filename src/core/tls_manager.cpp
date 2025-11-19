/*
 * src/core/tls_manager.cpp
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

#include "simple_utcd/tls_manager.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <cstring>
#include <unistd.h>

#ifdef ENABLE_SSL
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>
#endif

namespace simple_utcd {

TLSManager::TLSManager()
    : configured_(false)
#ifdef ENABLE_SSL
    , server_ctx_(nullptr)
    , client_ctx_(nullptr)
    , cert_store_(nullptr)
    , crl_(nullptr)
#endif
{
#ifdef ENABLE_SSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
#endif
}

TLSManager::~TLSManager() {
    destroy_context();
#ifdef ENABLE_SSL
    EVP_cleanup();
#endif
}

bool TLSManager::configure(const TLSConfig& config) {
    std::lock_guard<std::mutex> lock(config_mutex_);
    
    config_ = config;
    
    if (!config_.enabled) {
        configured_ = true;
        return true;
    }
    
    // Validate configuration
    if (config_.certificate_path.empty() || config_.private_key_path.empty()) {
        return false;
    }
    
    configured_ = true;
    return true;
}

bool TLSManager::create_server_context() {
#ifdef ENABLE_SSL
    if (!configured_ || !config_.enabled) {
        return false;
    }
    
    const SSL_METHOD* method = TLS_server_method();
    if (!method) {
        return false;
    }
    
    server_ctx_ = SSL_CTX_new(method);
    if (!server_ctx_) {
        return false;
    }
    
    // Set minimum protocol version
    if (!set_protocols(server_ctx_)) {
        SSL_CTX_free(server_ctx_);
        server_ctx_ = nullptr;
        return false;
    }
    
    // Set cipher suites
    if (!set_cipher_suites(server_ctx_)) {
        SSL_CTX_free(server_ctx_);
        server_ctx_ = nullptr;
        return false;
    }
    
    // Load certificates
    if (!load_certificates()) {
        SSL_CTX_free(server_ctx_);
        server_ctx_ = nullptr;
        return false;
    }
    
    // Setup certificate store for validation
    if (!setup_certificate_store()) {
        SSL_CTX_free(server_ctx_);
        server_ctx_ = nullptr;
        return false;
    }
    
    // Set verification mode
    int verify_mode = SSL_VERIFY_NONE;
    if (config_.verify_peer) {
        verify_mode = SSL_VERIFY_PEER;
        if (config_.require_client_certificate) {
            verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
        }
    }
    SSL_CTX_set_verify(server_ctx_, verify_mode, nullptr);
    
    // Set session cache
    SSL_CTX_set_session_cache_mode(server_ctx_, SSL_SESS_CACHE_SERVER);
    SSL_CTX_sess_set_cache_size(server_ctx_, config_.session_cache_size);
    SSL_CTX_set_timeout(server_ctx_, config_.session_timeout);
    
    return true;
#else
    return false;
#endif
}

bool TLSManager::create_client_context() {
#ifdef ENABLE_SSL
    if (!configured_ || !config_.enabled) {
        return false;
    }
    
    const SSL_METHOD* method = TLS_client_method();
    if (!method) {
        return false;
    }
    
    client_ctx_ = SSL_CTX_new(method);
    if (!client_ctx_) {
        return false;
    }
    
    // Set minimum protocol version
    if (!set_protocols(client_ctx_)) {
        SSL_CTX_free(client_ctx_);
        client_ctx_ = nullptr;
        return false;
    }
    
    // Set cipher suites
    if (!set_cipher_suites(client_ctx_)) {
        SSL_CTX_free(client_ctx_);
        client_ctx_ = nullptr;
        return false;
    }
    
    // Setup certificate store for validation
    if (!setup_certificate_store()) {
        SSL_CTX_free(client_ctx_);
        client_ctx_ = nullptr;
        return false;
    }
    
    // Set verification mode
    int verify_mode = SSL_VERIFY_PEER;
    if (!config_.verify_peer) {
        verify_mode = SSL_VERIFY_NONE;
    }
    SSL_CTX_set_verify(client_ctx_, verify_mode, nullptr);
    
    return true;
#else
    return false;
#endif
}

bool TLSManager::load_certificates() {
#ifdef ENABLE_SSL
    if (!server_ctx_) {
        return false;
    }
    
    // Load server certificate
    if (SSL_CTX_use_certificate_file(server_ctx_, config_.certificate_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        return false;
    }
    
    // Load private key
    if (SSL_CTX_use_PrivateKey_file(server_ctx_, config_.private_key_path.c_str(), SSL_FILETYPE_PEM) <= 0) {
        return false;
    }
    
    // Verify certificate and key match
    if (!SSL_CTX_check_private_key(server_ctx_)) {
        return false;
    }
    
    return true;
#else
    return false;
#endif
}

void TLSManager::destroy_context() {
#ifdef ENABLE_SSL
    if (server_ctx_) {
        SSL_CTX_free(server_ctx_);
        server_ctx_ = nullptr;
    }
    
    if (client_ctx_) {
        SSL_CTX_free(client_ctx_);
        client_ctx_ = nullptr;
    }
    
    if (cert_store_) {
        X509_STORE_free(cert_store_);
        cert_store_ = nullptr;
    }
    
    if (crl_) {
        X509_CRL_free(crl_);
        crl_ = nullptr;
    }
#endif
}

bool TLSManager::validate_certificate(const std::string& certificate_path) const {
#ifdef ENABLE_SSL
    std::ifstream file(certificate_path);
    if (!file.is_open()) {
        return false;
    }
    
    X509* cert = PEM_read_X509_AUX(file, nullptr, nullptr, nullptr);
    file.close();
    
    if (!cert) {
        return false;
    }
    
    // Check validity period
    time_t now = time(nullptr);
    if (X509_cmp_time(X509_get_notBefore(cert), &now) > 0 ||
        X509_cmp_time(X509_get_notAfter(cert), &now) < 0) {
        X509_free(cert);
        return false;
    }
    
    X509_free(cert);
    return true;
#else
    return false;
#endif
}

bool TLSManager::validate_certificate_chain(const std::string& certificate_path) const {
#ifdef ENABLE_SSL
    if (!cert_store_) {
        return false;
    }
    
    std::ifstream file(certificate_path);
    if (!file.is_open()) {
        return false;
    }
    
    X509* cert = PEM_read_X509_AUX(file, nullptr, nullptr, nullptr);
    file.close();
    
    if (!cert) {
        return false;
    }
    
    X509_STORE_CTX* ctx = X509_STORE_CTX_new();
    if (!ctx) {
        X509_free(cert);
        return false;
    }
    
    X509_STORE_CTX_init(ctx, cert_store_, cert, nullptr);
    int result = X509_verify_cert(ctx);
    
    X509_STORE_CTX_free(ctx);
    X509_free(cert);
    
    return result == 1;
#else
    return false;
#endif
}

bool TLSManager::check_certificate_revocation(const std::string& certificate_path) const {
#ifdef ENABLE_SSL
    if (!crl_ || config_.crl_path.empty()) {
        return true;  // No CRL configured, assume not revoked
    }
    
    std::ifstream file(certificate_path);
    if (!file.is_open()) {
        return false;
    }
    
    X509* cert = PEM_read_X509_AUX(file, nullptr, nullptr, nullptr);
    file.close();
    
    if (!cert) {
        return false;
    }
    
    X509_REVOKED* revoked = nullptr;
    int result = X509_CRL_get0_by_cert(crl_, &revoked, cert);
    
    X509_free(cert);
    
    return result == 0;  // 0 means not found (not revoked)
#else
    return true;
#endif
}

CertificateInfo TLSManager::get_certificate_info(const std::string& certificate_path) const {
    CertificateInfo info;
    
#ifdef ENABLE_SSL
    std::ifstream file(certificate_path);
    if (!file.is_open()) {
        return info;
    }
    
    X509* cert = PEM_read_X509_AUX(file, nullptr, nullptr, nullptr);
    file.close();
    
    if (!cert) {
        return info;
    }
    
    info = parse_certificate(cert);
    X509_free(cert);
#endif
    
    return info;
}

#ifdef ENABLE_SSL
bool TLSManager::setup_certificate_store() {
    cert_store_ = X509_STORE_new();
    if (!cert_store_) {
        return false;
    }
    
    // Load CA certificate
    if (!config_.ca_certificate_path.empty()) {
        X509_LOOKUP* lookup = X509_STORE_add_lookup(cert_store_, X509_LOOKUP_file());
        if (lookup) {
            X509_LOOKUP_load_file(lookup, config_.ca_certificate_path.c_str(), X509_FILETYPE_PEM);
        }
    }
    
    // Load CA certificate directory
    if (!config_.ca_certificate_directory.empty()) {
        X509_LOOKUP* lookup = X509_STORE_add_lookup(cert_store_, X509_LOOKUP_hash_dir());
        if (lookup) {
            X509_LOOKUP_add_dir(lookup, config_.ca_certificate_directory.c_str(), X509_FILETYPE_PEM);
        }
    }
    
    // Load CRL if configured
    if (config_.check_certificate_revocation) {
        load_crl();
    }
    
    return true;
}

bool TLSManager::load_crl() {
    if (config_.crl_path.empty()) {
        return true;  // No CRL configured
    }
    
    std::ifstream file(config_.crl_path);
    if (!file.is_open()) {
        return false;
    }
    
    crl_ = PEM_read_X509_CRL(file, nullptr, nullptr, nullptr);
    file.close();
    
    if (!crl_) {
        return false;
    }
    
    X509_STORE_add_crl(cert_store_, crl_);
    X509_STORE_set_flags(cert_store_, X509_V_FLAG_CRL_CHECK);
    
    return true;
}

bool TLSManager::set_cipher_suites(SSL_CTX* ctx) {
    if (config_.cipher_suites.empty()) {
        // Use default secure ciphers
        return SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5") == 1;
    }
    
    std::string cipher_list;
    for (size_t i = 0; i < config_.cipher_suites.size(); ++i) {
        if (i > 0) {
            cipher_list += ":";
        }
        cipher_list += config_.cipher_suites[i];
    }
    
    return SSL_CTX_set_cipher_list(ctx, cipher_list.c_str()) == 1;
}

bool TLSManager::set_protocols(SSL_CTX* ctx) {
    long options = 0;
    bool has_tls12 = false;
    bool has_tls13 = false;
    
    for (const auto& version : config_.protocols) {
        switch (version) {
            case TLSVersion::TLS_1_2:
                has_tls12 = true;
                break;
            case TLSVersion::TLS_1_3:
                has_tls13 = true;
                break;
            case TLSVersion::TLS_AUTO:
                has_tls12 = true;
                has_tls13 = true;
                break;
        }
    }
    
    // Disable older protocols
    options |= SSL_OP_NO_SSLv2;
    options |= SSL_OP_NO_SSLv3;
    options |= SSL_OP_NO_TLSv1;
    options |= SSL_OP_NO_TLSv1_1;
    
    if (!has_tls12) {
        options |= SSL_OP_NO_TLSv1_2;
    }
    if (!has_tls13) {
        options |= SSL_OP_NO_TLSv1_3;
    }
    
    SSL_CTX_set_options(ctx, options);
    
    // Set minimum version
    if (has_tls12) {
        SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    } else if (has_tls13) {
        SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
    }
    
    return true;
}

std::string TLSManager::get_openssl_error() const {
    char error_buf[256];
    unsigned long error = ERR_get_error();
    ERR_error_string_n(error, error_buf, sizeof(error_buf));
    return std::string(error_buf);
}

CertificateInfo TLSManager::parse_certificate(X509* cert) const {
    CertificateInfo info;
    
    if (!cert) {
        return info;
    }
    
    // Subject
    char* subject = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
    if (subject) {
        info.subject = subject;
        OPENSSL_free(subject);
    }
    
    // Issuer
    char* issuer = X509_NAME_oneline(X509_get_issuer_name(cert), nullptr, 0);
    if (issuer) {
        info.issuer = issuer;
        OPENSSL_free(issuer);
    }
    
    // Serial number
    ASN1_INTEGER* serial = X509_get_serialNumber(cert);
    if (serial) {
        BIGNUM* bn = ASN1_INTEGER_to_BN(serial, nullptr);
        if (bn) {
            char* serial_str = BN_bn2hex(bn);
            if (serial_str) {
                info.serial_number = serial_str;
                OPENSSL_free(serial_str);
            }
            BN_free(bn);
        }
    }
    
    // Fingerprint
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len;
    if (X509_digest(cert, EVP_sha256(), md, &md_len)) {
        std::ostringstream oss;
        for (unsigned int i = 0; i < md_len; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md[i]);
        }
        info.fingerprint = oss.str();
    }
    
    // Common name
    X509_NAME* name = X509_get_subject_name(cert);
    int pos = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
    if (pos >= 0) {
        X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, pos);
        ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
        if (data) {
            unsigned char* utf8;
            int len = ASN1_STRING_to_UTF8(&utf8, data);
            if (len > 0) {
                info.common_name = std::string(reinterpret_cast<char*>(utf8), len);
                OPENSSL_free(utf8);
            }
        }
    }
    
    // Validity period
    const ASN1_TIME* not_before = X509_get_notBefore(cert);
    const ASN1_TIME* not_after = X509_get_notAfter(cert);
    
    struct tm tm_before = {};
    struct tm tm_after = {};
    ASN1_TIME_to_tm(not_before, &tm_before);
    ASN1_TIME_to_tm(not_after, &tm_after);
    
    info.not_before = std::chrono::system_clock::from_time_t(mktime(&tm_before));
    info.not_after = std::chrono::system_clock::from_time_t(mktime(&tm_after));
    
    // Check validity
    time_t now = time(nullptr);
    info.is_valid = (X509_cmp_time(not_before, &now) <= 0 && X509_cmp_time(not_after, &now) >= 0);
    
    return info;
}
#endif

// TLSConnection implementation
TLSConnection::TLSConnection()
    : socket_fd_(-1)
    , connected_(false)
#ifdef ENABLE_SSL
    , ssl_(nullptr)
#endif
{
}

TLSConnection::~TLSConnection() {
    close();
}

bool TLSConnection::accept(int socket_fd, TLSManager* tls_manager) {
#ifdef ENABLE_SSL
    if (!tls_manager || !tls_manager->is_enabled()) {
        return false;
    }
    
    SSL_CTX* ctx = tls_manager->get_server_context();
    if (!ctx) {
        return false;
    }
    
    ssl_ = SSL_new(ctx);
    if (!ssl_) {
        return false;
    }
    
    SSL_set_fd(ssl_, socket_fd);
    
    int result = SSL_accept(ssl_);
    if (result <= 0) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        return false;
    }
    
    socket_fd_ = socket_fd;
    connected_ = true;
    
    load_peer_certificate();
    
    return true;
#else
    return false;
#endif
}

bool TLSConnection::connect(int socket_fd, TLSManager* tls_manager, const std::string& hostname) {
#ifdef ENABLE_SSL
    if (!tls_manager || !tls_manager->is_enabled()) {
        return false;
    }
    
    SSL_CTX* ctx = tls_manager->get_client_context();
    if (!ctx) {
        return false;
    }
    
    ssl_ = SSL_new(ctx);
    if (!ssl_) {
        return false;
    }
    
    SSL_set_fd(ssl_, socket_fd);
    
    if (!hostname.empty()) {
        SSL_set_tlsext_host_name(ssl_, hostname.c_str());
    }
    
    int result = SSL_connect(ssl_);
    if (result <= 0) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        return false;
    }
    
    socket_fd_ = socket_fd;
    connected_ = true;
    
    load_peer_certificate();
    
    return true;
#else
    return false;
#endif
}

void TLSConnection::close() {
#ifdef ENABLE_SSL
    if (ssl_) {
        SSL_shutdown(ssl_);
        SSL_free(ssl_);
        ssl_ = nullptr;
    }
#endif
    
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
    
    connected_ = false;
}

int TLSConnection::read(void* buffer, size_t size) {
#ifdef ENABLE_SSL
    if (!connected_ || !ssl_) {
        return -1;
    }
    
    return SSL_read(ssl_, buffer, size);
#else
    return -1;
#endif
}

int TLSConnection::write(const void* buffer, size_t size) {
#ifdef ENABLE_SSL
    if (!connected_ || !ssl_) {
        return -1;
    }
    
    return SSL_write(ssl_, buffer, size);
#else
    return -1;
#endif
}

CertificateInfo TLSConnection::get_peer_certificate_info() const {
    return peer_cert_info_;
}

std::string TLSConnection::get_peer_certificate_subject() const {
    return peer_cert_info_.subject;
}

std::string TLSConnection::get_peer_certificate_common_name() const {
    return peer_cert_info_.common_name;
}

bool TLSConnection::load_peer_certificate() {
#ifdef ENABLE_SSL
    if (!ssl_) {
        return false;
    }
    
    X509* cert = SSL_get_peer_certificate(ssl_);
    if (!cert) {
        return false;
    }
    
    // Parse certificate using TLSManager's method
    // For now, we'll do basic parsing here
    char* subject = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
    if (subject) {
        peer_cert_info_.subject = subject;
        OPENSSL_free(subject);
    }
    
    X509_NAME* name = X509_get_subject_name(cert);
    int pos = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
    if (pos >= 0) {
        X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, pos);
        ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
        if (data) {
            unsigned char* utf8;
            int len = ASN1_STRING_to_UTF8(&utf8, data);
            if (len > 0) {
                peer_cert_info_.common_name = std::string(reinterpret_cast<char*>(utf8), len);
                OPENSSL_free(utf8);
            }
        }
    }
    
    X509_free(cert);
    return true;
#else
    return false;
#endif
}

} // namespace simple_utcd

