/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

#pragma once
#ifndef NEGOTIO_MONITOR_H
#define NEGOTIO_MONITOR_H

#ifdef Monitor
#undef Monitor
#endif

#include <atomic>
#include <thread>

namespace negotio {

    class Monitor {
    public:
        Monitor();
        ~Monitor();

        /**
         * @brief 启动监控线程
         */
        void start();

        /**
         * @brief 停止监控线程
         */
        void stop();

        /**
         * @brief 记录一次协商完成事件，传入协商耗时（毫秒）及是否成功
         * @param durationMs 协商耗时（毫秒）
         * @param success 成功为 true，否则 false
         */
        void recordNegotiation(uint32_t durationMs, bool success);

    private:
        std::atomic<bool> running;
        std::thread monitorThread;
        std::atomic<uint32_t> totalNegotiations;
        std::atomic<uint32_t> successfulNegotiations;
        std::atomic<uint32_t> totalLatencyMs; // 累计延迟（毫秒）

        void monitorLoop();
    };

} // namespace negotio

#endif // NEGOTIO_MONITOR_H
