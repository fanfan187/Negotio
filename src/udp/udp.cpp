/**
 * @file udp.cpp
 * @brief 实现 UDP 通信模块
 *
 * 提供基于非阻塞 UDP 套接字的发送与接收功能。
 * 使用线程局部缓冲区减少频繁内存分配，
 * 并结合 select 实现带超时机制的数据接收。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#include "udp.h"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <fcntl.h>
#include <sys/select.h>
#include <linux/udp.h>

namespace negotio {

    /**
     * @brief 构造函数
     *
     * 初始化 sockfd 为 -1。
     */
    UdpSocket::UdpSocket() : sockfd(-1) {
    }

    /**
     * @brief 析构函数
     *
     * 如果套接字已打开则关闭。
     */
    UdpSocket::~UdpSocket() {
        if (sockfd != -1) {
            close(sockfd);
        }
    }

    /**
     * @brief 初始化 UDP 套接字并绑定指定端口
     *
     * 包含以下配置步骤：
     * - 创建套接字
     * - 设置非阻塞模式
     * - 设置地址重用 SO_REUSEADDR
     * - 绑定本地端口
     *
     * @param port 本地绑定端口号
     * @return ErrorCode 初始化结果
     */
    ErrorCode UdpSocket::init(uint16_t port) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == -1) {
            return ErrorCode::SOCKET_ERROR;
        }

        // 设置非阻塞模式
        if (const int flags = fcntl(sockfd, F_GETFL, 0);
            flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            close(sockfd);
            return ErrorCode::SOCKET_ERROR;
        }

        // 设置地址重用
        constexpr int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            close(sockfd);
            return ErrorCode::SOCKET_ERROR;
        }

        // 绑定本地地址
        sockaddr_in localAddr = {};
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddr.sin_port = htons(port);

        if (bind(sockfd, reinterpret_cast<sockaddr *>(&localAddr), sizeof(localAddr)) < 0) {
            close(sockfd);
            return ErrorCode::SOCKET_ERROR;
        }

        return ErrorCode::SUCCESS;
    }

    /**
     * @brief 发送数据包
     *
     * 使用线程局部缓冲区避免频繁分配，并持有发送互斥锁保证线程安全。
     *
     * @param packet 协议数据包
     * @param addr   对方地址
     * @return ErrorCode 发送结果
     */
    ErrorCode UdpSocket::sendPacket(const NegotiationPacket &packet, sockaddr_in &addr) {
        std::lock_guard lock(sendMutex);

        // 使用线程局部缓冲区，避免重复分配
        static thread_local std::vector<uint8_t> buffer;
        buffer.clear();

        if (const ssize_t bytes = serializePacket(packet, buffer); bytes == -1) {
            return ErrorCode::INVALID_PARAM;
        }

        // 发送数据
        if (const ssize_t sent = sendto(sockfd, buffer.data(), buffer.size(), 0,
                                        reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)); sent < 0) {
            return ErrorCode::SOCKET_ERROR;
        }

        return ErrorCode::SUCCESS;
    }

    /**
     * @brief 接收数据包（带超时）
     *
     * 使用 select 监听 sockfd 是否可读，若在指定时间内无数据则返回 TIMEOUT。
     * 成功接收后进行反序列化。
     *
     * @param packet 输出参数，用于存储接收后的数据包
     * @param addr   输出参数，存储发送方地址
     * @param timeout_ms 超时时间（毫秒）
     * @return ErrorCode 接收结果
     */
    ErrorCode UdpSocket::recvPacket(NegotiationPacket &packet, sockaddr_in &addr, int timeout_ms) const {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        timeval tv{};
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        if (const int ret = select(sockfd + 1, &readfds, nullptr, nullptr, &tv); ret < 0) {
            return ErrorCode::SOCKET_ERROR;
        } else if (ret == 0) {
            return ErrorCode::TIMEOUT;
        }

        std::vector<uint8_t> buffer(4096);
        socklen_t addrLen = sizeof(addr);
        const ssize_t received = recvfrom(sockfd, buffer.data(), buffer.size(), 0,
                                          reinterpret_cast<struct sockaddr *>(&addr), &addrLen);
        if (received == -1) {
            return ErrorCode::SOCKET_ERROR;
        }

        buffer.resize(received);
        if (const ssize_t deserialize = deserializePacket(buffer, packet); deserialize < 0) {
            return ErrorCode::INVALID_PARAM;
        }

        return ErrorCode::SUCCESS;
    }

    /**
     * @brief 将协议数据包序列化为二进制缓冲区
     *
     * 序列化格式：PacketHeader + Payload（二进制）
     *
     * @param packet 协议数据包
     * @param buffer 输出缓冲区
     * @return ssize_t 序列化后的字节数
     */
    ssize_t UdpSocket::serializePacket(const NegotiationPacket &packet, std::vector<uint8_t> &buffer) {
        constexpr size_t headerSize = sizeof(PacketHeader);
        const size_t payloadSize = packet.payload.size() * sizeof(uint32_t);
        const size_t totalSize = headerSize + payloadSize;

        buffer.resize(totalSize);

        // 拷贝头部
        std::memcpy(buffer.data(), &packet.header, headerSize);

        // 拷贝负载
        if (payloadSize > 0) {
            std::memcpy(buffer.data() + headerSize, packet.payload.data(), payloadSize);
        }

        return static_cast<ssize_t>(totalSize);
    }

    /**
     * @brief 从缓冲区反序列化为协议数据包
     *
     * 校验 buffer 大小是否合理，并提取 header 和 payload。
     *
     * @param buffer 输入数据缓冲区
     * @param packet 输出协议数据包
     * @return ssize_t 实际字节数（失败返回 -1）
     */
    ssize_t UdpSocket::deserializePacket(const std::vector<uint8_t> &buffer, NegotiationPacket &packet) {
        constexpr size_t headerSize = sizeof(PacketHeader);

        // 检查 buffer 是否足够大
        if (buffer.size() < headerSize) {
            return -1;
        }

        std::memcpy(&packet.header, buffer.data(), headerSize);

        size_t payloadSize = buffer.size() - headerSize;
        if (payloadSize % sizeof(uint32_t) != 0) {
            return -1;
        }

        const size_t payloadCount = payloadSize / sizeof(uint32_t);
        packet.payload.resize(payloadCount);

        if (payloadCount > 0) {
            std::memcpy(packet.payload.data(), buffer.data() + headerSize, payloadSize);
        }

        return static_cast<ssize_t>(buffer.size());
    }

} // namespace negotio
