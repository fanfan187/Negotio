/**
 * 实现 UDP 模块接口
 * 用于协商数据包的发送和接收
 *
 * 协商数据包定义在 common.h 中（或相应头文件中）
 *
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
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

    class UdpSocket {
    public:
        UdpSocket();

        ~UdpSocket();

        // 移动构造函数
        UdpSocket(UdpSocket&& other) noexcept : sockfd(other.sockfd) {
            other.sockfd = -1;
        };
        // 移动赋值函数
        UdpSocket& operator=(UdpSocket&& other) noexcept {
            if (this != &other) {
                close(sockfd);
                sockfd = other.sockfd;
                other.sockfd = -1;
            }
            return *this;
        }

        /**
         * @brief 初始化 UDP 套接字, 绑定到指定端口
         * @param port 绑定端口号
         * @return 成功返回 ErrorCode::SUCCESS, 否则返回相应错误代码
         */
        ErrorCode init(uint16_t port);

        /**
         * @brief 发送数据包到指定地址
         * @param packet 协商数据包
         * @param addr 对端地址结构
         * @return 成功返回 ErrorCode::SUCCESS, 否则返回相应错误代码
         */
        ErrorCode sendPacket(const NegotiationPacket &packet, sockaddr_in &addr);

        /**
         * @brief 接收数据包
         * @param packet 输出参数，接到的数据包
         * @param addr 输出参数，发送方地址
         * @param timeout_ms 超时时间，默认 1000 毫秒
         * @return 成功返回 ErrorCode::SUCCESS, 否则返回相应错误代码
         */
        ErrorCode recvPacket(NegotiationPacket &packet, sockaddr_in &addr, int timeout_ms = 1000) const;

        /**
         * @brief 获取套接字文件描述符
         * @return 套接字文件描述符
         */
        [[nodiscard]] int getSocketFd() const { return sockfd; }

    private:
        int sockfd;
        std::mutex sendMutex;

        /**
         * @brief 将 NegotiationPacket 序列化到缓冲区
         * @param packet 协商数据包
         * @param buffer 输出缓冲区
         * @return 序列化后的字节数，失败返回负值
         */
        static ssize_t serializePacket(const NegotiationPacket &packet, std::vector<uint8_t> &buffer);

        /**
         * @brief 将缓冲区中的数据反序列化到 NegotiationPacket
         * @param buffer 输入缓冲区
         * @param packet 输出参数，反序列化后的协商数据包
         * @return 字节数，失败返回负值
         */
        static ssize_t deserializePacket(const std::vector<uint8_t> &buffer, NegotiationPacket &packet);
    };
}
