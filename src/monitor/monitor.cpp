/**
 * @file monitor.cpp
 * @brief 监控模块实现
 *
 * @author fanfan187
 * @version 1.0.0
 * @since 1.0.0
 */

#include "monitor.h"
#include <iostream>
#include <chrono>
#include <thread>

namespace negotio {
    /**
     * @brief 构造函数
     *
     * 初始化监控模块的状态以及统计变量：
     * - running：监控线程运行状态
     * - totalNegotiations：总协商次数
     * - successfulNegotiations：成功协商次数
     * - totalLatencyMs：成功协商总延迟（单位：毫秒）
     */
    Monitor::Monitor()
        : running(false), totalNegotiations(0), successfulNegotiations(0), totalLatencyMs(0) {
    }

    /**
     * @brief 析构函数
     *
     * 停止监控线程并关闭日志文件（如果已经打开）。
     */
    Monitor::~Monitor() {
        stop();
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    /**
     * @brief 启动监控模块
     *
     * 设置监控状态为运行，打开日志文件（追加模式），
     * 并启动一个后台线程来周期性统计协商数据。
     */
    void Monitor::start() {
        running = true;
        // 打开日志文件，采用追加模式
        logFile.open("monitor_log.txt", std::ios::app);
        monitorThread = std::thread(&Monitor::monitorLoop, this);
    }

    /**
     * @brief 停止监控模块
     *
     * 将监控状态设置为停止，并等待监控线程安全退出。
     */
    void Monitor::stop() {
        running = false;
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }

    /**
     * @brief 记录一次协商操作
     *
     * 累加一次协商记录，若协商成功则更新成功计数并累计延迟。
     *
     * @param durationMs 协商耗时，单位为毫秒
     * @param success 协商是否成功的标志，true 表示成功
     */
    void Monitor::recordNegotiation(const uint32_t durationMs, const bool success) {
        ++totalNegotiations;
        if (success) {
            ++successfulNegotiations;
            totalLatencyMs += durationMs;
        }
    }

    /**
     * @brief 监控线程主循环
     *
     * 循环每隔1秒钟统计一次协商数据：
     * - 读取总协商次数、成功协商次数以及累计延迟
     * - 计算平均延迟（仅在有成功协商时）
     * - 将统计数据写入日志文件，并在 DEBUG 模式下输出到标准输出
     *
     * 此函数在后台线程中运行，直到 running 标志被置为 false。
     */
    void Monitor::monitorLoop() {
        using namespace std::chrono_literals;
        while (running) {
            // 每隔1秒钟执行一次统计
            std::this_thread::sleep_for(1s);

            // 获取当前统计数据
            const uint32_t total = totalNegotiations.load();
            const uint32_t success = successfulNegotiations.load();
            const uint32_t latency = totalLatencyMs.load();

            // 计算平均延迟，若无成功协商则为0
            const double avgLatency = success > 0 ? static_cast<double>(latency) / success : 0;

            // 将统计信息写入日志文件
            if (logFile.is_open()) {
                if (success > 0) {
                    logFile << "监控统计: 总协商数: " << total
                            << ", 成功协商数: " << success
                            << ", 平均延迟: " << avgLatency << " ms" << std::endl;
                } else {
                    logFile << "监控统计: 总协商数: " << total
                            << ", 尚无成功协商数据" << std::endl;
                }
                logFile.flush();
            }

#ifdef DEBUG
            // 如果编译时定义了 DEBUG，输出调试日志到标准输出
            std::cout << "调试日志: 总协商数: " << total
                    << ", 成功协商数: " << success
                    << ", 平均延迟: " << avgLatency << " ms" << std::endl;
#endif
        }
    }
} // namespace negotio
