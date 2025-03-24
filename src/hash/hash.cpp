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
#include <openssl/evp.h>
#include <openssl/types.h>

#include "common.h"

// 内部静态函数: 对任意数据缓冲区计算 SHA256
static std::vector<uint8_t> SHA256HashBytes(const void *data, const size_t length) {
    std::vector<uint8_t> hashValue(SHA256_DIGEST_LENGTH);  // SHA256 输出长度32字节

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    // 创建 EVC_MD_CTX 上下文
    if (const EVP_MD_CTX* md_ctx_st = EVP_MD_CTX_new(); !md_ctx_st) {
        // 创建失败，返回空向量
        return {};
    }

    // 初始化上下文为 SHA256
    if (EVP_DigestInit(ctx, EVP_sha256()) != 1) {
        // 初始化失败，释放上下文并返回空向量
        EVP_MD_CTX_free(ctx);
        return {};
    }

    // 更新数据
    if (EVP_DigestUpdate(ctx, data, length) != 1) {
        // 更新失败，释放上下文并返回空向量
        EVP_MD_CTX_free(ctx);
        return {};
    }

    unsigned int hashLength = 0;
    // 计算哈希值,将结果写入 hashValue
    if (EVP_DigestFinal(ctx, hashValue.data(), &hashLength) != 1) {
        EVP_MD_CTX_free(ctx);
        return {};
    }

    EVP_MD_CTX_free(ctx);
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
