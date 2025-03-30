/**
 * performance_test.cpp
 *
 * 性能测试示例：
 * 1. 模拟大量协商流程（例如 100,000 次协商）
 * 2. 使用多线程分摊协商任务
 * 3. 模拟完整三包交互：
 *    - 发起者调用 startNegotiation 创建会话（状态为 WAIT_R2）
 *    - 模拟响应者生成随机数 R2 并构造 RANDOM2 数据包，调用 handlePacket 更新会话状态
 *    - 模拟响应者发送 CONFIRM 数据包，调用 handlePacket 完成协商
 * 4. 统计总耗时和平均每次协商耗时
 *
 * 注意：本测试代码仅调用 Negotiator 模块接口进行逻辑验证和性能统计，
 *       使用 dummy 网络地址（loopback）模拟网络收发，不执行真实的网络 I/O。
 */

#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <cstring>
#include <netinet/in.h>

#include "../src/negotiate/negotiate.h"
#include "../src/monitor/monitor.h"
#include "common.h" // 包含 ErrorCode、MAGIC_NUMBER、PacketType、RANDOM_NUMBER 等定义
#include <openssl/rand.h>

using namespace negotio;
using namespace std::chrono;

// 全局 dummy 地址（模拟接收方）
sockaddr_in dummyAddr;

//
// 模拟一次完整的协商流程：
// 1. 发起者调用 startNegotiation 创建会话（状态为 WAIT_R2）
// 2. 模拟响应者生成随机数 R2，构造 RANDOM2 包，并调用 handlePacket 更新会话状态
// 3. 模拟响应者发送 CONFIRM 包，调用 handlePacket 完成协商
//
void simulateNegotiationFlow(Negotiator &negotiator, uint32_t startId, uint32_t numSessions) {
    for (uint32_t policyId = startId; policyId < startId + numSessions; policyId++) {
        // 1. 发起者启动协商，创建会话
        ErrorCode ec = negotiator.startNegotiation(policyId, dummyAddr);
        if (ec != ErrorCode::SUCCESS) {
            std::cerr << "startNegotiation 失败，策略ID: " << policyId << std::endl;
            continue;
        }

        // 2. 模拟响应者生成随机数 R2
        // 使用 Negotiator::generateRandomData 生成随机数（静态函数调用）
        std::vector<uint8_t> responderRandom2 = Negotiator::generateRandomData(RANDOM_NUMBER);
        if (responderRandom2.empty()) {
            std::cerr << "Responder生成随机数失败，策略ID: " << policyId << std::endl;
            continue;
        }
        // 构造 RANDOM2 数据包
        NegotiationPacket random2Packet;
        random2Packet.header.magic = MAGIC_NUMBER;
        random2Packet.header.type = PacketType::RANDOM2;
        random2Packet.header.sequence = policyId;
        size_t random2Count = responderRandom2.size() / sizeof(uint32_t);
        random2Packet.payload.resize(random2Count);
        std::memcpy(random2Packet.payload.data(), responderRandom2.data(), responderRandom2.size());
        ec = negotiator.handlePacket(random2Packet, dummyAddr);
        if (ec != ErrorCode::SUCCESS) {
            std::cerr << "handlePacket (RANDOM2) 失败，策略ID: " << policyId << std::endl;
            continue;
        }

        // 3. 模拟响应者发送 CONFIRM 数据包
        NegotiationPacket confirmPacket;
        confirmPacket.header.magic = MAGIC_NUMBER;
        confirmPacket.header.type = PacketType::CONFIRM;
        confirmPacket.header.sequence = policyId;
        confirmPacket.header.timestamp = static_cast<uint32_t>(
            duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
        ec = negotiator.handlePacket(confirmPacket, dummyAddr);
        if (ec != ErrorCode::SUCCESS) {
            std::cerr << "handlePacket (CONFIRM) 失败，策略ID: " << policyId << std::endl;
            continue;
        }
    }
}

int main() {
    // 初始化 dummyAddr（使用回环地址）
    std::memset(&dummyAddr, 0, sizeof(dummyAddr));
    dummyAddr.sin_family = AF_INET;
    dummyAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    dummyAddr.sin_port = htons(0);

    Negotiator negotiator;
    Monitor monitor;
    negotiator.setMonitor(&monitor);
    monitor.start();

    // 设置总协商次数
    constexpr uint32_t totalSessions = 4096;
    // 根据硬件线程数拆分任务
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    uint32_t sessionsPerThread = totalSessions / numThreads;

    std::vector<std::thread> threads;
    auto startTime = steady_clock::now();
    for (unsigned int i = 0; i < numThreads; i++) {
        uint32_t startId = i * sessionsPerThread;
        threads.emplace_back(simulateNegotiationFlow, std::ref(negotiator), startId, sessionsPerThread);
    }
    for (auto &t : threads) {
        t.join();
    }
    const auto endTime = steady_clock::now();
    const auto totalMs = duration_cast<milliseconds>(endTime - startTime).count();
    std::cout << "总协商次数: " << totalSessions
              << ", 总耗时: " << totalMs << " ms, 平均每次协商耗时: "
              << static_cast<double>(totalMs) / totalSessions << " ms" << std::endl;

    monitor.stop();
    return 0;
}
