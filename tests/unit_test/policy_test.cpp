/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

// tests/unit_test/policy_test.cpp

#include <gtest/gtest.h>
#include "../../src/policy/policy.h"

using namespace negotio;

// 构造一个简单策略配置
PolicyConfig makePolicy(uint32_t id, const std::string &ip = "192.168.1.1", uint16_t port = 9000) {
    PolicyConfig cfg;
    cfg.policy_id = id;
    cfg.remote_ip = ip;
    cfg.remote_port = port;
    cfg.timeout_ms = 100;
    cfg.retry_times = 3;
    return cfg;
}

TEST(PolicyManagerTest, AddPolicySuccess) {
    PolicyManager manager;
    auto policy = makePolicy(1);
    EXPECT_TRUE(manager.addPolicy(policy));
}

TEST(PolicyManagerTest, AddDuplicatePolicyFails) {
    PolicyManager manager;
    auto policy = makePolicy(42);
    EXPECT_TRUE(manager.addPolicy(policy));
    EXPECT_FALSE(manager.addPolicy(policy));  // 不允许重复添加
}

TEST(PolicyManagerTest, RemoveExistingPolicy) {
    PolicyManager manager;
    auto policy = makePolicy(88);
    manager.addPolicy(policy);
    EXPECT_TRUE(manager.removePolicy(88));
    EXPECT_FALSE(manager.checkPolicy(88));
}

TEST(PolicyManagerTest, RemoveNonexistentPolicyReturnsFalse) {
    PolicyManager manager;
    EXPECT_FALSE(manager.removePolicy(999)); // 不存在
}

TEST(PolicyManagerTest, CheckPolicyExistence) {
    PolicyManager manager;
    auto policy = makePolicy(100);
    manager.addPolicy(policy);
    EXPECT_TRUE(manager.checkPolicy(100));
    EXPECT_FALSE(manager.checkPolicy(200));
}

TEST(PolicyManagerTest, GetPolicySuccessAndFailure) {
    PolicyManager manager;
    auto policy = makePolicy(7, "10.0.0.1", 8000);
    manager.addPolicy(policy);

    auto result = manager.getPolicy(7);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->remote_ip, "10.0.0.1");
    EXPECT_EQ(result->remote_port, 8000);

    EXPECT_FALSE(manager.getPolicy(8888).has_value()); // 不存在
}

TEST(PolicyManagerTest, MaxPoliciesLimit) {
    PolicyManager manager;
    bool allAdded = true;
    for (uint32_t i = 0; i < 4096; ++i) {
        if (!manager.addPolicy(makePolicy(i))) {
            allAdded = false;
            break;
        }
    }
    EXPECT_TRUE(allAdded); // 应该能成功添加 4096 条策略

    // 第 4097 条应该失败
    EXPECT_FALSE(manager.addPolicy(makePolicy(999999)));
}
