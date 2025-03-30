/**
 * @file unixsocket.cpp
 * @brief 实现基于 Unix 域套接字的本地命令接收模块
 *
 * 使用非阻塞模式 + epoll 实现本地命令监听与处理。
 * 支持客户端通过 Unix 套接字连接发送控制命令，服务端接收处理。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#include "unixsocket.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netinet/in.h>
#include <cstring>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cerrno>
#include <iostream>
#include <filesystem>
#include "common.h"

namespace negotio {

    /**
     * @brief 构造函数
     *
     * 初始化成员变量，设置 sockfd 为 -1，running 为 false。
     */
    UnixSocketServer::UnixSocketServer() : sockfd(-1), running(false) {
    }

    /**
     * @brief 析构函数
     *
     * 停止服务并删除 Unix 套接字路径文件。
     */
    UnixSocketServer::~UnixSocketServer() {
        stop();
        if (!socketPath.empty()) {
            unlink(socketPath.c_str());
        }
    }

    /**
     * @brief 初始化并绑定 Unix 域套接字
     *
     * 创建 SOCK_STREAM 类型的 AF_UNIX 套接字，绑定路径并监听。
     * 设置非阻塞属性。
     *
     * @param path Unix 套接字路径
     * @return 初始化成功返回 true，否则返回 false
     */
    bool UnixSocketServer::init(const std::string &path) {
        socketPath = path;
        unlink(socketPath.c_str());  // 确保旧套接字路径被清除

        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd == -1) {
            std::cerr << "创建Unix域套接字失败" << std::endl;
            return false;
        }

        int flags = fcntl(sockfd, F_GETFL, 0);
        if (flags == -1 || fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
            std::cerr << "设置套接字为非阻塞失败" << std::endl;
            close(sockfd);
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

        if (listen(sockfd, 5) == -1) {
            std::cerr << "Unix域套接字监听失败" << std::endl;
            close(sockfd);
            return false;
        }

        return true;
    }

    /**
     * @brief 设置命令处理回调函数
     *
     * 接收到命令字符串后将通过该回调处理。
     *
     * @param handler 命令处理函数
     */
    void UnixSocketServer::setCommandHandler(const CommandHandler &handler) {
        commandHandler = handler;
    }

    /**
     * @brief 启动监听并处理客户端命令
     *
     * 使用 epoll 监听多个连接，实现高效的非阻塞命令接收处理流程。
     */
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

            const int nfds = epoll_wait(epollFd, events, kMaxEvents, 0);
            if (nfds == -1) {
                if (errno == EINTR) continue;
                std::cerr << "epoll_wait 失败" << std::endl;
                break;
            }

            for (int i = 0; i < nfds; ++i) {
                if (events[i].data.fd == sockfd) {
                    // 有新连接到来
                    while (true) {
                        const int clientFd = accept(sockfd, nullptr, nullptr);
                        if (clientFd == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break; // 已无更多连接
                            std::cerr << "accept 失败" << std::endl;
                            break;
                        }

                        // 设置为非阻塞
                        int clientFlags = fcntl(clientFd, F_GETFL, 0);
                        if (clientFlags != -1 &&
                            fcntl(clientFd, F_SETFL, clientFlags | O_NONBLOCK) == -1) {
                            std::cerr << "设置客户端套接字非阻塞失败" << std::endl;
                            close(clientFd);
                            continue;
                        }

                        // 添加到 epoll 监听
                        epoll_event clientEv{};
                        clientEv.events = EPOLLIN | EPOLLET;
                        clientEv.data.fd = clientFd;
                        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &clientEv) == -1) {
                            std::cerr << "将 clientFd 添加到 epoll 失败" << std::endl;
                            close(clientFd);
                        }
                    }
                } else {
                    // 处理客户端命令数据
                    const int clientFd = events[i].data.fd;
                    std::string cmd;
                    bool done = false;

                    while (!done) {
                        char buffer[1024];
                        const ssize_t count = read(clientFd, buffer, sizeof(buffer));
                        if (count == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                            std::cerr << "读取客户端数据失败" << std::endl;
                            done = true;
                        } else if (count == 0) {
                            // 客户端关闭连接
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

                    // 移除监听并关闭连接
                    if (epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr) == -1) {
                        std::cerr << "从 epoll 移除 clientFd 失败" << std::endl;
                    }
                    close(clientFd);
                }
            }
        }

        close(epollFd);
    }

    /**
     * @brief 停止服务器并关闭套接字
     */
    void UnixSocketServer::stop() {
        running = false;
        if (sockfd != -1) {
            close(sockfd);
            sockfd = -1;
        }
    }

} // namespace negotio
