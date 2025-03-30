/**
 * @file udp.h
 * @brief UDP 模块接口定义
 *
 * 提供基于非阻塞 UDP 套接字的发送与接收接口，
 * 用于协商模块中传输 NegotiationPacket 数据结构。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <unistd.h>
#include <vector>
#include "common.h"

struct sockaddr_in;

namespace negotio {

    struct NegotiationPacket;

    /**
     * @brief UDP 套接字通信类
     *
     * 提供初始化、数据包发送、接收和序列化工具函数，支持线程安全的发送。
     */
    class UdpSocket {
    public:
        /**
         * @brief 构造函数
         *
         * 初始化套接字描述符。
         */
        UdpSocket();

        /**
         * @brief 析构函数
         *
         * 自动关闭套接字。
         */
        ~UdpSocket();

        /**
         * @brief 初始化 UDP 套接字并绑定本地端口
         *
         * 设置非阻塞、地址重用等选项。
         *
         * @param port 本地绑定端口
         * @return ErrorCode::SUCCESS 成功，其他为失败原因
         */
        ErrorCode init(uint16_t port);

        /**
         * @brief 发送协商数据包
         *
         * 使用内部互斥锁保护线程安全。
         *
         * @param packet 要发送的协商数据包
         * @param addr   对端地址
         * @return 发送结果错误码
         */
        ErrorCode sendPacket(const NegotiationPacket& packet, sockaddr_in& addr);

        /**
         * @brief 接收协商数据包（带超时）
         *
         * 使用 select 监听并阻塞至超时或数据到达。
         *
         * @param packet 输出参数，接收到的数据包
         * @param addr   输出参数，发送方地址
         * @param timeout_ms 超时时间（毫秒），默认值为 10ms
         * @return 接收结果错误码
         */
        ErrorCode recvPacket(NegotiationPacket& packet, sockaddr_in& addr, int timeout_ms = 10) const;

        /**
         * @brief 获取内部套接字文件描述符
         * @return 套接字文件描述符
         */
        [[nodiscard]] int getSocketFd() const { return sockfd; }

    private:
        int sockfd;               ///< 套接字文件描述符
        std::mutex sendMutex;     ///< 发送锁，用于保护多线程发送操作

        /**
         * @brief 将数据包序列化为二进制缓冲区
         *
         * 格式为：PacketHeader + payload。
         *
         * @param packet 要序列化的数据包
         * @param buffer 输出缓冲区
         * @return 正常返回字节数，失败返回负数
         */
        static ssize_t serializePacket(const NegotiationPacket& packet, std::vector<uint8_t>& buffer);

        /**
         * @brief 从二进制缓冲区反序列化数据包
         *
         * 校验缓冲区长度后提取 header 与 payload。
         *
         * @param buffer 输入缓冲区
         * @param packet 输出数据包结构
         * @return 成功返回字节数，失败返回负数
         */
        static ssize_t deserializePacket(const std::vector<uint8_t>& buffer, NegotiationPacket& packet);
    };

} // namespace negotio
