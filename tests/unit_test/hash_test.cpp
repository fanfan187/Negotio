/**
* @author fanfan187
 * @version v1.0.0
 * @since v1.0.0
 */

#include <gtest/gtest.h>
#include "../src/hash/hash.h"
#include <sstream>
#include <iomanip>
#include <vector>

// 辅助函数：将 vector<uint8_t> 转换为十六进制字符串
std::string vectorToHex(const std::vector<uint8_t>& vec) {
    std::ostringstream oss;
    for (auto byte : vec) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return oss.str();
}

// 测试 CalculateSHA256 针对字节数组的实现 [参考 hash.cpp :contentReference[oaicite:0]{index=0}&#8203;:contentReference[oaicite:1]{index=1} 和 hash.h :contentReference[oaicite:2]{index=2}&#8203;:contentReference[oaicite:3]{index=3}]
TEST(HashTest, SHA256ByteData) {
    // 测试数据：字符串 "test"
    std::vector<uint8_t> data = {'t', 'e', 's', 't'};
    const auto hash = CalculateSHA256(data);
    std::string hashHex = vectorToHex(hash);
    // 预期结果可以通过在线工具或 OpenSSL 命令生成，例如：
    // echo -n "test" | openssl dgst -sha256
    const std::string expected = "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08";
    EXPECT_EQ(hashHex, expected);
}

// 测试 CalculateSHA256 针对 uint32_t 数组的实现
TEST(HashTest, SHA256Uint32Data) {
    // 构造测试数据：以 uint32_t 数组表示 "test"
    // 注意字节序问题，确保输入数据与上面的字节数组相同
    // 假设本机为小端模式，下列表示与 "test" 等效的 uint32_t 值
    std::vector<uint32_t> data = {0x74736574}; // 't' 'e' 's' 't'
    const auto hash = CalculateSHA256(data);
    const std::string hashHex = vectorToHex(hash);
    const std::string expected = "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08";
    EXPECT_EQ(hashHex, expected);
}
