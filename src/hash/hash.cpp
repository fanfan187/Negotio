/**
 * @file hash.cpp
 * @brief 哈希计算模块实现
 *
 * 该文件实现了计算 SHA256 哈希值的函数，包括对字节数据和 32 位无符号整数数据的计算。
 *
 * @author  fanfan187
 * @version 1.0.0
 * @since 1.0.0
 */

#include "hash.h"
#include <openssl/sha.h>
#include <vector>
#include <cstdint>
#include <cstring>
#include "common.h"

/**
 * @brief 计算字节数据的 SHA256 哈希值。
 *
 * 对输入的字节数据进行 SHA256 哈希计算，并返回一个包含 32 字节结果的 vector。
 * 如果计算过程中发生错误，则返回一个空的 vector。
 *
 * @param data 包含待计算哈希的字节数据的 vector。
 * @return std::vector<uint8_t> 长度为 32 字节的 SHA256 哈希值，如果计算失败则返回空 vector。
 */
std::vector<uint8_t> CalculateSHA256(const std::vector<uint8_t> &data) {
    std::vector<uint8_t> hashValue(SHA256_DIGEST_LENGTH);
    if (!SHA256(data.data(), data.size(), hashValue.data())) {
        return {};
    }
    return hashValue;
}

/**
 * @brief 计算 32 位无符号整数数据的 SHA256 哈希值。
 *
 * 将输入的 32 位无符号整数数据转换为字节数据后进行 SHA256 哈希计算，
 * 并返回一个包含 32 字节结果的 vector。如果计算过程中发生错误，则返回一个空的 vector。
 *
 * @param data 包含待计算哈希的 32 位无符号整数数据的 vector。
 * @return std::vector<uint8_t> 长度为 32 字节的 SHA256 哈希值，如果计算失败则返回空 vector。
 */
std::vector<uint8_t> CalculateSHA256(const std::vector<uint32_t> &data) {
    const auto *byteData = reinterpret_cast<const uint8_t *>(data.data());
    std::vector<uint8_t> hashValue(SHA256_DIGEST_LENGTH);
    if (!SHA256(byteData, data.size() * sizeof(uint32_t), hashValue.data())) {
        return {};
    }
    return hashValue;
}
