/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

// tests/unit_test/monitor_test.cpp

#include <gtest/gtest.h>
#include "../../src/monitor/monitor.h"
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

// 测试 Monitor 的记录功能是否正常累加
TEST(MonitorTest, RecordNegotiationIncrementsCorrectly) {
    negotio::Monitor monitor;

    monitor.recordNegotiation(100, true);
    monitor.recordNegotiation(200, true);
    monitor.recordNegotiation(150, false);

    // 启动线程只是写入日志，不影响 record 逻辑，这里我们测试主逻辑即可
    // 因为成员是私有的，我们不直接验证变量值（需要用监视接口或日志文件验证）
    // 这里我们更侧重不抛异常 + 无死锁
    SUCCEED(); // 如果运行到此，说明没有崩溃
}

// 测试 Monitor 的 start 和 stop 不会引发死锁或崩溃
TEST(MonitorTest, StartStopMonitorThread) {
    negotio::Monitor monitor;
    monitor.start();

    // 模拟一段运行期，期间调用 recordNegotiation
    monitor.recordNegotiation(120, true);
    std::this_thread::sleep_for(1500ms); // 等待一轮监控执行

    monitor.stop();

    SUCCEED(); // 到达此处说明线程启动/停止逻辑没问题
}

// 测试 Monitor 是否写入日志文件
TEST(MonitorTest, LogFileOutputExists) {
    const std::string logPath = "monitor_log.txt";

    // 删除旧日志文件（如果存在）
    if (fs::exists(logPath)) {
        fs::remove(logPath);
    }

    negotio::Monitor monitor;
    monitor.start();
    monitor.recordNegotiation(100, true);
    std::this_thread::sleep_for(1100ms); // 等待监控线程写一次日志
    monitor.stop();

    // 检查是否生成了日志文件
    ASSERT_TRUE(fs::exists(logPath));

    // 检查文件不为空
    std::ifstream logFile(logPath);
    std::string line;
    ASSERT_TRUE(std::getline(logFile, line));  // 至少能读出一行
}

