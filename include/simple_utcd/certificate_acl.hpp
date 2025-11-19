/*
 * includes/simple_utcd/certificate_acl.hpp
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
#include <map>
#include <set>
#include <mutex>
#include <memory>
#include "tls_manager.hpp"

namespace simple_utcd {

/**
 * @brief Certificate-based ACL rule
 */
struct CertificateACLRule {
    std::string id;
    std::string common_name;
    std::string subject;
    std::string fingerprint;
    std::string issuer;
    bool allow;
    int priority;
    std::map<std::string, std::string> metadata;
    
    CertificateACLRule() : allow(true), priority(0) {}
};

/**
 * @brief Certificate-based Access Control List
 */
class CertificateACL {
public:
    CertificateACL();
    ~CertificateACL();

    // Rule management
    bool add_rule(const CertificateACLRule& rule);
    bool remove_rule(const std::string& rule_id);
    void clear_rules();
    std::vector<CertificateACLRule> get_rules() const;
    
    // Access control
    bool is_allowed(const CertificateInfo& cert_info) const;
    bool is_denied(const CertificateInfo& cert_info) const;
    
    // Matching
    bool matches_rule(const CertificateInfo& cert_info, const CertificateACLRule& rule) const;
    
    // Default action
    void set_default_action(bool allow) { default_allow_ = allow; }
    bool get_default_action() const { return default_allow_; }
    
    // Statistics
    uint64_t get_allowed_count() const { return allowed_count_; }
    uint64_t get_denied_count() const { return denied_count_; }
    void reset_statistics();

private:
    std::vector<CertificateACLRule> rules_;
    bool default_allow_;
    mutable std::mutex rules_mutex_;
    
    std::atomic<uint64_t> allowed_count_;
    std::atomic<uint64_t> denied_count_;
    
    // Matching helpers
    bool matches_common_name(const std::string& cert_cn, const std::string& rule_cn) const;
    bool matches_subject(const std::string& cert_subject, const std::string& rule_subject) const;
    bool matches_fingerprint(const std::string& cert_fingerprint, const std::string& rule_fingerprint) const;
    bool matches_issuer(const std::string& cert_issuer, const std::string& rule_issuer) const;
};

} // namespace simple_utcd

