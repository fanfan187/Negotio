/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

// tests/unit_test/negotiate_test.cpp

#include <gtest/gtest.h>
#include "../../src/negotiate/negotiate.h"
#include "../../src/monitor/monitor.h"
#include <netinet/in.h>
#include <cstring>

using namespace negotio;

// 验证随机数生成器返回 32 字节的数据
TEST(NegotiatorTest, GenerateRandomData) {
    auto data = Negotiator::generateRandomData(RANDOM_NUMBER);
    EXPECT_EQ(data.size(), RANDOM_NUMBER);
}

// 验证 computeKey 正确拼接并计算 SHA256
TEST(NegotiatorTest, ComputeKey) {
    std::vector<uint8_t> r1(RANDOM_NUMBER, 0x11); // 32字节，全为0x11
    std::vector<uint8_t> r2(RANDOM_NUMBER, 0x22); // 32字节，全为0x22

    auto key = Negotiator::computeKey(r1, r2);
    EXPECT_EQ(key.size(), KEY_SIZE);
}

// 测试 startNegotiation 成功并写入桶中
TEST(NegotiatorTest, StartNegotiation) {
    Negotiator negotiator;
    sockaddr_in dummyAddr{};
    uint32_t policy_id = 123;

    auto result = negotiator.startNegotiation(policy_id, dummyAddr);
    EXPECT_EQ(result, ErrorCode::SUCCESS);

    auto sessionOpt = negotiator.getSession(policy_id);
    ASSERT_TRUE(sessionOpt.has_value());
    EXPECT_EQ(sessionOpt->policy_id, policy_id);
    EXPECT_EQ(sessionOpt->state, NegotiateState::WAIT_R2);
    EXPECT_EQ(sessionOpt->random1.size(), RANDOM_NUMBER);
    EXPECT_TRUE(sessionOpt->key.empty()); // 尚未生成密钥
}

// 模拟协商流程（RANDOM1 -> RANDOM2 -> CONFIRM）
TEST(NegotiatorTest, FullNegotiationFlow) {
    Negotiator initiator;
    Negotiator responder;
    Monitor monitor;
    initiator.setMonitor(&monitor);
    responder.setMonitor(&monitor);

    uint32_t policy_id = 456;
    sockaddr_in dummyAddr{};

    // Initiator 发起协商
    ASSERT_EQ(initiator.startNegotiation(policy_id, dummyAddr), ErrorCode::SUCCESS);
    auto session1 = initiator.getSession(policy_id);
    ASSERT_TRUE(session1.has_value());
    auto r1 = session1->random1;

    // responder 处理 RANDOM1
    NegotiationPacket p1 = Negotiator::createPacket(PacketType::RANDOM1, policy_id, r1);
    ASSERT_EQ(responder.handlePacket(p1, dummyAddr), ErrorCode::SUCCESS);
    auto session2 = responder.getSession(policy_id);
    ASSERT_TRUE(session2.has_value());
    auto r2 = session2->random2;

    // initiator 处理 RANDOM2
    NegotiationPacket p2 = Negotiator::createPacket(PacketType::RANDOM2, policy_id, r2);
    ASSERT_EQ(initiator.handlePacket(p2, dummyAddr), ErrorCode::SUCCESS);

    // responder 处理 CONFIRM
    NegotiationPacket p3 = Negotiator::createPacket(PacketType::CONFIRM, policy_id, {});
    ASSERT_EQ(responder.handlePacket(p3, dummyAddr), ErrorCode::SUCCESS);

    // 检查协商完成状态
    auto finalSession1 = initiator.getSession(policy_id);
    ASSERT_TRUE(finalSession1.has_value());
    EXPECT_EQ(finalSession1->state, NegotiateState::WAIT_CONFIRM);  // 发起方未处理 CONFIRM

    auto finalSession2 = responder.getSession(policy_id);
    ASSERT_TRUE(finalSession2.has_value());
    EXPECT_EQ(finalSession2->state, NegotiateState::DONE);
    EXPECT_EQ(finalSession2->key.size(), KEY_SIZE);
}

