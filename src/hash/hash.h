/**
* @file hash.h
 * @brief 哈希计算模块
 * @details 实现基于 SHA256 的哈希计算功能,用于生成协商密钥
 *
 * @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */
#pragma once

#include <cstdint>
#ifndef NEGOTIO_HASH_H
#define NEGOTIO_HASH_H

#include <vector>
#include <string>
#include "common.h"

// namespace negotio {
//
//     class Hash {
//     public:
//         /**
//          * @brief 计算输入数据的 SHA256 哈希值
//          * @param data 输入数据
//          * @return 32字节的哈希值
//          */
//         static std::vector<uint8_t> CalculateSHA256(const std::vector<uint8_t>& data);
//
//         /**
//          * @brief 根据两个随机数生成协商密钥
//          * @param random1 第一个随机数
//          * @param random2 第二个随机数
//          * @return 32个字节的协商密钥
//          * @throw std::runtime_error 随机数大小不符合要求
//          */
//         static std::vector<uint8_t> GenerateKey(
//             const std::vector<uint8_t>& random1,
//             const std::vector<uint8_t>& random2
//             );
//
//         /**
//          * @brief 将哈希值转换为十六进制字符串
//          * @param hash 哈希值
//          * @return 十六进制字符串
//          */
//         static std::string ToHexString(const std::vector<uint8_t>& hash);
//
//     private:
//         // 禁止实例化
//         Hash() = delete;
//         ~Hash() = delete;
//         Hash(const Hash&) = delete;
//         Hash& operator=(const Hash&) = delete;
//     };
// }
//
//

// 计算输入字节数据的 SHA256 哈希值, 返回 32 字节的哈希值
std::vector<uint8_t> CalculateSHA256(const std::vector<uint8_t>& data);

// 重载:计算以 uint32_t 列表表示的二进制数据的 SHA256 哈希值, 返回 32 字节的哈希值
std::vector<uint8_t> CalculateSHA256(const std::vector<uint32_t>& data);

#endif // NEGOTIO_HASH_H

