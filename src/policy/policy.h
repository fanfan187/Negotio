/**
* @file policy.h
 * @brief 策略管理模块接口定义
 *
 * 提供策略的添加、删除、校验和查询功能，支持并发访问。
 * 可用于协商模块或策略控制流程中。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#pragma once
#ifndef NEGOTIO_POLICY_H
#define NEGOTIO_POLICY_H

#include "common.h"
#include <unordered_map>
#include <mutex>
#include <optional>

namespace negotio {

    /**
     * @brief 策略管理类
     *
     * 负责管理内存中的策略集合，支持线程安全的添加、移除、查询等操作。
     */
    class PolicyManager {
    public:
        /**
         * @brief 构造函数
         *
         * 初始化策略管理器，可预留空间以优化性能。
         */
        PolicyManager();

        /**
         * @brief 析构函数
         *
         * 默认析构行为，自动释放资源。
         */
        ~PolicyManager();

        /**
         * @brief 添加策略
         *
         * 若策略数量未超过上限，且策略 ID 不重复，则添加成功。
         *
         * @param config 策略配置对象
         * @return 添加成功返回 true，失败返回 false
         */
        bool addPolicy(const PolicyConfig &config);

        /**
         * @brief 删除指定策略
         *
         * @param policy_id 策略 ID
         * @return 若删除成功返回 true，否则返回 false
         */
        bool removePolicy(uint32_t policy_id);

        /**
         * @brief 检查策略是否存在
         *
         * 用于快速判断指定策略是否有效。
         *
         * @param policy_id 策略 ID
         * @return 存在返回 true，不存在返回 false
         */
        bool checkPolicy(uint32_t policy_id);

        /**
         * @brief 获取指定策略配置
         *
         * 返回只读的策略信息副本。
         *
         * @param policy_id 策略 ID
         * @return 若存在返回策略配置，否则返回 std::nullopt
         */
        std::optional<PolicyConfig> getPolicy(uint32_t policy_id);

    private:
        std::unordered_map<uint32_t, PolicyConfig> policies;  ///< 策略容器，使用策略 ID 作为键
        std::mutex policiesMutex;                             ///< 互斥锁，保护并发访问策略容器
        static constexpr uint32_t MAX_POLICIES = 4096;        ///< 策略最大容量限制
    };

} // namespace negotio

#endif // NEGOTIO_POLICY_H
