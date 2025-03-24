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

// 计算输入字节数据的 SHA256 哈希值, 返回 32 字节的哈希值
std::vector<uint8_t> CalculateSHA256(const std::vector<uint8_t>& data);

// 重载:计算以 uint32_t 列表表示的二进制数据的 SHA256 哈希值, 返回 32 字节的哈希值
std::vector<uint8_t> CalculateSHA256(const std::vector<uint32_t>& data);

#endif // NEGOTIO_HASH_H

