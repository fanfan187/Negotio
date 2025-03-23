/**
* @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

#pragma once
#ifndef NEGOTIO_UNIXSOCKET_H
#define NEGOTIO_UNIXSOCKET_H

#include <string>
#include <functional>

namespace negotio {

    /**
     * @brief 定义命令处理回调，参数为接收到的命令字符串
     */
    using CommandHandler = std::function<void(const std::string &cmd)>;

    /**
     * @brief UNIX 域套接字服务器类
     */
    class UnixSocketServer {
    public:
        UnixSocketServer();
        ~UnixSocketServer();

        /**
         * @brief 初始化Unix域套接字服务端
         * @param path Unix域套接字文件路径
         * @return true 表示成功，false 表示失败
         */
        bool init(const std::string &path);

        /**
         * @brief 设置命令处理回调函数
         * @param handler 命令处理回调
         */
        void setCommandHandler(CommandHandler handler);

        /**
         * @brief 启动服务（阻塞方式接受连接并处理命令）
         */
        void run();

        /**
         * @brief 停止服务
         */
        void stop();

    private:
        int sockfd;                ///< 套接字文件描述符
        std::string socketPath;    ///< 套接字路径
        CommandHandler commandHandler; ///< 命令处理回调
        bool running;              ///< 运行标志
    };

} // namespace negotio

#endif // NEGOTIO_UNIXSOCKET_H
