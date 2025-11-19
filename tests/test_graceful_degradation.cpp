/*
 * tests/test_graceful_degradation.cpp
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

#include <gtest/gtest.h>
#include "simple_utcd/graceful_degradation.hpp"

using namespace simple_utcd;

class GracefulDegradationTest : public ::testing::Test {
protected:
    void SetUp() override {
        degradation_.set_resource_thresholds(1024, 80.0, 1000);
        degradation_.set_health_threshold(0.5);
    }

    void TearDown() override {
        // Clean up if needed
    }

    GracefulDegradation degradation_;
};

// Test default constructor
TEST_F(GracefulDegradationTest, DefaultConstructor) {
    GracefulDegradation deg;
    EXPECT_FALSE(deg.is_degraded());
    EXPECT_EQ(deg.get_degradation_level(), DegradationLevel::NORMAL);
}

// Test degradation level setting
TEST_F(GracefulDegradationTest, DegradationLevel) {
    degradation_.set_degradation_level(DegradationLevel::DEGRADED);
    EXPECT_TRUE(degradation_.is_degraded());
    EXPECT_EQ(degradation_.get_degradation_level(), DegradationLevel::DEGRADED);
    
    degradation_.set_degradation_level(DegradationLevel::NORMAL);
    EXPECT_FALSE(degradation_.is_degraded());
}

// Test feature registration
TEST_F(GracefulDegradationTest, FeatureRegistration) {
    EXPECT_TRUE(degradation_.register_feature("feature1", ServicePriority::CRITICAL));
    EXPECT_TRUE(degradation_.register_feature("feature2", ServicePriority::NORMAL));
    EXPECT_TRUE(degradation_.register_feature("feature3", ServicePriority::LOW));
    
    EXPECT_TRUE(degradation_.is_feature_enabled("feature1"));
    EXPECT_TRUE(degradation_.is_feature_enabled("feature2"));
    EXPECT_TRUE(degradation_.is_feature_enabled("feature3"));
}

// Test feature disabling by priority
TEST_F(GracefulDegradationTest, FeatureDisablingByPriority) {
    degradation_.register_feature("critical", ServicePriority::CRITICAL);
    degradation_.register_feature("normal", ServicePriority::NORMAL);
    degradation_.register_feature("low", ServicePriority::LOW);
    
    degradation_.set_degradation_level(DegradationLevel::DEGRADED);
    EXPECT_TRUE(degradation_.is_feature_enabled("critical"));
    EXPECT_TRUE(degradation_.is_feature_enabled("normal"));
    EXPECT_FALSE(degradation_.is_feature_enabled("low"));
    
    degradation_.set_degradation_level(DegradationLevel::LIMITED);
    EXPECT_TRUE(degradation_.is_feature_enabled("critical"));
    EXPECT_FALSE(degradation_.is_feature_enabled("normal"));
    EXPECT_FALSE(degradation_.is_feature_enabled("low"));
    
    degradation_.set_degradation_level(DegradationLevel::EMERGENCY);
    EXPECT_TRUE(degradation_.is_feature_enabled("critical"));
    EXPECT_FALSE(degradation_.is_feature_enabled("normal"));
    EXPECT_FALSE(degradation_.is_feature_enabled("low"));
}

// Test resource-based degradation
TEST_F(GracefulDegradationTest, ResourceBasedDegradation) {
    degradation_.register_feature("feature1", ServicePriority::LOW);
    
    // Normal usage
    degradation_.update_resource_usage(100, 20.0, 100);
    EXPECT_EQ(degradation_.evaluate_degradation_level(), DegradationLevel::NORMAL);
    
    // High memory
    degradation_.update_resource_usage(950, 20.0, 100);
    EXPECT_EQ(degradation_.evaluate_degradation_level(), DegradationLevel::DEGRADED);
    
    // High CPU
    degradation_.update_resource_usage(100, 85.0, 100);
    EXPECT_EQ(degradation_.evaluate_degradation_level(), DegradationLevel::DEGRADED);
    
    // High connections
    degradation_.update_resource_usage(100, 20.0, 950);
    EXPECT_EQ(degradation_.evaluate_degradation_level(), DegradationLevel::DEGRADED);
}

// Test health-based degradation
TEST_F(GracefulDegradationTest, HealthBasedDegradation) {
    degradation_.update_health_score(0.8);
    EXPECT_EQ(degradation_.evaluate_degradation_level(), DegradationLevel::NORMAL);
    
    degradation_.update_health_score(0.4);
    EXPECT_EQ(degradation_.evaluate_degradation_level(), DegradationLevel::DEGRADED);
}

// Test required features
TEST_F(GracefulDegradationTest, RequiredFeatures) {
    degradation_.register_feature("required", ServicePriority::NORMAL, true);
    degradation_.register_feature("optional", ServicePriority::NORMAL, false);
    
    degradation_.set_degradation_level(DegradationLevel::EMERGENCY);
    
    // Required features should always be enabled
    EXPECT_TRUE(degradation_.is_feature_enabled("required"));
    EXPECT_FALSE(degradation_.is_feature_enabled("optional"));
}

// Test feature enable/disable
TEST_F(GracefulDegradationTest, FeatureEnableDisable) {
    degradation_.register_feature("feature1", ServicePriority::NORMAL);
    
    degradation_.disable_feature("feature1");
    EXPECT_FALSE(degradation_.is_feature_enabled("feature1"));
    
    degradation_.enable_feature("feature1");
    EXPECT_TRUE(degradation_.is_feature_enabled("feature1"));
}

// Test get enabled/disabled features
TEST_F(GracefulDegradationTest, GetEnabledDisabledFeatures) {
    degradation_.register_feature("critical", ServicePriority::CRITICAL);
    degradation_.register_feature("normal", ServicePriority::NORMAL);
    degradation_.register_feature("low", ServicePriority::LOW);
    
    degradation_.set_degradation_level(DegradationLevel::DEGRADED);
    
    auto enabled = degradation_.get_enabled_features();
    auto disabled = degradation_.get_disabled_features();
    
    EXPECT_TRUE(enabled.find("critical") != enabled.end());
    EXPECT_TRUE(enabled.find("normal") != enabled.end());
    EXPECT_TRUE(disabled.find("low") != disabled.end());
}

// Test automatic degradation evaluation
TEST_F(GracefulDegradationTest, AutomaticDegradationEvaluation) {
    degradation_.register_feature("low", ServicePriority::LOW);
    
    // Trigger degradation with high resource usage
    degradation_.update_resource_usage(950, 85.0, 950);
    
    DegradationLevel level = degradation_.evaluate_degradation_level();
    EXPECT_NE(level, DegradationLevel::NORMAL);
}

// Test degradation reason
TEST_F(GracefulDegradationTest, DegradationReason) {
    degradation_.update_resource_usage(950, 85.0, 950);
    degradation_.evaluate_degradation_level();
    
    EXPECT_FALSE(degradation_.get_degradation_reason().empty());
}

