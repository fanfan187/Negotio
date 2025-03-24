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
        uint32_t policy_id; ///< 策略ID，用作会话标识
        NegotiateState state; ///< 当前协商状态
        std::vector<uint8_t> random1; ///< 发起方随机数 (32字节)
        std::vector<uint8_t> random2; ///< 响应方随机数 (32字节)
        std::vector<uint8_t> key; ///< 计算得到的共享密钥 (32字节)
        std::chrono::steady_clock::time_point startTime; ///< 协商开始时间
    };

    class Monitor;

    class Negotiator {
    public:
        Negotiator();

        ~Negotiator();

        void setMonitor(Monitor *m);

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
         * @param addr 发送方地址
         * @return ErrorCode
         */
        ErrorCode handlePacket(const NegotiationPacket &packet, const sockaddr_in &addr);

        /**
         * @brief 获取指定会话信息（只读）
         * @param policy_id 策略ID
         * @return 若存在返回会话，否则返回 std::nullopt
         */
        std::optional<NegotiationSession> getSession(uint32_t policy_id);

    private:
        std::unordered_map<uint32_t, NegotiationSession> sessions; ///< 会话表
        std::mutex sessionsMutex; ///< 会话表互斥锁

        Monitor *monitor;

        /**
         * @brief 生成指定长度的随机数据
         * @param size 随机数据字节数
         * @return 随机数据向量；失败返回空向量
         */
        std::vector<uint8_t> generateRandomData(size_t size);

        /**
         * @brief 计算共享密钥：SHA256(random1 || random2)
         * @param random1 第一个随机数
         * @param random2 第二个随机数
         * @return 计算得到的密钥（32字节）
         */
        std::vector<uint8_t> computeKey(const std::vector<uint8_t> &random1, const std::vector<uint8_t> &random2);
    };
} // namespace negotio

#endif // NEGOTIO_NEGOTIATE_H
