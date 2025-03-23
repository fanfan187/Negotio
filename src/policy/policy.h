/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
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
     * @brief 策略管理类，提供添加、删除和查询策略接口
     */
    class PolicyManager {
    public:
        PolicyManager();

        ~PolicyManager();

        /**
         * @brief 添加策略
         * @param config 策略配置
         * @return 添加成功返回 true；若策略已存在或超出上限返回 false
         */
        bool addPolicy(const PolicyConfig &config);

        /**
         * @brief 删除指定策略
         * @param policy_id 策略ID
         * @return 删除成功返回 true，否则返回 false
         */
        bool removePolicy(uint32_t policy_id);

        /**
         * @brief 检查策略是否合法或满足某些条件
         * @param policy_id
         * @return 若策略通过校验返回 true，否则返回 false
         */
        bool checkPolicy(uint32_t policy_id);

        /**
         * @brief 获取指定策略（只读）
         * @param policy_id 策略ID
         * @return 若存在返回对应策略，否则返回 std::nullopt
         */
        std::optional<PolicyConfig> getPolicy(uint32_t policy_id);

    private:
        std::unordered_map<uint32_t, PolicyConfig> policies; ///< 存储策略的容器
        std::mutex policiesMutex; ///< 保护容器的互斥锁
        static constexpr uint32_t MAX_POLICIES = 4096; ///< 最大支持策略数量
    };
} // namespace negotio

#endif // NEGOTIO_POLICY_H
