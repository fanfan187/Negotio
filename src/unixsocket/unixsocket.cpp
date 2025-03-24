/**
 * 实现 UNIX 域套接字模块
 *
 * 采用阻塞方式接受客户端连接，读取以换行结束的命令字符串，
 * 每条命令通过回调函数传递出去。
 *
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

#include "unixsocket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include <iostream>
#include <sstream>

namespace negotio {

UnixSocketServer::UnixSocketServer() : sockfd(-1), running(false) {}

UnixSocketServer::~UnixSocketServer() {
    stop();
    if (!socketPath.empty()) {
        // 删除 socket 文件
        unlink(socketPath.c_str());
    }
}

bool UnixSocketServer::init(const std::string &path) {
    socketPath = path;
    // 如果已存在同名 socket 文件，先删除
    unlink(socketPath.c_str());

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "创建Unix域套接字失败" << std::endl;
        return false;
    }

    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Unix域套接字绑定失败" << std::endl;
        close(sockfd);
        return false;
    }

    if (listen(sockfd, 5) < 0) {
        std::cerr << "Unix域套接字监听失败" << std::endl;
        close(sockfd);
        return false;
    }
    return true;
}

void UnixSocketServer::setCommandHandler(const CommandHandler &handler) {
    commandHandler = handler;
}

void UnixSocketServer::run() {
    running = true;
    std::cout << "UnixSocketServer 运行中，监听 " << socketPath << std::endl;
    while (running) {
        const int clientFd = accept(sockfd, nullptr, nullptr);
        if (clientFd < 0) {
            // 如果被中断则退出
            if (running) {
                std::cerr << "接受客户端连接失败" << std::endl;
            }
            continue;
        }

        // 处理客户端连接（这里采用简单方式：同步读取并处理单个命令）
        constexpr size_t bufSize = 1024;
        char buffer[bufSize];
        std::string cmd;
        ssize_t n;
        // 持续读取直到遇到换行符或客户端关闭
        while ((n = read(clientFd, buffer, bufSize)) > 0) {
            cmd.append(buffer, n);
            if (cmd.find('\n') != std::string::npos) {
                break;
            }
        }
        // 移除末尾换行符
        if (!cmd.empty() && cmd.back() == '\n') {
            cmd.pop_back();
        }
        if (!cmd.empty() && commandHandler) {
            commandHandler(cmd);
        }
        close(clientFd);
    }
}

void UnixSocketServer::stop() {
    running = false;
    if (sockfd != -1) {
        close(sockfd);
        sockfd = -1;
    }
}

} // namespace negotio
