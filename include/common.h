/**
 * @file common.h
 * @brief 共享数据库结构和常量定义
 * @details 包含所有模块共享的数据包模式、状态码、错误码等定义
 *
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

#ifndef NEGOTIO_COMMON_H
#define NEGOTIO_COMMON_H

#include <cstdint>
#include <string>
#include <vector>

// 调试开关: 0:关闭调试输出,1:开启
#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG 0
#endif

#if ENABLE_DEBUG
#include <iostream>
#define DEBUG_LOG(x) std::cout << x << std::endl;
#else
#define DEBUG_LOG(x) do {} while (0)
#endif

namespace negotio {
    // 错误码定义
    enum class ErrorCode {
        SUCCESS = 0, // 成功
        TIMEOUT = 1, // 超时
        INVALID_PARAM = 2, // 无效参数
        NEGOTIATION_FAILED = 3, // 协商失败
        MEMORY_ERROR = 4, // 内存错误
        SOCKET_ERROR = 5, // 套接字错误
    };

    // 协商状态定义
    enum class NegotiationStatus {
        INIT = 0, // 初始化
        WAITING_FOR_RESPONSE = 1, // 等待响应
        NEGOTIATION_SUCCESS = 2, // 协商成功
        NEGOTIATION_FAILED = 3, // 协商失败
    };

    // 数据包类型定义
    enum class PacketType {
        RANDOM1 = 1, // 随机数1
        RANDOM2 = 2, // 随机数2
        CONFIRM = 3, // 确认
    };

    // 数据包头部结构
#pragma pack(push, 1)
    struct PacketHeader {
        uint32_t magic; // 魔数,用于包识别
        PacketType type; // 数据包类型
        uint32_t sequence; // 包序号
        uint32_t timestamp; // 时间戳
        uint32_t payload_len; // 负载长度
    };
#pragma pack(pop)

    // 协商数据包结构
    struct NegotiationPacket {
        PacketHeader header;
        std::vector<uint32_t> payload;
    };

    // 策略配置结构
    struct PolicyConfig {
        uint32_t policy_id; // 策略ID
        std::string remote_ip; // 远程IP
        uint16_t remote_port; // 远程端口
        uint32_t timeout_ms; // 超时时间
        uint32_t retry_times; // 重试次数
    };

    // 常量定义
    constexpr uint32_t MAGIC_NUMBER = 0xE45474F; // 'NEDO'
    constexpr uint32_t MAX_POLICY_SIZE = 1024; // 最大负载大小
    constexpr uint32_t MAX_POLICY_COUNT = 4096; // 最大策略数量
    constexpr uint32_t DEFAULT_TIMEOUT_MS = 1000; // 默认超时时间
    constexpr uint32_t DEFAULT_RETRY_TIMES = 3; // 默认重试次数
    constexpr uint32_t RANDOM_NUMBER = 32; // 随机数大小(字节)
    constexpr uint32_t KEY_SIZE = 32; // 密钥大小(字节)

    // 错误处理函数
    std::string GetErrorMessage(ErrorCode code);
}


#endif      //NEGOTIO_COMMON_H








