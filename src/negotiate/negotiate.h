/**
 * @file negotiate.h
 * @brief 协议协商器头文件，定义了协商状态、会话、协商器类等。
 *
 * 用于实现基于策略的三包协商流程，支持异步 UDP 通信、会话并发管理、密钥协商等功能。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
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
#include <functional>

namespace negotio {
    /**
     * @brief 协商状态枚举
     */
    enum class NegotiateState {
        INIT, ///< 初始状态，尚未开始
        WAIT_R2, ///< 等待响应方 RANDOM2
        WAIT_CONFIRM, ///< 等待对方确认 CONFIRM 包
        DONE, ///< 协商成功完成
        FAILED ///< 协商失败
    };

    /**
     * @brief 单个协商会话信息结构体
     */
    struct NegotiationSession {
        uint32_t policy_id; ///< 策略ID，用作会话标识
        NegotiateState state; ///< 当前协商状态
        std::vector<uint8_t> random1; ///< 发起方随机数（32字节）
        std::vector<uint8_t> random2; ///< 响应方随机数（32字节）
        std::vector<uint8_t> key; ///< 计算得到的共享密钥（SHA256）
        std::chrono::steady_clock::time_point startTime; ///< 协商开始时间
    };

    class Monitor;

    /**
     * @brief 协商会话桶结构，用于并发分桶管理会话
     */
    struct SessionBucket {
        std::unordered_map<uint32_t, NegotiationSession> sessions;
        std::mutex mtx;
    };

    static const size_t NUM_BUCKETS = 16; ///< 会话分桶数量（必须为 2 的倍数以利哈希）

    /**
     * @brief UDP 发送函数类型定义
     *
     * 通过 std::function 包装一个函数，其可接受数据包和对端地址用于发送。
     */
    using UdpSenderFunc = std::function<void(const NegotiationPacket &, const sockaddr_in &)>;

    /**
     * @brief 协商器类，实现发起、处理、协商逻辑
     */
    class Negotiator {
    public:
        Negotiator();

        ~Negotiator();

        /**
         * @brief 设置监控器指针，用于统计协商成功率与延迟
         * @param m 指向 Monitor 对象
         */
        void setMonitor(Monitor *m);

        /**
         * @brief 设置 UDP 发送函数
         * @param sender 外部提供的 UDP 发送器
         */
        void setUdpSender(const UdpSenderFunc &sender);

        /**
         * @brief 异步发送一个协商数据包（新线程）
         * @param packet 要发送的数据包
         * @param peerAddr 接收方地址
         */
        void sendAsync(const NegotiationPacket &packet, const sockaddr_in &peerAddr) const;

        /**
         * @brief 发起协商（由 initiator 使用）
         *
         * 会自动生成 random1 并发送 RANDOM1 包。
         *
         * @param policy_id 策略 ID（会话标识符）
         * @param peerAddr 对端地址
         * @return ErrorCode 协商是否发起成功
         */
        ErrorCode startNegotiation(uint32_t policy_id, const sockaddr_in &peerAddr);

        /**
         * @brief 处理接收到的协商包
         *
         * 根据包类型分别处理 RANDOM1、RANDOM2 和 CONFIRM。
         *
         * @param packet 接收到的数据包
         * @param addr 数据包来源地址
         * @return ErrorCode 处理结果
         */
        ErrorCode handlePacket(const NegotiationPacket &packet, const sockaddr_in &addr);

        /**
         * @brief 生成随机数据
         * @param size 数据字节数
         * @return std::vector<uint8_t> 随机字节向量
         */
        static std::vector<uint8_t> generateRandomData(size_t size);

        /**
         * @brief 计算协商密钥
         *
         * 使用 SHA256(random1 || random2) 计算共享密钥。
         *
         * @param random1 发起方随机数
         * @param random2 响应方随机数
         * @return std::vector<uint8_t> 密钥（32字节）
         */
        static std::vector<uint8_t> computeKey(const std::vector<uint8_t> &random1,
                                               const std::vector<uint8_t> &random2);

    private:
        std::array<SessionBucket, NUM_BUCKETS> sessionBuckets; ///< 会话桶数组

        Monitor *monitor = nullptr; ///< 用于统计性能的监控模块

        UdpSenderFunc udpSender; ///< UDP 数据包发送器

        /**
         * @brief 哈希函数：根据策略 ID 获取对应桶索引
         * @param policy_id 策略 ID
         * @return size_t 桶索引
         */
        static size_t bucketIndex(uint32_t policy_id) {
            return policy_id % NUM_BUCKETS;
        }

        /**
         * @brief 构建协商数据包
         * @param type 协商包类型（RANDOM1、RANDOM2、CONFIRM）
         * @param policy_id 策略 ID
         * @param payloadData 可选负载数据（如随机数）
         * @return NegotiationPacket 构建好的包
         */
        static NegotiationPacket createPacket(PacketType type, uint32_t policy_id,
                                              const std::vector<uint8_t> &payloadData);

#ifdef UNIT_TEST
        friend class NegotiatorTest_FullNegotiationFlow_Test;
#endif
    };
} // namespace negotio

#endif // NEGOTIO_NEGOTIATE_H
