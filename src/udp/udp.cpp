/**
 * 实现 udp 模块
 *
 * 采用线程局部缓冲区复用机制减少内存频繁分配，
 * 同时保持非阻塞模式及 select 超时处理，确保低延迟。
 *
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
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
    UdpSocket::UdpSocket() : sockfd(-1) {
    }

    UdpSocket::~UdpSocket() {
        if (sockfd != -1) {
            close(sockfd);
        }
    }

    ErrorCode UdpSocket::init(uint16_t port) {
        sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == -1) {
            return ErrorCode::SOCKET_ERROR;
        }

        // 设置套接字为非阻塞模式
        if (const int flags = fcntl(sockfd, F_GETFL, 0);
            flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            close(sockfd);
            return ErrorCode::SOCKET_ERROR;
        }

        // 设置 SO_REUSEADDR 选项
        constexpr int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            close(sockfd);
            return ErrorCode::SOCKET_ERROR;
        }

        // 绑定地址
        sockaddr_in localAddr = {};
        localAddr.sin_family = AF_INET;
        localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        localAddr.sin_port = htons(port);

        if (bind(sockfd, reinterpret_cast<sockaddr *>(&localAddr), sizeof(localAddr)) < 0) {
            close(sockfd);
            return ErrorCode::SOCKET_ERROR;
        }
        //
        // // 增加发送和增加缓冲区大小
        // int sendBufSize = 1024 * 1024;  // 1MB buffer
        // int recvBufSize = 1024 * 1024;
        // setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &sendBufSize, sizeof(sendBufSize));
        // setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &recvBufSize, sizeof(recvBufSize));
        //
        // // 开启 UDP_CORK 选项减小包发送
        // int cork = 1;
        // setsockopt(sockfd, IPPROTO_UDP, UDP_CORK, &cork, sizeof(cork));

        return ErrorCode::SUCCESS;
    }

    ErrorCode UdpSocket::sendPacket(const NegotiationPacket &packet, sockaddr_in &addr) {
        std::lock_guard lock(sendMutex);
        // 使用线程局部的缓冲区,避免频繁分配
        static thread_local std::vector<uint8_t> buffer;
        buffer.clear();
        if (const ssize_t bytes = serializePacket(packet, buffer); bytes == -1) {
            return ErrorCode::INVALID_PARAM;
        }
        if (const ssize_t sent = sendto(sockfd, buffer.data(), buffer.size(), 0,
                                        reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)); sent < 0) {
            return ErrorCode::SOCKET_ERROR;
        }
        return ErrorCode::SUCCESS;
    }

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

    ssize_t UdpSocket::serializePacket(const NegotiationPacket &packet, std::vector<uint8_t> &buffer) {
        // 序列化格式: PacketHeader 固定大小 + payload 长度 * sizeof(uint32_t)
        constexpr size_t headerSize = sizeof(PacketHeader);
        const size_t payloadSize = packet.payload.size() * sizeof(uint32_t);
        const size_t totalSize = headerSize + payloadSize;

        buffer.resize(totalSize);
        // 将 header 拷贝到 buffer 前部分
        std::memcpy(buffer.data(), &packet.header, headerSize);

        // 将 payload 拷贝到 buffer 后部分（若存在）
        if (payloadSize > 0) {
            std::memcpy(buffer.data() + headerSize, packet.payload.data(), payloadSize);
        }

        return static_cast<ssize_t>(totalSize);
    }

    ssize_t UdpSocket::deserializePacket(const std::vector<uint8_t> &buffer, NegotiationPacket &packet) {
        constexpr size_t headerSize = sizeof(PacketHeader);
        // 检查 buffer 长度是否满足 PacketHeader 的大小
        if (buffer.size() < headerSize) {
            return -1;
        }
        // 从 buffer 中读取 PacketHeader
        std::memcpy(&packet.header, buffer.data(), headerSize);

        // 检查 payload 长度是否满足 buffer 长度减去 PacketHeader 的大小
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
