/**
 * @file NegotioApplication.cpp
 * @brief 协商系统主程序入口
 *
 * 初始化 UDP 模块、Unix 套接字模块、策略管理器与协商器，加载 JSON 配置文件，
 * 启动线程接收策略命令与 UDP 数据包，完成策略动态添加与三包协商。
 *
 * 支持 JSON 命令格式：
 * {
 *   "action": "add",
 *   "policy": {
 *     "policy_id": 1,
 *     "remote_ip": "127.0.0.1",
 *     "remote_port": 12345,
 *     "timeout_ms": 100,
 *     "retry_times": 3
 *   }
 * }
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>
#include <sys/mman.h>
#include <pthread.h>
#include <sched.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "unixsocket/unixsocket.h"
#include "udp/udp.h"
#include "policy/policy.h"
#include "negotiate/negotiate.h"
#include "monitor/monitor.h"
#include "common.h"

#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std::chrono;

// 控制主循环是否继续运行
std::atomic running(true);

/// @brief 性能追踪块，用于输出函数耗时（us）
#define TRACE_BLOCK(name) TraceBlock __trace_block__(name)

/// @brief 性能追踪结构体
struct TraceBlock {
    std::string name;
    steady_clock::time_point start;

    explicit TraceBlock(const std::string &n) : name(n), start(steady_clock::now()) {
    }

    ~TraceBlock() {
        const auto end = steady_clock::now();
        const auto duration = std::chrono::duration_cast<microseconds>(end - start).count();
        std::cout << "[TRACE] " << name << " 耗时: " << duration << " us" << std::endl;
    }
};

/// @brief 信号处理函数，设置 running 为 false 用于优雅退出
void signalHandler(int signum) {
    running = false;
}

/// @brief 将当前线程绑定到指定 CPU 核
void setThreadAffinity(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    const pthread_t current_thread = pthread_self();
    pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

int main() {
    // 锁住当前和未来内存，防止交换，提高实时性
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        std::cerr << "mlockall 失败" << std::endl;
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 加载配置文件
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

    // 提取配置项
    uint16_t udpPort = config["network"]["udp_port"].get<uint16_t>();
    auto unixSocketPath = config["network"]["unix_socket_path"].get<std::string>();
    (void)config["negotiation"]["timeout_ms"].get<uint32_t>();
    // constexpr int epollTimeoutMs = 10;

    // 初始化 UDP 模块
    negotio::UdpSocket udpSocket;
    if (udpSocket.init(udpPort) != negotio::ErrorCode::SUCCESS) {
        std::cerr << "UDP 模块初始化失败" << std::endl;
        return 1;
    }

#ifdef DEBUG
    std::cout << "UDP 模块初始化成功，端口: " << udpPort << std::endl;
#endif

    // 初始化 Unix 套接字模块
    negotio::UnixSocketServer unixServer;
    if (!unixServer.init(unixSocketPath)) {
        std::cerr << "Unix Socket 模块初始化失败" << std::endl;
        return 1;
    }

#ifdef DEBUG
    std::cout << "Unix Socket 模块初始化成功，路径: " << unixSocketPath << std::endl;
#endif

    // 初始化策略管理器、协商器、监控器
    negotio::PolicyManager policyManager;
    negotio::Negotiator negotiator;
    negotio::Monitor monitor;
    negotiator.setMonitor(&monitor);
    monitor.start();

    // 设置 UDP 发送器（用于发送 CONFIRM）
    negotiator.setUdpSender([&udpSocket](const negotio::NegotiationPacket &pkt, const sockaddr_in &addr) {
        udpSocket.sendPacket(pkt, const_cast<sockaddr_in &>(addr));
    });

    /**
     * @brief 启动 Unix 套接字监听线程
     *
     * 接收命令并根据 JSON 结构触发策略添加及自动发起协商。
     */
    std::thread unixThread([&unixServer, &policyManager, &negotiator]() {
        setThreadAffinity(0);
        unixServer.setCommandHandler([&](const std::string &cmd) {
#ifdef DEBUG
            std::cout << "收到 Unix 命令: " << cmd << std::endl;
#endif
            try {
                auto j = json::parse(cmd);
                const std::string action = j["action"].get<std::string>();
                if (action == "add") {
                    auto policy_config = j["policy"].get<negotio::PolicyConfig>();
                    bool success = policyManager.addPolicy(policy_config);

#ifdef DEBUG
                    DEBUG_LOG("策略" << (success ? "添加成功" : "添加失败")
                        << "，策略ID: " << policy_config.policy_id);
#else
                    (void) success;
#endif
                    // 发起协商
                    sockaddr_in addr{};
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(policy_config.remote_port);
                    inet_pton(AF_INET, policy_config.remote_ip.c_str(), &addr.sin_addr);

                    negotiator.startNegotiation(policy_config.policy_id, addr);
                }
                // 可扩展其他 action
            } catch (const std::exception &e) {
                std::cerr << "命令解析错误: " << e.what() << std::endl;
            }
        });

        unixServer.run();
    });

    /**
     * @brief 启动 UDP 协商包接收线程
     *
     * 使用 epoll + 非阻塞 socket + detach 线程进行并发处理。
     */
    std::thread udpThread([&udpSocket]() {
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
