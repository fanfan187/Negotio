#include "../monitor/monitor.h"
#include "negotiate.h"
#include "../hash/hash.h"
#include <openssl/rand.h>
#include <cstring>
#include <chrono>
#include <iostream>

namespace negotio {
    Negotiator::Negotiator() : monitor(nullptr) {
    }

    Negotiator::~Negotiator() = default;

    void Negotiator::setMonitor(Monitor *m) {
        monitor = m;
    }

    void Negotiator::setUdpSender(const UdpSenderFunc &sender) {
        udpSender = sender;
    }

    void Negotiator::sendAsync(const NegotiationPacket &packet, const sockaddr_in &peerAddr) const {
        std::thread([this, packet, peerAddr]() {
            if (udpSender) {
                udpSender(packet, peerAddr);  // 通过 UDP 发送数据包
            }
        }).detach();  // 异步发送，避免阻塞
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

    NegotiationPacket Negotiator::createPacket(PacketType type, uint32_t policy_id,
                                               const std::vector<uint8_t> &payloadData) {
        NegotiationPacket packet{};
        packet.header.magic = MAGIC_NUMBER;
        packet.header.type = type;
        packet.header.sequence = policy_id;
        packet.header.timestamp = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
        packet.header.payload_len = payloadData.size() / sizeof(uint32_t);
        if (!payloadData.empty()) {
            packet.payload.resize(packet.header.payload_len);
            std::memcpy(packet.payload.data(), payloadData.data(), payloadData.size());
        }
        return packet;
    }

    ErrorCode Negotiator::startNegotiation(uint32_t policy_id, const sockaddr_in &peerAddr) {
        // 过滤无效的 policy_id
        if (policy_id == 0) {
            std::cout << "[TRACE] 忽略无效 policy_id: 0 (startNegotiation)" << std::endl;
            return ErrorCode::INVALID_PARAM;
        }
        NegotiationSession session;
        session.policy_id = policy_id;
        session.state = NegotiateState::WAIT_R2;
        session.random1 = generateRandomData(RANDOM_NUMBER);
        if (session.random1.empty()) return ErrorCode::MEMORY_ERROR;
        session.startTime = std::chrono::steady_clock::now();
        {
            const size_t idx = bucketIndex(policy_id);
            std::lock_guard lock(sessionBuckets[idx].mtx);
            sessionBuckets[idx].sessions[policy_id] = session;
        }

        NegotiationPacket packet = createPacket(PacketType::RANDOM1, policy_id, session.random1);

        std::cout << "[TRACE] 发起协商: policy_id = " << policy_id << std::endl;

        if (udpSender) {
            udpSender(packet, peerAddr);
        }
        return ErrorCode::SUCCESS;
    }

    ErrorCode Negotiator::handlePacket(const NegotiationPacket &packet, const sockaddr_in &peerAddr) {
        const uint32_t policy_id = packet.header.sequence;
        // 过滤无效的 policy_id
        if (policy_id == 0) {
            std::cout << "[TRACE] 忽略无效 policy_id: 0 (handlePacket)" << std::endl;
            return ErrorCode::INVALID_PARAM;
        }
        const auto now = std::chrono::steady_clock::now();
        const size_t idx = bucketIndex(policy_id); // 缓存 sessionBuckets 索引

        switch (packet.header.type) {
            case PacketType::RANDOM1: {
                {
                    // 将锁定范围最小化，锁定后尽快释放
                    std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx);
                    if (sessionBuckets[idx].sessions.find(policy_id) != sessionBuckets[idx].sessions.end()) {
                        // 已发起协商的发起方收到误发的 RANDOM1，忽略
                        return ErrorCode::SUCCESS;
                    }
                }

                std::cout << "[TRACE] responder 收到 RANDOM1, 自动响应, policy_id = " << policy_id << std::endl;

                NegotiationSession session;
                session.policy_id = policy_id;
                session.state = NegotiateState::WAIT_CONFIRM;
                session.startTime = now;

                if (packet.payload.size() * sizeof(uint32_t) < RANDOM_NUMBER) {
                    return ErrorCode::INVALID_PARAM;
                }

                session.random1.resize(RANDOM_NUMBER);
                std::memcpy(session.random1.data(), packet.payload.data(), RANDOM_NUMBER);
                session.random2 = generateRandomData(RANDOM_NUMBER);
                session.key = computeKey(session.random1, session.random2);
                {
                    std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx); // 锁住 sessionBuckets，更新会话信息
                    sessionBuckets[idx].sessions[policy_id] = session;
                }

                if (udpSender) {
                    auto response = createPacket(PacketType::RANDOM2, policy_id, session.random2);
                    udpSender(response, peerAddr);
                }

                return ErrorCode::SUCCESS;
            }

            case PacketType::RANDOM2: {
                std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx); // 锁住 sessionBuckets，处理 RANDOM2 包
                auto it = sessionBuckets[idx].sessions.find(policy_id);
                if (it == sessionBuckets[idx].sessions.end()) return ErrorCode::INVALID_PARAM;

                NegotiationSession &session = it->second;
                if (session.state != NegotiateState::WAIT_R2) return ErrorCode::INVALID_PARAM;

                if (packet.payload.size() * sizeof(uint32_t) < RANDOM_NUMBER) return ErrorCode::INVALID_PARAM;

                session.random2.resize(RANDOM_NUMBER);
                std::memcpy(session.random2.data(), packet.payload.data(), RANDOM_NUMBER);
                session.key = computeKey(session.random1, session.random2);
                session.state = NegotiateState::WAIT_CONFIRM;

                if (udpSender) {
                    auto confirm = createPacket(PacketType::CONFIRM, policy_id, {});
                    udpSender(confirm, peerAddr);
                }

                session.state = NegotiateState::DONE;
                if (monitor) {
                    uint32_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - session.startTime).count();
                    monitor->recordNegotiation(duration, true);
                    std::cout << "[TRACE] initiator 协商完成, 耗时: " << duration << "ms, policy_id = " << policy_id << std::endl;
                }

                return ErrorCode::SUCCESS;
            }

            case PacketType::CONFIRM: {
                std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx); // 锁住 sessionBuckets，处理 CONFIRM 包
                auto it = sessionBuckets[idx].sessions.find(policy_id);
                if (it == sessionBuckets[idx].sessions.end()) return ErrorCode::INVALID_PARAM;

                NegotiationSession &session = it->second;
                session.state = NegotiateState::DONE;

                if (monitor) {
                    uint32_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - session.startTime).count();
                    monitor->recordNegotiation(duration, true);
                    std::cout << "[TRACE] responder 协商完成, 耗时: " << duration << "ms, policy_id = " << policy_id << std::endl;
                }

                return ErrorCode::SUCCESS;
            }

            default:
                return ErrorCode::INVALID_PARAM;
        }
    }
} // namespace negotio
