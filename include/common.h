/**
 * @file common.h
 * @brief 共享数据结构和常量定义
 * @details 定义所有模块共享的通用类型，包括数据包格式、状态码、错误码、策略配置等。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#ifndef NEGOTIO_COMMON_H
#define NEGOTIO_COMMON_H

#include <cstdint>
#include <string>
#include <vector>

/// 调试输出开关：0 为关闭，1 为开启
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

    /**
     * @brief 错误码枚举
     *
     * 表示函数或模块的运行结果状态。
     */
    enum class ErrorCode {
        SUCCESS = 0,              ///< 成功
        TIMEOUT = 1,              ///< 操作超时
        INVALID_PARAM = 2,        ///< 参数无效
        NEGOTIATION_FAILED = 3,   ///< 协商失败
        MEMORY_ERROR = 4,         ///< 内存分配失败
        SOCKET_ERROR = 5          ///< 套接字操作失败
    };

    /**
     * @brief 协商流程状态
     *
     * 用于记录协商过程中的业务状态。
     */
    enum class NegotiationStatus {
        INIT = 0,                   ///< 初始化
        WAITING_FOR_RESPONSE = 1,  ///< 等待响应
        NEGOTIATION_SUCCESS = 2,   ///< 协商成功
        NEGOTIATION_FAILED = 3     ///< 协商失败
    };

    /**
     * @brief 协商数据包类型
     *
     * 用于标识协商阶段的数据包类型。
     */
    enum class PacketType {
        RANDOM1 = 1,  ///< 发起方发送的随机数包
        RANDOM2 = 2,  ///< 响应方生成的随机数包
        CONFIRM = 3   ///< 发起方发回的确认包
    };

    /**
     * @brief 数据包头部结构
     *
     * 用于识别包类型与基本信息。
     */
#pragma pack(push, 1)
    struct PacketHeader {
        uint32_t magic;           ///< 魔数，用于验证数据包合法性
        PacketType type;          ///< 数据包类型
        uint32_t sequence;        ///< 策略 ID 或包序列号
        uint32_t timestamp;       ///< 时间戳（毫秒）
        uint32_t payload_len;     ///< 负载长度（以 uint32_t 为单位）
    };
#pragma pack(pop)

    /**
     * @brief 协商数据包结构
     *
     * 包括头部与 uint32_t 数组形式的负载。
     */
    struct NegotiationPacket {
        PacketHeader header;              ///< 包头
        std::vector<uint32_t> payload;    ///< 包体负载数据
    };

    /**
     * @brief 策略配置结构
     *
     * 定义协商时使用的目标信息和控制参数。
     */
    struct PolicyConfig {
        uint32_t policy_id;      ///< 策略 ID
        std::string remote_ip;   ///< 对端 IP 地址（字符串形式）
        uint16_t remote_port;    ///< 对端端口
        uint32_t timeout_ms;     ///< 超时时间（毫秒）
        uint32_t retry_times;    ///< 最大重试次数
    };

    //=============================
    // 常量定义
    //=============================

    constexpr uint32_t MAGIC_NUMBER        = 0xE45474F;  ///< 固定魔数标识，用于验证数据包
    constexpr uint32_t MAX_POLICY_SIZE     = 1024;       ///< 最大负载大小（单位：uint32_t）
    constexpr uint32_t MAX_POLICY_COUNT    = 4096;       ///< 最大支持的策略条目数
    constexpr uint32_t DEFAULT_TIMEOUT_MS  = 100;        ///< 默认超时时间（单位：毫秒）
    constexpr uint32_t DEFAULT_RETRY_TIMES = 3;          ///< 默认重试次数
    constexpr uint32_t RANDOM_NUMBER       = 32;         ///< 随机数大小（字节）
    constexpr uint32_t KEY_SIZE            = 32;         ///< 协商密钥大小（字节）

    /**
     * @brief 获取错误码对应的文本描述
     *
     * @param code 错误码
     * @return 对应错误码的字符串信息
     */
    std::string GetErrorMessage(ErrorCode code);

} // namespace negotio

#endif // NEGOTIO_COMMON_H
