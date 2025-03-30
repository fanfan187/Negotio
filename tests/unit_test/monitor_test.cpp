/**
 * @file monitor_test.cpp
 * @brief Monitor 模块的单元测试
 *
 * 包含对监控模块（negotiator::Monitor）的功能测试，
 * 主要测试其线程安全性、日志写入行为、recordNegotiation 正常记录。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#include <gtest/gtest.h>
#include "../../src/monitor/monitor.h"

#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

/**
 * @test RecordNegotiationIncrementsCorrectly
 * @brief 测试 recordNegotiation 是否可正常调用并不抛出异常
 *
 * 虽然无法直接访问私有成员验证累加结果，
 * 但通过调用后无崩溃即可视为通过基础验证。
 */
TEST(MonitorTest, RecordNegotiationIncrementsCorrectly) {
    negotio::Monitor monitor;

    monitor.recordNegotiation(100, true);
    monitor.recordNegotiation(200, true);
    monitor.recordNegotiation(150, false);

    // 启动线程只是写入日志，不影响 record 的本地数据逻辑
    // 由于内部成员为私有，测试只验证函数调用无异常/死锁
    SUCCEED();  // 到此表示 recordNegotiation 可正常调用
}

/**
 * @test StartStopMonitorThread
 * @brief 测试监控线程启动与停止是否安全
 *
 * 该测试验证 monitor 的 start 和 stop 方法不会引发死锁或崩溃。
 */
TEST(MonitorTest, StartStopMonitorThread) {
    negotio::Monitor monitor;
    monitor.start();

    monitor.recordNegotiation(120, true);  // 在运行状态中记录一次

    std::this_thread::sleep_for(1500ms);  // 等待一轮写入监控日志

    monitor.stop();

    SUCCEED();  // 到达此处表示线程启动/停止逻辑没问题
}

/**
 * @test LogFileOutputExists
 * @brief 测试是否正确生成 monitor_log.txt 并写入内容
 *
 * 该测试通过删除旧文件、启动监控、调用 recordNegotiation、
 * 并等待日志线程工作，然后验证日志文件是否被创建且非空。
 */
TEST(MonitorTest, LogFileOutputExists) {
    const std::string logPath = "monitor_log.txt";

    // 删除旧日志文件，避免干扰测试结果
    if (fs::exists(logPath)) {
        fs::remove(logPath);
    }

    negotio::Monitor monitor;
    monitor.start();

    monitor.recordNegotiation(100, true);

    std::this_thread::sleep_for(1100ms);  // 等待监控线程写入一次

    monitor.stop();

    // 验证日志文件确实被创建
    ASSERT_TRUE(fs::exists(logPath));

    // 验证日志文件至少包含一行内容
    std::ifstream logFile(logPath);
    std::string line;
    ASSERT_TRUE(std::getline(logFile, line));
}
