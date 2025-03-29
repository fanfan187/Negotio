/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

// tests/unit_test/udp_test.cpp

#include <gtest/gtest.h>
#include "../../src/udp/udp.h"
#include <netinet/in.h>
#include <thread>

using namespace negotio;

// 构造一个最小的数据包
NegotiationPacket makeTestPacket(uint32_t seq = 42) {
    NegotiationPacket packet{};
    packet.header.magic = MAGIC_NUMBER;
    packet.header.type = PacketType::RANDOM1;
    packet.header.sequence = seq;
    packet.header.timestamp = 12345678;
    packet.header.payload_len = 1;
    packet.payload = {0xDEADBEEF};
    return packet;
}

TEST(UdpSocketTest, InitSocket) {
    UdpSocket socket;
    EXPECT_EQ(socket.init(0), ErrorCode::SUCCESS);  // 绑定随机端口
    EXPECT_GT(socket.getSocketFd(), 0);
}

TEST(UdpSocketTest, SendAndReceivePacket) {
    UdpSocket sender;
    UdpSocket receiver;

    ASSERT_EQ(sender.init(0), ErrorCode::SUCCESS);   // 发送方绑定随机端口
    ASSERT_EQ(receiver.init(7777), ErrorCode::SUCCESS); // 接收方绑定固定端口

    sockaddr_in recvAddr{};
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(7777);
    recvAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1

    // 构造一个协商包
    NegotiationPacket outPacket = makeTestPacket(99);
    NegotiationPacket inPacket{};

    // 启动接收线程
    std::thread recvThread([&]() {
        sockaddr_in from{};
        ErrorCode result = receiver.recvPacket(inPacket, from, 1000);
        EXPECT_EQ(result, ErrorCode::SUCCESS);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 等待接收线程就绪

    // 发送数据包
    ErrorCode sendResult = sender.sendPacket(outPacket, recvAddr);
    EXPECT_EQ(sendResult, ErrorCode::SUCCESS);

    recvThread.join(); // 等待接收完成

    // 验证包是否一致
    EXPECT_EQ(inPacket.header.magic, outPacket.header.magic);
    EXPECT_EQ(inPacket.header.sequence, outPacket.header.sequence);
    EXPECT_EQ(inPacket.header.payload_len, outPacket.header.payload_len);
    ASSERT_EQ(inPacket.payload.size(), outPacket.payload.size());
    EXPECT_EQ(inPacket.payload[0], outPacket.payload[0]);
}

TEST(UdpSocketTest, ReceiveTimeout) {
    UdpSocket receiver;
    ASSERT_EQ(receiver.init(8888), ErrorCode::SUCCESS);

    NegotiationPacket packet{};
    sockaddr_in from{};
    ErrorCode result = receiver.recvPacket(packet, from, 100); // 设置短超时
    EXPECT_EQ(result, ErrorCode::TIMEOUT); // 应该超时
}
