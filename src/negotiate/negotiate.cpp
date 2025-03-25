/**
 * @author fanfan187
 * @version v1.1.1
 * @since v1.1.1
 *
 * 优化说明：
 * 1. 在 computeKey 中预先分配固定大小缓冲区，避免多次 vector 插入带来的开销。
 * 2. 移除了大量调试输出（日志），减少 I/O 延迟。
 * 3. 会话管理采用分桶机制，将全局会话表拆分为多个桶，每个桶独立加锁，
 *    降低高并发下的锁竞争。
 */

#include "../monitor/monitor.h"
#include "negotiate.h"
#include "../hash/hash.h"
#include <openssl/rand.h>
#include <cstring>

namespace negotio {
    Negotiator::Negotiator() : monitor(nullptr) {
        // 可选：预先清空各个桶（默认构造已完成初始化）
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
        // 假定 random1 与 random2 长度均为 RANDOM_NUMBER（例如32字节）
        std::vector<uint8_t> concat(RANDOM_NUMBER * 2);
        std::memcpy(concat.data(), random1.data(), RANDOM_NUMBER);
        std::memcpy(concat.data() + RANDOM_NUMBER, random2.data(), RANDOM_NUMBER);
        return CalculateSHA256(concat);
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

        // 将会话存入对应的桶中
        {
            const size_t idx = bucketIndex(policy_id);
            std::lock_guard lock(sessionBuckets[idx].mtx);
            sessionBuckets[idx].sessions[policy_id] = session;
        }

        NegotiationPacket packet;
        packet.header.magic = MAGIC_NUMBER;
        packet.header.type = PacketType::RANDOM1;
        packet.header.sequence = policy_id;
        packet.header.timestamp = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                session.startTime.time_since_epoch()).count());

        // payload 按unit32_t 数组存放
        if (session.random1.size() % sizeof(uint32_t) != 0) {
            return ErrorCode::INVALID_PARAM;
        }
        const size_t num = session.random1.size() / sizeof(uint32_t);
        packet.payload.resize(num);
        std::memcpy(packet.payload.data(), session.random1.data(), session.random1.size());

        // 上层应调用 UDP 模块发送 packet 到 peerAddr（这里仅返回 SUCCESS）
        return ErrorCode::SUCCESS;
    }

    ErrorCode Negotiator::handlePacket(const NegotiationPacket &packet, const sockaddr_in &peerAddr) {
        const uint32_t policy_id = packet.header.sequence;
        if (packet.header.type == PacketType::RANDOM1) {
            // 响应者角色：收到 RANDOM1 包后生成随机数 R2，计算共享密钥
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

            // 存入分桶管理的会话中
            size_t idx = bucketIndex(policy_id); {
                std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx);
                sessionBuckets[idx].sessions[policy_id] = session;
            }

            NegotiationPacket respPacket;
            respPacket.header.magic = MAGIC_NUMBER;
            respPacket.header.type = PacketType::RANDOM2;
            respPacket.header.sequence = policy_id;
            respPacket.header.timestamp = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    session.startTime.time_since_epoch()).count());
            size_t num = session.random2.size() / sizeof(uint32_t);
            respPacket.payload.resize(num);
            std::memcpy(respPacket.payload.data(), session.random2.data(), session.random2.size());

            // 上层应调用 UDP 模块发送 respPacket 到 addr
            return ErrorCode::SUCCESS;
        }
        if (packet.header.type == PacketType::RANDOM2) {
            // 发起者角色：收到 RANDOM2 包后完成计算，准备发送 CONFIRM 包
            size_t idx = bucketIndex(policy_id); {
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

            NegotiationPacket confirmPacket;
            confirmPacket.header.magic = MAGIC_NUMBER;
            confirmPacket.header.type = PacketType::CONFIRM;
            confirmPacket.header.sequence = policy_id;
            confirmPacket.header.timestamp = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            // 上层应调用 UDP 模块发送 confirmPacket 到 addr
            return ErrorCode::SUCCESS;
        }
        if (packet.header.type == PacketType::CONFIRM) {
            size_t idx = bucketIndex(policy_id); {
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
