/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

// tests/utils/test_util.h

#pragma once

#include <string>
#include <sstream>
#include <filesystem>
#include <unistd.h>  // getpid()

namespace testutils {

    /**
     * @brief 为每次测试生成唯一的 Unix Socket 路径，并自动清理残留 socket 文件
     */
    class UniqueSocketPath {
    public:
        explicit UniqueSocketPath(const std::string &prefix = "/tmp/test_negotio_socket_") {
            std::ostringstream oss;
            oss << prefix << getpid();
            path = oss.str();
            remove();  // 测试开始前清理旧文件
        }

        ~UniqueSocketPath() {
            remove();  // 测试结束自动清理
        }

        [[nodiscard]] const std::string &get() const {
            return path;
        }

    private:
        std::string path;

        void remove() const {
            if (std::filesystem::exists(path)) {
                std::filesystem::remove(path);
            }
        }
    };

} // namespace testutils


