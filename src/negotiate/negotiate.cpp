/**
 * @file negotiate.cpp
 * @brief 实现协商器功能，包括发起协商、处理协商数据包、生成随机数据和密钥计算等。
 *
 * 该文件实现了 Negotiator 类的方法，用于管理协商会话、异步发送数据包、生成随机数据、计算密钥、
 * 创建协商数据包以及处理接收到的协商数据包。协商过程涉及多个数据包类型，如 RANDOM1、RANDOM2 和 CONFIRM，
 * 并利用 Monitor 对象记录协商统计信息。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#include "../monitor/monitor.h"
#include "negotiate.h"
#include "../hash/hash.h"
#include <openssl/rand.h>
#include <cstring>
#include <chrono>
#include <iostream>

namespace negotio {

    /**
     * @brief 构造函数
     *
     * 初始化 Negotiator 对象，将 Monitor 指针设置为空。
     */
    Negotiator::Negotiator() : monitor(nullptr) {
    }

    /**
     * @brief 析构函数
     *
     * 默认析构函数。
     */
    Negotiator::~Negotiator() = default;

    /**
     * @brief 设置监控对象
     *
     * 关联一个 Monitor 对象，用于记录协商过程中的统计数据。
     *
     * @param m 指向 Monitor 对象的指针
     */
    void Negotiator::setMonitor(Monitor *m) {
        monitor = m;
    }

    /**
     * @brief 设置 UDP 发送函数
     *
     * 为 Negotiator 指定用于通过 UDP 发送数据包的回调函数。
     *
     * @param sender UDP 发送函数对象
     */
    void Negotiator::setUdpSender(const UdpSenderFunc &sender) {
        udpSender = sender;
    }

    /**
     * @brief 异步发送数据包
     *
     * 在新线程中通过 UDP 发送协商数据包，避免阻塞主线程。
     *
     * @param packet 要发送的协商数据包
     * @param peerAddr 目标端点地址
     */
    void Negotiator::sendAsync(const NegotiationPacket &packet, const sockaddr_in &peerAddr) const {
        std::thread([this, packet, peerAddr]() {
            if (udpSender) {
                udpSender(packet, peerAddr);  // 通过 UDP 发送数据包
            }
        }).detach();  // 异步发送，避免阻塞
    }

    /**
     * @brief 生成随机数据
     *
     * 使用 OpenSSL 的 RAND_bytes 函数生成指定大小的随机字节数据。
     *
     * @param size 生成的随机数据字节数
     * @return std::vector<uint8_t> 包含随机数据的字节向量；若生成失败则返回空向量
     */
    std::vector<uint8_t> Negotiator::generateRandomData(size_t size) {
        std::vector<uint8_t> data(size);
        if (RAND_bytes(data.data(), static_cast<int>(size)) != 1) {
            data.clear();  // 若随机生成失败，则清空数据
        }
        return data;
    }

    /**
     * @brief 计算协商密钥
     *
     * 将两个随机数据向量拼接后计算 SHA-256 哈希，作为协商密钥。
     *
     * @param random1 第一个随机数据向量
     * @param random2 第二个随机数据向量
     * @return std::vector<uint8_t> 计算得到的 SHA-256 哈希值
     */
    std::vector<uint8_t> Negotiator::computeKey(const std::vector<uint8_t> &random1,
                                                const std::vector<uint8_t> &random2) {
        std::vector<uint8_t> concat(RANDOM_NUMBER * 2);
        std::memcpy(concat.data(), random1.data(), RANDOM_NUMBER);
        std::memcpy(concat.data() + RANDOM_NUMBER, random2.data(), RANDOM_NUMBER);
        return CalculateSHA256(concat);
    }

    /**
     * @brief 创建协商数据包
     *
     * 根据传入的数据构造协商数据包，填充包头信息（魔数、类型、序列号、时间戳、负载长度）
     * 并将负载数据复制到数据包中。
     *
     * @param type 数据包类型（如 RANDOM1、RANDOM2、CONFIRM）
     * @param policy_id 协商会话的策略标识符
     * @param payloadData 要附加的负载数据
     * @return NegotiationPacket 构造好的协商数据包
     */
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

    /**
     * @brief 发起协商
     *
     * 根据指定的 policy_id 和目标地址，发起协商会话：
     * 1. 检查 policy_id 的合法性；
     * 2. 生成随机数据并初始化协商会话；
     * 3. 将会话存入会话桶；
     * 4. 构造并发送 RANDOM1 数据包。
     *
     * @param policy_id 协商会话的策略标识符（非 0）
     * @param peerAddr 目标端点地址
     * @return ErrorCode SUCCESS 表示成功，否则返回相应错误码
     */
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

        const NegotiationPacket packet = createPacket(PacketType::RANDOM1, policy_id, session.random1);

        std::cout << "[TRACE] 发起协商: policy_id = " << policy_id << std::endl;

        if (udpSender) {
            udpSender(packet, peerAddr);
        }
        return ErrorCode::SUCCESS;
    }

    /**
     * @brief 处理接收到的协商数据包
     *
     * 根据数据包的类型对协商会话进行不同处理：
     * - RANDOM1：若当前没有会话，则作为响应方自动响应，生成随机数、计算密钥并发送 RANDOM2；
     * - RANDOM2：若当前为发起方且状态正确，则接收 RANDOM2、计算密钥、发送 CONFIRM，并更新状态为 DONE；
     * - CONFIRM：作为响应方接收到确认包后，将会话状态更新为 DONE。
     *
     * 同时，在会话完成时通过 Monitor 记录协商耗时和成功状态。
     *
     * @param packet 接收到的协商数据包
     * @param peerAddr 数据包发送方的地址
     * @return ErrorCode SUCCESS 表示处理成功，否则返回相应错误码
     */
    ErrorCode Negotiator::handlePacket(const NegotiationPacket &packet, const sockaddr_in &peerAddr) {
        const uint32_t policy_id = packet.header.sequence;
        // 过滤无效的 policy_id
        if (policy_id == 0) {
            std::cout << "[TRACE] 忽略无效 policy_id: 0 (handlePacket)" << std::endl;
            return ErrorCode::INVALID_PARAM;
        }
        const auto now = std::chrono::steady_clock::now();
        // 缓存 sessionBuckets 索引
        const size_t idx = bucketIndex(policy_id);

        switch (packet.header.type) {
            case PacketType::RANDOM1: {
                {
                    // 锁定会话桶，缩小锁定范围以提高并发性能
                    std::lock_guard<std::mutex> lock(sessionBuckets[idx].mtx);
                    if (sessionBuckets[idx].sessions.contains(policy_id)) {
                        // 发起方收到重复的 RANDOM1（误发）时直接忽略
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

                // 复制随机数据
                session.random1.resize(RANDOM_NUMBER);
                std::memcpy(session.random1.data(), packet.payload.data(), RANDOM_NUMBER);
                session.random2 = generateRandomData(RANDOM_NUMBER);
                session.key = computeKey(session.random1, session.random2);
                {
                    // 更新会话信息
                    std::lock_guard lock(sessionBuckets[idx].mtx);
                    sessionBuckets[idx].sessions[policy_id] = session;
                }

                if (udpSender) {
                    auto response = createPacket(PacketType::RANDOM2, policy_id, session.random2);
                    udpSender(response, peerAddr);
                }

                return ErrorCode::SUCCESS;
            }

            case PacketType::RANDOM2: {
                // 锁定会话桶，处理 RANDOM2 包
                std::lock_guard lock(sessionBuckets[idx].mtx);
                const auto it = sessionBuckets[idx].sessions.find(policy_id);
                if (it == sessionBuckets[idx].sessions.end()) return ErrorCode::INVALID_PARAM;

                NegotiationSession &session = it->second;
                if (session.state != NegotiateState::WAIT_R2) return ErrorCode::INVALID_PARAM;

                if (packet.payload.size() * sizeof(uint32_t) < RANDOM_NUMBER) return ErrorCode::INVALID_PARAM;

                // 接收并复制随机数据
                session.random2.resize(RANDOM_NUMBER);
                std::memcpy(session.random2.data(), packet.payload.data(), RANDOM_NUMBER);
                session.key = computeKey(session.random1, session.random2);
                session.state = NegotiateState::WAIT_CONFIRM;

                if (udpSender) {
                    const auto confirm = createPacket(PacketType::CONFIRM, policy_id, {});
                    udpSender(confirm, peerAddr);
                }

                session.state = NegotiateState::DONE;
                if (monitor) {
                    const uint32_t duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - session.startTime).count();
                    monitor->recordNegotiation(duration, true);
                    std::cout << "[TRACE] initiator 协商完成, 耗时: " << duration << "ms, policy_id = " << policy_id << std::endl;
                }

                return ErrorCode::SUCCESS;
            }

            case PacketType::CONFIRM: {
                std::lock_guard lock(sessionBuckets[idx].mtx); // 锁定会话桶，处理 CONFIRM 包
                const auto it = sessionBuckets[idx].sessions.find(policy_id);
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
