/**
 * @file monitor.h
 * @brief 监控模块接口声明，提供协商操作监控与统计功能
 *
 * 本头文件定义了 negotio::Monitor 类，用于记录和监控协商操作的统计信息，
 * 包括协商总次数、成功次数以及累计延迟，并通过后台线程定时写入日志文件。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#pragma once
#ifndef NEGOTIO_MONITOR_H
#define NEGOTIO_MONITOR_H

// 取消可能已定义的宏 Monitor，以避免名称冲突
#ifdef Monitor
#undef Monitor
#endif

#include <atomic>
#include <thread>
#include <fstream>

namespace negotio {

    /**
     * @brief 协商监控类
     *
     * 该类负责记录协商操作的统计数据，并在后台线程中周期性地将数据写入日志文件。
     * 它主要用于监控协商过程中的性能指标，如成功率和平均延迟。
     */
    class Monitor {
    public:
        /**
         * @brief 构造函数
         *
         * 初始化统计数据和运行状态。
         */
        Monitor();

        /**
         * @brief 析构函数
         *
         * 在析构时停止监控线程，并确保日志文件正确关闭。
         */
        ~Monitor();

        /**
         * @brief 启动监控线程
         *
         * 设置监控状态为运行，打开日志文件（追加模式），并启动后台线程记录统计数据。
         */
        void start();

        /**
         * @brief 停止监控线程
         *
         * 将监控状态置为停止，并等待后台监控线程安全退出。
         */
        void stop();

        /**
         * @brief 记录一次协商完成事件
         *
         * 根据传入的协商耗时和成功标识，更新统计数据。
         *
         * @param durationMs 协商耗时（单位：毫秒）
         * @param success    协商是否成功，成功为 true，否则为 false
         */
        void recordNegotiation(uint32_t durationMs, bool success);

        // 日志文件对象，用于写入监控统计数据
        std::ofstream logFile;

    private:
        // 标识监控线程是否正在运行
        std::atomic<bool> running;

        // 监控线程对象
        std::thread monitorThread;

        // 协商总次数
        std::atomic<uint32_t> totalNegotiations;

        // 成功协商次数
        std::atomic<uint32_t> successfulNegotiations;

        // 成功协商累计延迟（单位：毫秒）
        std::atomic<uint32_t> totalLatencyMs;

        /**
         * @brief 监控线程主循环
         *
         * 后台线程每隔一段时间统计一次协商数据，并将结果写入日志文件。
         * 此函数在 start() 启动监控线程时调用，直到调用 stop() 后退出循环。
         */
        void monitorLoop();
    };

} // namespace negotio

#endif // NEGOTIO_MONITOR_H
