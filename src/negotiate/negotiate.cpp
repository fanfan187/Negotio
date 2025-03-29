/**
 * @file negotiate.cpp
 * @brief 协商模块实现
 */

#include "../monitor/monitor.h"
#include "negotiate.h"
#include "../hash/hash.h"
#include <openssl/rand.h>
#include <cstring>
#include <chrono>

namespace negotio {

    Negotiator::Negotiator() : monitor(nullptr) {
    }

    Negotiator::~Negotiator() = default;

    void Negotiator::setMonitor(Monitor *m) {
        monitor = m;
    }

    std::vector<uint8_t> Negotiator::generateRandomData(size_t size) {
        std::vector<uint8_t> data(size);
        if (RAND_bytes(data.data(), static_cast<int>(size)) != 1) {
            data.clear();
        }
        return data;
    }

    std::vector<uint8_t> Negotiator::computeKey(const std::vector<uint8_t> &random1,
                                                const std::vector<uint8_t> &random2) {
        std::vector<uint8_t> concat(RANDOM_NUMBER * 2);
        std::memcpy(concat.data(), random1.data(), RANDOM_NUMBER);
        std::memcpy(concat.data() + RANDOM_NUMBER, random2.data(), RANDOM_NUMBER);
        return CalculateSHA256(concat);
    }

    // 新增辅助函数，用于构造数据包，减少重复代码
    NegotiationPacket Negotiator::createPacket(PacketType type, uint32_t policy_id, const std::vector<uint8_t>& payloadData) {
        NegotiationPacket packet{};
        packet.header.magic = MAGIC_NUMBER;
        packet.header.type = type;
        packet.header.sequence = policy_id;
        packet.header.timestamp = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
        // 假设 payloadData 的字节数除以 sizeof(uint32_t) 为 payload_len
        packet.header.payload_len = payloadData.empty() ? 0 : static_cast<uint32_t>(payloadData.size() / sizeof(uint32_t));
        if (!payloadData.empty()) {
            size_t num = payloadData.size() / sizeof(uint32_t);
            packet.payload.resize(num);
            std::memcpy(packet.payload.data(), payloadData.data(), payloadData.size());
        }
        return packet;
    }

    ErrorCode Negotiator::startNegotiation(uint32_t policy_id, const sockaddr_in &peerAddr) {
        NegotiationSession session;
        session.policy_id = policy_id;
        session.state = NegotiateState::WAIT_R2;
        session.random1 = generateRandomData(RANDOM_NUMBER);
        if (session.random1.empty()) {
            return ErrorCode::MEMORY_ERROR;
        }
        session.startTime = std::chrono::steady_clock::now();
        session.key.clear();

        // 存入分桶
        {
            const size_t idx = bucketIndex(policy_id);
            std::lock_guard lock(sessionBuckets[idx].mtx);
            sessionBuckets[idx].sessions[policy_id] = session;
        }

        // 使用辅助函数构造数据包
        NegotiationPacket packet = createPacket(PacketType::RANDOM1, policy_id, session.random1);

        // 此处上层应调用 UDP 模块发送 packet 到 peerAddr
        return ErrorCode::SUCCESS;
    }

    ErrorCode Negotiator::handlePacket(const NegotiationPacket &packet, const sockaddr_in &peerAddr) {
        const uint32_t policy_id = packet.header.sequence;
        if (packet.header.type == PacketType::RANDOM1) {
            // 响应者角色
            NegotiationSession session;
            session.policy_id = policy_id;
            session.state = NegotiateState::WAIT_CONFIRM;
            constexpr size_t randomBytes = RANDOM_NUMBER;
            if (packet.payload.size() * sizeof(uint32_t) < randomBytes) {
                return ErrorCode::INVALID_PARAM;
            }
            session.random1.resize(randomBytes);
            std::memcpy(session.random1.data(), packet.payload.data(), randomBytes);

            session.random2 = generateRandomData(RANDOM_NUMBER);
            if (session.random2.empty()) {
                return ErrorCode::MEMORY_ERROR;
            }
            session.key = computeKey(session.random1, session.random2);
            session.startTime = std::chrono::steady_clock::now();

            {
                size_t idx = bucketIndex(policy_id);
                std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx);
                sessionBuckets[idx].sessions[policy_id] = session;
            }

            NegotiationPacket respPacket = createPacket(PacketType::RANDOM2, policy_id, session.random2);
            // 上层调用 UDP 模块发送 respPacket 到对端地址
            return ErrorCode::SUCCESS;
        }
        if (packet.header.type == PacketType::RANDOM2) {
            size_t idx = bucketIndex(policy_id);
            {
                std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx);
                auto it = sessionBuckets[idx].sessions.find(policy_id);
                if (it == sessionBuckets[idx].sessions.end()) {
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
            }
            NegotiationPacket confirmPacket = createPacket(PacketType::CONFIRM, policy_id, {});
            // 上层调用 UDP 模块发送 confirmPacket
            return ErrorCode::SUCCESS;
        }
        if (packet.header.type == PacketType::CONFIRM) {
            size_t idx = bucketIndex(policy_id);
            {
                std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx);
                auto it = sessionBuckets[idx].sessions.find(policy_id);
                if (it == sessionBuckets[idx].sessions.end()) {
                    return ErrorCode::INVALID_PARAM;
                }
                NegotiationSession &session = it->second;
                session.state = NegotiateState::DONE;
                const auto now = std::chrono::steady_clock::now();
                const uint32_t duration = static_cast<uint32_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now - session.startTime).count());
                if (monitor) {
                    monitor->recordNegotiation(duration, true);
                }
            }
            return ErrorCode::SUCCESS;
        }
        return ErrorCode::INVALID_PARAM;
    }

    std::optional<NegotiationSession> Negotiator::getSession(uint32_t policy_id) {
        const size_t idx = bucketIndex(policy_id);
        std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx);
        if (const auto it = sessionBuckets[idx].sessions.find(policy_id); it != sessionBuckets[idx].sessions.end()) {
            return it->second;
        }
        return std::nullopt;
    }
} // namespace negotio
