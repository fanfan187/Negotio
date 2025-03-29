#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>
#include <cstdlib>
#include <sys/mman.h>
#include <pthread.h>
#include <sched.h>

#include "unixsocket/unixsocket.h"
#include "udp/udp.h"
#include "policy/policy.h"
#include "negotiate/negotiate.h"
#include "monitor/monitor.h"

#include "nlohmann/json.hpp"
#include <sys/epoll.h>
#include <cerrno>
#include <arpa/inet.h>

#include "json_support.h"
#include "common.h"

using json = nlohmann::json;
using namespace std::chrono;

std::atomic<bool> running(true);

#define TRACE_BLOCK(name) TraceBlock __trace_block__(name)

struct TraceBlock {
    std::string name;
    std::chrono::steady_clock::time_point start;

    explicit TraceBlock(const std::string &n) : name(n), start(std::chrono::steady_clock::now()) {
    }

    ~TraceBlock() {
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "[TRACE] " << name << " 耗时: " << duration << " us" << std::endl;
    }
};

void signalHandler(int signum) {
    running = false;
}

void setThreadAffinity(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

int main() {
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        std::cerr << "mlockall 失败" << std::endl;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    std::ifstream configFile("configs/config.json");
    if (!configFile) {
        std::cerr << "无法打开配置文件 configs/config.json" << std::endl;
        return 1;
    }
    json config;
    try {
        configFile >> config;
    } catch (const std::exception &e) {
        std::cerr << "解析配置文件失败: " << e.what() << std::endl;
        return 1;
    }

    uint16_t udpPort = config["network"]["udp_port"].get<uint16_t>();
    std::string unixSocketPath = config["network"]["unix_socket_path"].get<std::string>();
    uint32_t negotiationTimeoutMs = config["negotiation"]["timeout_ms"].get<uint32_t>();
    const int epollTimeoutMs = 10;

    negotio::UdpSocket udpSocket;
    if (udpSocket.init(udpPort) != negotio::ErrorCode::SUCCESS) {
        std::cerr << "UDP 模块初始化失败" << std::endl;
        return 1;
    }

#ifdef DEBUG
    std::cout << "UDP 模块初始化成功，端口: " << udpPort << std::endl;
#endif

    negotio::UnixSocketServer unixServer;
    if (!unixServer.init(unixSocketPath)) {
        std::cerr << "Unix Socket 模块初始化失败" << std::endl;
        return 1;
    }

#ifdef DEBUG
    std::cout << "Unix Socket 模块初始化成功，路径: " << unixSocketPath << std::endl;
#endif

    negotio::PolicyManager policyManager;
    negotio::Negotiator negotiator;
    negotio::Monitor monitor;
    negotiator.setMonitor(&monitor);
    monitor.start();

    // 设置 UDP 发送器，便于 Negotiator 内部发送 CONFIRM 包
    negotiator.setUdpSender([&udpSocket](const negotio::NegotiationPacket &pkt, const sockaddr_in &addr) {
        udpSocket.sendPacket(pkt, const_cast<sockaddr_in &>(addr));
    });

    // 启动 Unix 域套接字服务线程
    std::thread unixThread([&unixServer, &policyManager, &negotiator]() {
        setThreadAffinity(0);
        unixServer.setCommandHandler([&](const std::string &cmd) {
#ifdef DEBUG
            std::cout << "收到 Unix 命令: " << cmd << std::endl;
#endif
            try {
                auto j = json::parse(cmd);
                std::string action = j["action"].get<std::string>();
                if (action == "add") {
                    auto policy_config = j["policy"].get<negotio::PolicyConfig>();
                    bool success = policyManager.addPolicy(policy_config);
#ifdef DEBUG
                    DEBUG_LOG("策略" << (success ? "添加成功" : "添加失败")
                        << "，策略ID: " << policy_config.policy_id);
#else
                    (void) success; // 引用 success 以避免未使用警告
#endif
                    // 立即发起协商
                    sockaddr_in addr{};
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(policy_config.remote_port);
                    inet_pton(AF_INET, policy_config.remote_ip.c_str(), &addr.sin_addr);

                    negotiator.startNegotiation(policy_config.policy_id, addr);
                }
                // 可添加其它命令处理
            } catch (const std::exception &e) {
                std::cerr << "命令解析错误: " << e.what() << std::endl;
            }
        });

        unixServer.run();
    });

    constexpr int recvTimeoutMs = 0;

    // 启动 UDP 数据包接收线程
    std::thread udpThread([&udpSocket, &negotiator, recvTimeoutMs, epollTimeoutMs]() {
        TRACE_BLOCK("udpThread total");
        setThreadAffinity(1);
        int epollFd = epoll_create1(0);
        if (epollFd == -1) {
            std::cerr << "UDP epoll_create1 失败" << std::endl;
            return;
        }
        struct epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = udpSocket.getSocketFd();
        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, udpSocket.getSocketFd(), &ev) == -1) {
            std::cerr << "UDP epoll_ctl 添加失败" << std::endl;
            close(epollFd);
            return;
        }
        while (running) {
            constexpr int MAX_EVENTS = 10;
            struct epoll_event events[MAX_EVENTS];
            int nfds = epoll_wait(epollFd, events, MAX_EVENTS, epollTimeoutMs);
            if (nfds < 0) {
                if (errno == EINTR)
                    continue;
                std::cerr << "UDP epoll_wait 失败" << std::endl;
                break;
            }
            for (int i = 0; i < nfds; ++i) {
                sockaddr_in srcAddr{};
                negotio::NegotiationPacket packet;
                if (udpSocket.recvPacket(packet, srcAddr, recvTimeoutMs) ==
                    negotio::ErrorCode::SUCCESS) {
#ifdef DEBUG
                    std::cout << "收到 UDP 数据包，策略ID: " << packet.header.sequence << std::endl;
#endif
                    // ✅ 加 TRACE_BLOCK 观察 handlePacket 是否耗时
                    std::thread([packet, srcAddr, &negotiator]() {
                        TRACE_BLOCK("recvPacket+handlePacket");
                        negotiator.handlePacket(packet, srcAddr);
                    }).detach();
                }
            }
        }
        close(epollFd);
    });

    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "正在停止服务..." << std::endl;
    unixServer.stop();
    monitor.stop();
    if (udpThread.joinable()) {
        udpThread.join();
    }
    if (unixThread.joinable()) {
        unixThread.join();
    }
    std::cout << "服务已停止." << std::endl;
    return 0;
}
