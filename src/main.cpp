#include <iostream>
#include <fstream>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>

// 引入各模块头文件
#include "unixsocket/unixsocket.h"
#include "udp/udp.h"
#include "policy/policy.h"
#include "negotiate/negotiate.h"
#include "monitor/monitor.h"

// 引入 nlohmann/json 库和 JSON 转换支持头文件
#include "nlohmann/json.hpp"
#include "json_support.h"
using json = nlohmann::json;

std::atomic<bool> running(true);

void signalHandler(int signum) {
    running = false;
}

int main() {
    // 注册信号处理（Ctrl+C 或 SIGTERM）
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 加载配置文件
    std::ifstream configFile("configs/config.json");
    if (!configFile) {
        std::cerr << "无法打开配置文件 configs/config.json" << std::endl;
        return -1;
    }
    json config;
    try {
        configFile >> config;
    } catch (const std::exception &e) {
        std::cerr << "解析配置文件失败: " << e.what() << std::endl;
        return -1;
    }

    // 从配置文件中读取参数（这里仅提取必要的配置项，后续可扩展）
    uint16_t udpPort = config["network"]["udp_port"].get<uint16_t>();
    std::string unixSocketPath = config["network"]["unix_socket_path"].get<std::string>();
    // 协商参数（例如超时时间、策略数量）可以根据需要读取
    uint32_t negotiationTimeoutMs = config["negotiation"]["timeout_ms"].get<uint32_t>();
    uint32_t maxStrategies = config["negotiation"]["max_strategies"].get<uint32_t>();

    // 初始化 UDP 模块
    negotio::UdpSocket udpSocket;
    if (udpSocket.init(udpPort) != negotio::ErrorCode::SUCCESS) {
        std::cerr << "UDP 模块初始化失败" << std::endl;
        return -1;
    }
    std::cout << "UDP 模块初始化成功，端口: " << udpPort << std::endl;

    // 初始化 Unix 域套接字模块
    negotio::UnixSocketServer unixServer;
    if (!unixServer.init(unixSocketPath)) {
        std::cerr << "Unix Socket 模块初始化失败" << std::endl;
        return -1;
    }
    std::cout << "Unix Socket 模块初始化成功，路径: " << unixSocketPath << std::endl;

    // 初始化其他模块
    negotio::PolicyManager policyManager;
    negotio::Negotiator negotiator;
    negotio::Monitor monitor;
    negotiator.setMonitor(&monitor);
    monitor.start();

    // 启动 Unix 域套接字服务线程
    std::thread unixThread([&unixServer, &policyManager, &negotiator]() {
        // 设置命令处理回调，解析 JSON 格式命令（例如添加、删除策略、刷新协商）
        unixServer.setCommandHandler([&](const std::string &cmd) {
            std::cout << "收到 Unix 命令: " << cmd << std::endl;
            // 示例：简单解析命令（这里假设命令格式为 {"action": "add", "policy": { ... }}）
            try {
                auto j = json::parse(cmd);
                std::string action = j["action"].get<std::string>();
                if (action == "add") {
                    negotio::PolicyConfig config = j["policy"].get<negotio::PolicyConfig>();
                    if (policyManager.addPolicy(config)) {
                        std::cout << "策略添加成功，策略ID: " << config.policy_id << std::endl;
                        // 可触发协商流程：例如 negotiator.startNegotiation(config.policy_id, peerAddr);
                    } else {
                        std::cout << "策略添加失败，策略ID: " << config.policy_id << std::endl;
                    }
                }
                // 可扩展其他命令，如 "remove", "refresh" 等
            } catch (const std::exception &e) {
                std::cerr << "命令解析错误: " << e.what() << std::endl;
            }
        });
        unixServer.run();
    });

    // 启动 UDP 数据包接收线程
    std::thread udpThread([&udpSocket, &negotiator, negotiationTimeoutMs]() {
        while (running) {
            negotio::NegotiationPacket packet;
            if (sockaddr_in srcAddr{}; udpSocket.recvPacket(packet, srcAddr, negotiationTimeoutMs) == negotio::ErrorCode::SUCCESS) {
                // 将接收到的数据包交由协商模块处理
                negotiator.handlePacket(packet, srcAddr);
            }
            // 短暂休眠以降低 CPU 占用
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });

    // 主线程：可加入其他业务逻辑，或等待退出信号
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 开始优雅关闭各模块
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
