/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
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
    }

    void Monitor::start() {
        running = true;
        monitorThread = std::thread(&Monitor::monitorLoop, this);
    }

    void Monitor::stop() {
        running = false;
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }

    void Monitor::recordNegotiation(uint32_t durationMs, bool success) {
        totalNegotiations++;
        if (success) {
            successfulNegotiations++;
            totalLatencyMs += durationMs;
        }
    }

    void Monitor::monitorLoop() {
        using namespace std::chrono_literals;
        while (running) {
            std::this_thread::sleep_for(1s);
            uint32_t total = totalNegotiations.load();
            uint32_t success = successfulNegotiations.load();
            uint32_t latency = totalLatencyMs.load();
            double avgLatency = success > 0 ? static_cast<double>(latency) / success : 0;
            std::cout << "监控统计: 总协商数: " << total
                    << ", 成功协商数: " << success
                    << ", 平均延迟: " << avgLatency << " ms"
                    << std::endl;
        }
    }

} // namespace negotio



