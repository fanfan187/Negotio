/**
 * @file monitor.cpp
 * @brief 监控模块实现
 */

#include "monitor.h"
#include <iostream>
#include <chrono>
#include <thread>

namespace negotio {
    Monitor::Monitor() : running(false), totalNegotiations(0), successfulNegotiations(0), totalLatencyMs(0) {
    }

    Monitor::~Monitor() {
        stop();
        if (logFile.is_open()) {
            logFile.close();
        }
    }

    void Monitor::start() {
        running = true;
        // 打开日志文件，但不立即写入统计数据
        logFile.open("monitor_log.txt", std::ios::app);
        monitorThread = std::thread(&Monitor::monitorLoop, this);
    }

    void Monitor::stop() {
        running = false;
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }

    void Monitor::recordNegotiation(const uint32_t durationMs, const bool success) {
        ++totalNegotiations;
        if (success) {
            ++successfulNegotiations;
            totalLatencyMs += durationMs;
        }
    }

    // 移除 const 限定符，以便修改 logFile
    void Monitor::monitorLoop() {
        using namespace std::chrono_literals;
        while (running) {
            std::this_thread::sleep_for(1s);
            uint32_t total = totalNegotiations.load();
            uint32_t success = successfulNegotiations.load();
            uint32_t latency = totalLatencyMs.load();
            double avgLatency = success > 0 ? static_cast<double>(latency) / success : 0;

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
            std::cout << "调试日志: 总协商数: " << total
                      << ", 成功协商数: " << success
                      << ", 平均延迟: " << avgLatency << " ms" << std::endl;
#endif
        }
    }
} // namespace negotio
