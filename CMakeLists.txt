cmake_minimum_required(VERSION 3.22.1)
project(Negotio)

set(CMAKE_CXX_STANDARD 20)

# 开启调试日志
add_definitions(-DENABLE_DEBUG=1 -DDEBUG)

# Windows 平台特殊设置
if (WIN32)
    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
    set(OPENSSL_CRYPTO_LIBRARY "C:/Program Files/OpenSSL-Win64/lib/libcrypto.lib")
    set(OPENSSL_SSL_LIBRARY "C:/Program Files/OpenSSL-Win64/lib/libssl.lib")
endif ()

find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_LIBRARIES}")
else ()
    message(FATAL_ERROR "OpenSSL not found!")
endif ()

# 添加 GoogleTest 子目录（引入 external/googletest）
add_subdirectory(external/googletest)

# -------------------------------------------------------------------------------
# 1. 创建业务逻辑库 negotiolib
# -------------------------------------------------------------------------------
add_library(negotiolib STATIC
        src/hash/hash.cpp
        src/hash/hash.h

        src/monitor/monitor.cpp
        src/monitor/monitor.h

        src/negotiate/negotiate.cpp
        src/negotiate/negotiate.h

        src/policy/policy.cpp
        src/policy/policy.h

        src/udp/udp.cpp
        src/udp/udp.h

        src/unixsocket/unixsocket.cpp
        src/unixsocket/unixsocket.h
)

target_include_directories(negotiolib
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        PRIVATE
        ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(negotiolib
        PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        pthread
)

# -------------------------------------------------------------------------------
# 2. 主程序目标 Negotio
# -------------------------------------------------------------------------------
add_executable(Negotio
        src/NegotioApplication.cpp
        tests/utils/test_util.h
)

target_link_libraries(Negotio
        PRIVATE negotiolib
)

target_include_directories(Negotio
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external
)

# -------------------------------------------------------------------------------
# 3. 单元测试目标 NegotioUnitTest
# -------------------------------------------------------------------------------
add_executable(NegotioUnitTest
        tests/unit_test/hash_test.cpp
        tests/unit_test/policy_test.cpp
        tests/unit_test/udp_test.cpp
        tests/unit_test/monitor_test.cpp
        tests/unit_test/negotiate_test.cpp
        tests/unit_test/unixsocket_test.cpp
)

target_include_directories(NegotioUnitTest
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external
        ${CMAKE_SOURCE_DIR}/tests/unit_test
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NegotioUnitTest
        PRIVATE
        negotiolib
        gtest_main
        pthread
)

# 自动注册 GTest 测试项，CLion 才能识别
include(GoogleTest)
gtest_discover_tests(NegotioUnitTest)


# -------------------------------------------------------------------------------
# 4. 性能测试目标 NegotioPerformanceTest
# -------------------------------------------------------------------------------
add_executable(NegotioPerformanceTest
        tests/performance_test.cpp
)

target_include_directories(NegotioPerformanceTest
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external
)

target_link_libraries(NegotioPerformanceTest
        PRIVATE negotiolib
)
