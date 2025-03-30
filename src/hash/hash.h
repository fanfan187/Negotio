/**
 * @file hash.h
 * @brief 提供 SHA256 哈希计算功能。
 *
 * 该文件声明了用于计算 SHA256 哈希值的函数。支持对字节数据和 32 位无符号整数数据的计算，
 * 输出的哈希值均为 32 字节。该模块主要用于生成协商密钥时的数据完整性校验。
 *
 * @author fanfan187
 * @version 1.0.0
 * @since 1.0.0
 */

#pragma once

#ifndef NEGOTIO_HASH_H
#define NEGOTIO_HASH_H

#include <cstdint>
#include <vector>

/**
 * @brief 计算字节数据的 SHA256 哈希值。
 *
 * 该函数接收一个包含字节数据的 vector，对数据进行 SHA256 哈希计算，
 * 并返回一个包含 32 字节结果的 vector。
 *
 * @param data 包含待计算哈希的字节数据的 vector。
 * @return std::vector<uint8_t> 长度为 32 字节的 SHA256 哈希值。
 */
std::vector<uint8_t> CalculateSHA256(const std::vector<uint8_t>& data);

/**
 * @brief 计算 32 位无符号整数数据的 SHA256 哈希值。
 *
 * 此函数为重载版本，接收一个包含 32 位无符号整数的 vector，对其进行 SHA256 哈希计算，
 * 并返回一个包含 32 字节结果的 vector。
 *
 * @param data 包含待计算哈希的 32 位无符号整数数据的 vector。
 * @return std::vector<uint8_t> 长度为 32 字节的 SHA256 哈希值。
 */
std::vector<uint8_t> CalculateSHA256(const std::vector<uint32_t>& data);

#endif // NEGOTIO_HASH_H
