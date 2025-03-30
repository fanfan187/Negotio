/**
 * @file policy.cpp
 * @brief 策略管理模块实现
 *
 * 提供策略的添加、移除、校验和查询等功能，支持并发访问。
 * 使用互斥锁保证线程安全。
 *
 * @author   fanfan187
 * @version  v1.0.0
 * @since    v1.0.0
 */

#include "policy.h"

namespace negotio {

    /**
     * @brief 构造函数
     *
     * 初始化策略容器，并为 unordered_map 预留空间，减少重哈希次数。
     */
    PolicyManager::PolicyManager() {
        policies.reserve(MAX_POLICIES);  // 预留最大策略数容量，提升性能
    }

    /**
     * @brief 析构函数
     *
     * 无需显式资源释放，容器会自动析构。
     */
    PolicyManager::~PolicyManager() {
        // 无需特殊释放，unordered_map 会自动释放
    }

    /**
     * @brief 添加策略配置
     *
     * 若当前策略数已达上限，或策略 ID 重复，则添加失败。
     *
     * @param config 策略配置对象
     * @return true 添加成功
     * @return false 添加失败
     */
    bool PolicyManager::addPolicy(const PolicyConfig &config) {
        std::lock_guard lock(policiesMutex);

        // 达到最大策略数限制
        if (policies.size() >= MAX_POLICIES) {
            return false;
        }

        // 策略 ID 已存在
        if (policies.contains(config.policy_id)) {
            return false;
        }

        // 添加新策略
        policies[config.policy_id] = config;
        return true;
    }

    /**
     * @brief 移除指定策略
     *
     * 若策略 ID 存在则移除，并返回 true；否则返回 false。
     *
     * @param policy_id 策略 ID
     * @return true 删除成功
     * @return false 策略不存在
     */
    bool PolicyManager::removePolicy(uint32_t policy_id) {
        std::lock_guard lock(policiesMutex);
        return policies.erase(policy_id) > 0;
    }

    /**
     * @brief 校验策略是否存在
     *
     * @param policy_id 策略 ID
     * @return true 存在
     * @return false 不存在
     */
    bool PolicyManager::checkPolicy(uint32_t policy_id) {
        std::lock_guard lock(policiesMutex);
        return policies.contains(policy_id);
    }

    /**
     * @brief 获取指定策略的配置内容
     *
     * 若策略存在则返回对应配置，否则返回 std::nullopt。
     *
     * @param policy_id 策略 ID
     * @return std::optional<PolicyConfig> 查询结果
     */
    std::optional<PolicyConfig> PolicyManager::getPolicy(uint32_t policy_id) {
        std::lock_guard lock(policiesMutex);

        if (const auto it = policies.find(policy_id); it != policies.end()) {
            return it->second;
        }

        return std::nullopt;
    }

} // namespace negotio
