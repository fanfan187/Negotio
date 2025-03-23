/**
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */
#pragma once

#include <nlohmann/json.hpp>

#include "common.h"

namespace negotio {

    inline void to_json(nlohmann::json& j, const PolicyConfig& p) {
        j = nlohmann::json{
            {"policy_id", p.policy_id},
            {"remote_ip", p.remote_ip},
            {"remote_port", p.remote_port},
            {"timeout_ms", p.timeout_ms},
            {"retry_times", p.retry_times}
        };
    }

    inline void from_json(const nlohmann::json& j, PolicyConfig& p) {
        j.at("policy_id").get_to(p.policy_id);
        j.at("remote_ip").get_to(p.remote_ip);
        j.at("remote_port").get_to(p.remote_port);
        j.at("timeout_ms").get_to(p.timeout_ms);
        j.at("retry_times").get_to(p.retry_times);
    }
}
