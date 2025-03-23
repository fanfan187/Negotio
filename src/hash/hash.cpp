/**
 * @file hash.cpp
 * @brief 哈希计算模块实现
 *
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

#include "hash.h"
#include <openssl/sha.h>
#include <vector>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <stdexcept>

#include "common.h"

// 内部静态函数: 对任意数据缓冲区计算 SHA256
static std::vector<uint8_t> SHA256HashBytes(const void *data,size_t length) {
    std::vector<uint8_t> hashValue(SHA256_DIGEST_LENGTH);  // SHA256 输出长度32字节
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    // 调用 OpenSSL 更新哈希计算
    SHA256_Update(&ctx, data, length);
    // 完成哈希计算并将结果写入 hashValue
    SHA256_Final(hashValue.data(), &ctx);
    return hashValue;
}

std::vector<uint8_t> CalculateSHA256(const std::vector<uint8_t>& data) {
    // 直接使用字节数据计算哈希
    return SHA256HashBytes(data.data(), data.size());
}

std::vector<uint8_t> CalculateSHA256(const std::vector<uint32_t>& data) {
    // 将 32 位整数向量视为原始字节数据来计算哈希
    return SHA256HashBytes(data.data(), data.size() * sizeof(uint32_t));
}
