/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

#pragma once
#ifndef NEGOTIO_NEGOTIATE_H
#define NEGOTIO_NEGOTIATE_H

#include "common.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <chrono>
#include <netinet/in.h>
#include <array>
#include <functional>  // ✅ 新增

namespace negotio {
    // 协商状态定义
    enum class NegotiateState {
        INIT,
        WAIT_R2,
        WAIT_CONFIRM,
        DONE,
        FAILED
    };

    // 单个协商会话结构体
    struct NegotiationSession {
        uint32_t policy_id;         ///< 策略ID，用作会话标识
        NegotiateState state;       ///< 当前协商状态
        std::vector<uint8_t> random1; ///< 发起方随机数 (32字节)
        std::vector<uint8_t> random2; ///< 响应方随机数 (32字节)
        std::vector<uint8_t> key;     ///< 计算得到的共享密钥 (32字节)
        std::chrono::steady_clock::time_point startTime; ///< 协商开始时间
    };

    class Monitor;

    // 会话桶结构体，用于分桶管理会话，降低锁竞争
    struct SessionBucket {
        std::unordered_map<uint32_t, NegotiationSession> sessions;
        std::mutex mtx;
    };

    // 定义分桶数量
    static const size_t NUM_BUCKETS = 16;

    // 定义 UDP 发送器函数类型
    using UdpSenderFunc = std::function<void(const NegotiationPacket&, const sockaddr_in&)>;

    class Negotiator {
    public:
        Negotiator();
        ~Negotiator();

        void setMonitor(Monitor *m);

        /**
         * 设置 UDP 发送器函数，供内部触发发送数据包（如 CONFIRM 包）
         */
        // negotiate.h
        using UdpSenderFunc = std::function<void(const NegotiationPacket &, const sockaddr_in &)>;
        void setUdpSender(const UdpSenderFunc &sender);

        void sendAsync(const NegotiationPacket &packet, const sockaddr_in &peerAddr) const;


        /**
         * @brief 发起协商流程（发起者角色）
         * @param policy_id 策略ID，同时作为会话标识
         * @param peerAddr 对端地址（UDP）
         * @return ErrorCode
         */
        ErrorCode startNegotiation(uint32_t policy_id, const sockaddr_in &peerAddr);

        /**
         * @brief 处理接收到的数据包（响应或确认）
         * @param packet 接收到的数据包
         * @param addr 发送方地址（UDP）
         * @return ErrorCode
         */
        ErrorCode handlePacket(const NegotiationPacket &packet, const sockaddr_in &addr);

        /**
         * @brief 获取指定会话信息（只读）
         * @param policy_id 策略ID
         * @return 若存在返回会话，否则返回 std::nullopt
         */
        std::optional<NegotiationSession> getSession(uint32_t policy_id);

        // 将 generateRandomData 从 private 移到 public，以便性能测试中调用
        static std::vector<uint8_t> generateRandomData(size_t size);

        // 计算共享密钥：SHA256(random1 || random2)
        static std::vector<uint8_t> computeKey(const std::vector<uint8_t> &random1, const std::vector<uint8_t> &random2);

    private:
        // 分桶管理会话，每个桶独立加锁，减少锁竞争
        std::array<SessionBucket, NUM_BUCKETS> sessionBuckets;

        Monitor *monitor;

        UdpSenderFunc udpSender;  ///< ✅ UDP 发送回调函数

        /**
         * @brief 根据 policy_id 获取对应的桶索引
         * @param policy_id 策略ID
         * @return 桶索引
         */
        static size_t bucketIndex(uint32_t policy_id) {
            return policy_id % NUM_BUCKETS;
        }

        /**
         * @brief 构造数据包
         * @param type 数据包类型
         * @param policy_id 策略ID
         * @param payloadData payload 数据（可以为空）
         * @return 构造好的 NegotiationPacket
         */
        static NegotiationPacket createPacket(PacketType type, uint32_t policy_id, const std::vector<uint8_t>& payloadData);
    };
} // namespace negotio

#endif // NEGOTIO_NEGOTIATE_H
