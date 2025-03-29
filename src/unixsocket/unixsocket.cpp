/**
 * unixsocket_test.cpp
 *
 * 修改说明：
 * - 将部分非关键状态信息的输出（例如 "UnixSocketServer 正在运行，监听 ..."）使用 DEBUG_LOG 包裹，
 *   这样在非调试模式下不会输出。
 */

#include "unixsocket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include <fcntl.h>
#include <sstream>
#include <sys/epoll.h>
#include <cerrno>
#include <iostream>
#include <unistd.h>             // getpid()
#include <filesystem>           // C++17 文件操作
#include <sstream>              // 拼接唯一路径

#include "common.h"

namespace negotio {
    UnixSocketServer::UnixSocketServer() : sockfd(-1), running(false) {
    }

    UnixSocketServer::~UnixSocketServer() {
        stop();
        if (!socketPath.empty()) {
            unlink(socketPath.c_str());
        }
    }

    bool UnixSocketServer::init(const std::string &path) {
        socketPath = path;
        unlink(socketPath.c_str());
        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd == -1) {
            std::cerr << "创建Unix域套接字失败" << std::endl;
            return false;
        }
        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1) {
            std::cerr << "获取套接字标志失败" << std::endl;
            close(sockfd);
            return false;
        }
        if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::cerr << "设置套接字为非阻塞失败" << std::endl;
            close(sockfd);
            return false;
        }
        sockaddr_un addr = {};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);
        if (bind(sockfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
            std::cerr << "Unix域套接字绑定失败" << std::endl;
            close(sockfd);
            return false;
        }
        if (listen(sockfd, 5) == -1) {
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
        int epollFd = epoll_create1(0);
        if (epollFd == -1) {
            std::cerr << "创建 epoll 实例失败" << std::endl;
            return;
        }
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = sockfd;
        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
            std::cerr << "添加 sockfd 到 epoll 失败" << std::endl;
            close(epollFd);
            return;
        }
        DEBUG_LOG("UnixSocketServer 正在运行，监听 " << socketPath);
        while (running) {
            constexpr int kMaxEvents = 10;
            epoll_event events[kMaxEvents];
            int nfds = epoll_wait(epollFd, events, kMaxEvents, 0);
            if (nfds == -1) {
                if (errno == EINTR)
                    continue;
                std::cerr << "epoll_wait 失败" << std::endl;
                break;
            }
            for (int i = 0; i < nfds; ++i) {
                if (events[i].data.fd == sockfd) {
                    while (true) {
                        int clientFd = accept(sockfd, nullptr, nullptr);
                        if (clientFd == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                            else {
                                std::cerr << "accept 失败" << std::endl;
                                break;
                            }
                        }
                        int clientFlags = fcntl(clientFd, F_GETFL, 0);
                        if (clientFlags != -1) {
                            if (fcntl(clientFd, F_SETFL, clientFlags | O_NONBLOCK) == -1) {
                                std::cerr << "设置客户端套接字非阻塞失败" << std::endl;
                                close(clientFd);
                                continue;
                            }
                        }
                        epoll_event clientEv{};
                        clientEv.events = EPOLLIN | EPOLLET;
                        clientEv.data.fd = clientFd;
                        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &clientEv) == -1) {
                            std::cerr << "将 clientFd 添加到 epoll 失败" << std::endl;
                            close(clientFd);
                        }
                    }
                } else {
                    int clientFd = events[i].data.fd;
                    std::string cmd;
                    bool done = false;
                    while (!done) {
                        char buffer[1024];
                        ssize_t count = read(clientFd, buffer, sizeof(buffer));
                        if (count == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                            std::cerr << "读取客户端数据失败" << std::endl;
                            done = true;
                        } else if (count == 0) {
                            done = true;
                        } else {
                            cmd.append(buffer, count);
                            if (cmd.find('\n') != std::string::npos)
                                done = true;
                        }
                    }
                    if (!cmd.empty()) {
                        if (cmd.back() == '\n') {
                            cmd.pop_back();
                        }
                        if (commandHandler) {
                            commandHandler(cmd);
                        }
                    }
                    if (epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr) == -1) {
                        std::cerr << "从 epoll 移除 clientFd 失败" << std::endl;
                    }
                    close(clientFd);
                }
            }
        }
        close(epollFd);
    }

    void UnixSocketServer::stop() {
        running = false;
        if (sockfd != -1) {
            close(sockfd);
            sockfd = -1;
        }
    }
} // namespace negotio
