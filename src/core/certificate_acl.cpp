/*
 * src/core/certificate_acl.cpp
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

#include "simple_utcd/certificate_acl.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>

namespace simple_utcd {

CertificateACL::CertificateACL()
    : default_allow_(true)
    , allowed_count_(0)
    , denied_count_(0)
{
}

CertificateACL::~CertificateACL() {
}

bool CertificateACL::add_rule(const CertificateACLRule& rule) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    // Check if rule ID already exists
    auto it = std::find_if(rules_.begin(), rules_.end(),
        [&rule](const CertificateACLRule& r) {
            return r.id == rule.id;
        });
    
    if (it != rules_.end()) {
        return false;  // Rule ID already exists
    }
    
    rules_.push_back(rule);
    
    // Sort by priority (higher priority first)
    std::sort(rules_.begin(), rules_.end(),
        [](const CertificateACLRule& a, const CertificateACLRule& b) {
            return a.priority > b.priority;
        });
    
    return true;
}

bool CertificateACL::remove_rule(const std::string& rule_id) {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    auto it = std::find_if(rules_.begin(), rules_.end(),
        [&rule_id](const CertificateACLRule& r) {
            return r.id == rule_id;
        });
    
    if (it != rules_.end()) {
        rules_.erase(it);
        return true;
    }
    
    return false;
}

void CertificateACL::clear_rules() {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    rules_.clear();
}

std::vector<CertificateACLRule> CertificateACL::get_rules() const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    return rules_;
}

bool CertificateACL::is_allowed(const CertificateInfo& cert_info) const {
    std::lock_guard<std::mutex> lock(rules_mutex_);
    
    // Check rules in priority order
    for (const auto& rule : rules_) {
        if (matches_rule(cert_info, rule)) {
            if (rule.allow) {
                const_cast<CertificateACL*>(this)->allowed_count_++;
                return true;
            } else {
                const_cast<CertificateACL*>(this)->denied_count_++;
                return false;
            }
        }
    }
    
    // No matching rule, use default action
    if (default_allow_) {
        const_cast<CertificateACL*>(this)->allowed_count_++;
        return true;
    } else {
        const_cast<CertificateACL*>(this)->denied_count_++;
        return false;
    }
}

bool CertificateACL::is_denied(const CertificateInfo& cert_info) const {
    return !is_allowed(cert_info);
}

bool CertificateACL::matches_rule(const CertificateInfo& cert_info, const CertificateACLRule& rule) const {
    // If all fields are empty, rule matches nothing
    if (rule.common_name.empty() && rule.subject.empty() && 
        rule.fingerprint.empty() && rule.issuer.empty()) {
        return false;
    }
    
    bool matches = true;
    
    // Check common name
    if (!rule.common_name.empty()) {
        if (!matches_common_name(cert_info.common_name, rule.common_name)) {
            matches = false;
        }
    }
    
    // Check subject
    if (!rule.subject.empty() && matches) {
        if (!matches_subject(cert_info.subject, rule.subject)) {
            matches = false;
        }
    }
    
    // Check fingerprint
    if (!rule.fingerprint.empty() && matches) {
        if (!matches_fingerprint(cert_info.fingerprint, rule.fingerprint)) {
            matches = false;
        }
    }
    
    // Check issuer
    if (!rule.issuer.empty() && matches) {
        if (!matches_issuer(cert_info.issuer, rule.issuer)) {
            matches = false;
        }
    }
    
    return matches;
}

bool CertificateACL::matches_common_name(const std::string& cert_cn, const std::string& rule_cn) const {
    if (cert_cn.empty() || rule_cn.empty()) {
        return false;
    }
    
    // Exact match
    if (cert_cn == rule_cn) {
        return true;
    }
    
    // Wildcard matching (simple implementation)
    if (rule_cn.find('*') != std::string::npos) {
        // Convert wildcard pattern to regex-like matching
        size_t pos = 0;
        size_t cert_pos = 0;
        
        while (pos < rule_cn.length() && cert_pos < cert_cn.length()) {
            if (rule_cn[pos] == '*') {
                // Match zero or more characters
                if (pos + 1 >= rule_cn.length()) {
                    return true;  // * at end matches rest
                }
                
                // Find next character after *
                char next_char = rule_cn[pos + 1];
                while (cert_pos < cert_cn.length() && cert_cn[cert_pos] != next_char) {
                    cert_pos++;
                }
                pos++;
            } else if (rule_cn[pos] == cert_cn[cert_pos]) {
                pos++;
                cert_pos++;
            } else {
                return false;
            }
        }
        
        return (pos == rule_cn.length() && cert_pos == cert_cn.length());
    }
    
    return false;
}

bool CertificateACL::matches_subject(const std::string& cert_subject, const std::string& rule_subject) const {
    if (cert_subject.empty() || rule_subject.empty()) {
        return false;
    }
    
    // Exact match
    if (cert_subject == rule_subject) {
        return true;
    }
    
    // Contains match (for partial matching)
    if (cert_subject.find(rule_subject) != std::string::npos) {
        return true;
    }
    
    return false;
}

bool CertificateACL::matches_fingerprint(const std::string& cert_fingerprint, const std::string& rule_fingerprint) const {
    if (cert_fingerprint.empty() || rule_fingerprint.empty()) {
        return false;
    }
    
    // Convert to lowercase for comparison
    std::string cert_lower = cert_fingerprint;
    std::string rule_lower = rule_fingerprint;
    
    std::transform(cert_lower.begin(), cert_lower.end(), cert_lower.begin(), ::tolower);
    std::transform(rule_lower.begin(), rule_lower.end(), rule_lower.begin(), ::tolower);
    
    return cert_lower == rule_lower;
}

bool CertificateACL::matches_issuer(const std::string& cert_issuer, const std::string& rule_issuer) const {
    if (cert_issuer.empty() || rule_issuer.empty()) {
        return false;
    }
    
    // Exact match
    if (cert_issuer == rule_issuer) {
        return true;
    }
    
    // Contains match (for partial matching)
    if (cert_issuer.find(rule_issuer) != std::string::npos) {
        return true;
    }
    
    return false;
}

void CertificateACL::reset_statistics() {
    allowed_count_ = 0;
    denied_count_ = 0;
}

} // namespace simple_utcd

