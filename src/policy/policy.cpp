/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

#include "policy.h"

namespace negotio {
    PolicyManager::PolicyManager() {
        // 可预留空间以减少重哈希开销
        policies.reserve(MAX_POLICIES);
    }

    PolicyManager::~PolicyManager() {
        // 无需特殊释放，unordered_map 会自动释放
    }

    bool PolicyManager::addPolicy(const PolicyConfig &config) {
        std::lock_guard lock(policiesMutex);
        if (policies.size() >= MAX_POLICIES) {
            return false;
        }
        // 若策略ID已存在，则不允许重复添加
        if (policies.contains(config.policy_id)) {
            return false;
        }
        policies[config.policy_id] = config;
        return true;
    }

    bool PolicyManager::removePolicy(uint32_t policy_id) {
        std::lock_guard lock(policiesMutex);
        return policies.erase(policy_id) > 0;
    }

    bool PolicyManager::checkPolicy(uint32_t policy_id) {
        std::lock_guard lock(policiesMutex);
        return policies.contains(policy_id);
    }

    std::optional<PolicyConfig> PolicyManager::getPolicy(uint32_t policy_id) {
        std::lock_guard lock(policiesMutex);
        if (const auto it = policies.find(policy_id); it != policies.end()) {
            return it->second;
        }
        return std::nullopt;
    }

} // namespace negotio

