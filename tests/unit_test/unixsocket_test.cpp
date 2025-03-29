/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

// tests/unit_test/unixsocket_test.cpp

#include <gtest/gtest.h>
#include "../../src/unixsocket/unixsocket.h"
#include "../utils/test_util.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <thread>
#include <chrono>

using namespace negotio;
using namespace testutils;

using namespace negotio;

namespace fs = std::filesystem;

static const std::string SOCKET_PATH = "/tmp/test_negotio_socket";

// 清理 socket 文件（防止冲突）
void removeSocketFile() {
    if (fs::exists(SOCKET_PATH)) {
        fs::remove(SOCKET_PATH);
    }
}

// 连接到 socket 并发送命令
void sendCommandToServer(const std::string& path, const std::string& cmd) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    ASSERT_EQ(connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    std::string cmdWithNewline = cmd + "\n";
    ssize_t sent = write(fd, cmdWithNewline.c_str(), cmdWithNewline.size());
    ASSERT_EQ(sent, cmdWithNewline.size());

    close(fd);
}


TEST(UnixSocketTest, InitAndHandleCommand) {
    UniqueSocketPath sockPath;
    UnixSocketServer server;

    ASSERT_TRUE(server.init(sockPath.get()));

    std::string receivedCmd;
    server.setCommandHandler([&receivedCmd](const std::string &cmd) {
        receivedCmd = cmd;
    });

    std::thread serverThread([&]() {
        server.run();
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 客户端发送命令
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_NE(fd, -1);

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, sockPath.get().c_str(), sizeof(addr.sun_path) - 1);

    ASSERT_EQ(connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);
    std::string msg = "shutdown\n";
    ASSERT_EQ(write(fd, msg.c_str(), msg.size()), msg.size());
    close(fd);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    server.stop();
    serverThread.join();

    EXPECT_EQ(receivedCmd, "shutdown");
}

TEST(UnixSocketTest, InitWithInvalidPathFails) {
    UnixSocketServer server;
    ASSERT_FALSE(server.init("/this/path/should/fail")); // 非法路径应初始化失败
}

