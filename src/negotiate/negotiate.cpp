/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

#include "../monitor/monitor.h"
#include "negotiate.h"
#include "../hash/hash.h"  // 调用SHA256计算函数
#include <openssl/rand.h>
#include <cstring>
#include <iostream>

namespace negotio {

Negotiator::Negotiator(): monitor(nullptr) {
}

Negotiator::~Negotiator() {
}

void Negotiator::setMonitor(Monitor* m) {
    monitor = m;
}

std::vector<uint8_t> Negotiator::generateRandomData(size_t size) {
    std::vector<uint8_t> data(size);
    if (RAND_bytes(data.data(), static_cast<int>(size)) != 1) {
        // RAND_bytes 返回 1 表示成功
        data.clear();
    }
    return data;
}

std::vector<uint8_t> Negotiator::computeKey(const std::vector<uint8_t>& random1, const std::vector<uint8_t>& random2) {
    std::vector<uint8_t> concat;
    concat.reserve(random1.size() + random2.size());
    concat.insert(concat.end(), random1.begin(), random1.end());
    concat.insert(concat.end(), random2.begin(), random2.end());
    return CalculateSHA256(concat);
}

ErrorCode Negotiator::startNegotiation(uint32_t policy_id, const sockaddr_in &peerAddr) {
    // 发起者：生成随机数 R1，并构造 RANDOM1 包
    NegotiationSession session;
    session.policy_id = policy_id;
    session.state = NegotiateState::WAIT_R2;
    session.random1 = generateRandomData(RANDOM_NUMBER);
    if (session.random1.empty()) {
        return ErrorCode::MEMORY_ERROR;
    }
    session.startTime = std::chrono::steady_clock::now();
    session.key.clear();

    {
        std::lock_guard<std::mutex> lock(sessionsMutex);
        sessions[policy_id] = session;
    }

    // 构造 RANDOM1 数据包
    NegotiationPacket packet;
    packet.header.magic = MAGIC_NUMBER;
    packet.header.type = PacketType::RANDOM1;
    packet.header.sequence = policy_id;
    packet.header.timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                   session.startTime.time_since_epoch()).count());

    // 将随机数 R1 分组为 uint32_t 数组 (32字节 / 4 = 8)
    if (session.random1.size() % sizeof(uint32_t) != 0) {
        return ErrorCode::INVALID_PARAM;
    }
    size_t num = session.random1.size() / sizeof(uint32_t);
    packet.payload.resize(num);
    std::memcpy(packet.payload.data(), session.random1.data(), session.random1.size());

    std::cout << "发起者：发送 RANDOM1 包, 策略ID: " << policy_id << std::endl;
    // 上层应调用 UDP 模块发送 packet 到 peerAddr
    return ErrorCode::SUCCESS;
}

ErrorCode Negotiator::handlePacket(const NegotiationPacket &packet, const sockaddr_in &addr) {
    uint32_t policy_id = packet.header.sequence;
    if (packet.header.type == PacketType::RANDOM1) {
        // 对方发起协商，我作为响应者
        NegotiationSession session;
        session.policy_id = policy_id;
        session.state = NegotiateState::WAIT_CONFIRM;
        size_t randomBytes = RANDOM_NUMBER;
        if (packet.payload.size() * sizeof(uint32_t) < randomBytes) {
            return ErrorCode::INVALID_PARAM;
        }
        session.random1.resize(randomBytes);
        std::memcpy(session.random1.data(), packet.payload.data(), randomBytes);

        // 生成 R2
        session.random2 = generateRandomData(RANDOM_NUMBER);
        if (session.random2.empty()) {
            return ErrorCode::MEMORY_ERROR;
        }
        // 计算共享密钥
        session.key = computeKey(session.random1, session.random2);
        session.startTime = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(sessionsMutex);
            sessions[policy_id] = session;
        }

        // 构造 RANDOM2 数据包
        NegotiationPacket respPacket;
        respPacket.header.magic = MAGIC_NUMBER;
        respPacket.header.type = PacketType::RANDOM2;
        respPacket.header.sequence = policy_id;
        respPacket.header.timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                           session.startTime.time_since_epoch()).count());
        size_t num = session.random2.size() / sizeof(uint32_t);
        respPacket.payload.resize(num);
        std::memcpy(respPacket.payload.data(), session.random2.data(), session.random2.size());

        std::cout << "响应者：接收到 RANDOM1 包, 发送 RANDOM2 包, 策略ID: " << policy_id << std::endl;
        // 上层应调用 UDP 模块发送 respPacket 到 addr
        return ErrorCode::SUCCESS;
    }
    else if (packet.header.type == PacketType::RANDOM2) {
        // 发起者收到 RANDOM2 包，完成计算并发送 CONFIRM 包
        std::lock_guard<std::mutex> lock(sessionsMutex);
        auto it = sessions.find(policy_id);
        if (it == sessions.end()) {
            return ErrorCode::INVALID_PARAM;
        }
        NegotiationSession &session = it->second;
        if (session.state != NegotiateState::WAIT_R2) {
            return ErrorCode::INVALID_PARAM;
        }
        size_t randomBytes = RANDOM_NUMBER;
        if (packet.payload.size() * sizeof(uint32_t) < randomBytes) {
            return ErrorCode::INVALID_PARAM;
        }
        session.random2.resize(randomBytes);
        std::memcpy(session.random2.data(), packet.payload.data(), randomBytes);
        session.key = computeKey(session.random1, session.random2);
        session.state = NegotiateState::WAIT_CONFIRM;

        NegotiationPacket confirmPacket;
        confirmPacket.header.magic = MAGIC_NUMBER;
        confirmPacket.header.type = PacketType::CONFIRM;
        confirmPacket.header.sequence = policy_id;
        confirmPacket.header.timestamp = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch()).count());
        std::cout << "发起者：收到 RANDOM2 包, 发送 CONFIRM 包, 策略ID: " << policy_id << std::endl;
        // 上层应调用 UDP 模块发送 confirmPacket 到 addr
        return ErrorCode::SUCCESS;
    }
    else if (packet.header.type == PacketType::CONFIRM) {
        // 响应者收到 CONFIRM 包，协商成功
        std::lock_guard<std::mutex> lock(sessionsMutex);
        const auto it = sessions.find(policy_id);
        if (it == sessions.end()) {
            return ErrorCode::INVALID_PARAM;
        }
        NegotiationSession &session = it->second;
        session.state = NegotiateState::DONE;
        auto now = std::chrono::steady_clock::now();
        uint32_t duration = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - session.startTime).count());
        if (monitor) {
            monitor->recordNegotiation(duration, true);
        }
        std::cout << "协商完成, 策略ID: " << policy_id << std::endl;
        return ErrorCode::SUCCESS;
    }
    return ErrorCode::INVALID_PARAM;
}

std::optional<NegotiationSession> Negotiator::getSession(uint32_t policy_id) {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    const auto it = sessions.find(policy_id);
    if (it != sessions.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace negotio
