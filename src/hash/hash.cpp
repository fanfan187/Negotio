/**
 * @file hash.cpp
 * @brief 哈希计算模块实现
 *
 */

#include "hash.h"
#include <openssl/sha.h>
#include <vector>
#include <cstdint>
#include <cstring>

#include "common.h"

std::vector<uint8_t> CalculateSHA256(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> hashValue(SHA256_DIGEST_LENGTH);
    if (!SHA256(data.data(), data.size(), hashValue.data())) {
        return {};
    }
    return hashValue;
}

std::vector<uint8_t> CalculateSHA256(const std::vector<uint32_t> &data) {
    const auto *byteData = reinterpret_cast<const uint8_t *>(data.data());
    std::vector<uint8_t> hashValue(SHA256_DIGEST_LENGTH);
    if (!SHA256(byteData, data.size() * sizeof(uint32_t), hashValue.data())) {
        return {};
    }
    return hashValue;
}
