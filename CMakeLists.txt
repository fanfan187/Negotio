cmake_minimum_required(VERSION 3.22.1)
project(Negotio)

set(CMAKE_CXX_STANDARD 20)

# 如果是 Windows 平台，则手动设置 OpenSSL 路径
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

# 主程序目标 Negotio
add_executable(Negotio
        src/NegotioApplication.cpp
        src/udp/udp.cpp
        src/udp/udp.h
        src/unixsocket/unixsocket.cpp
        src/unixsocket/unixsocket.h
        src/negotiate/negotiate.cpp
        src/negotiate/negotiate.h
        src/monitor/monitor.cpp
        src/monitor/monitor.h
        include/common.h
        src/hash/hash.cpp
        src/hash/hash.h
        src/policy/policy.cpp
        include/json_support.h
)

# 使用目标级设置添加头文件目录（例如 external 目录也可以这样添加）
target_include_directories(Negotio PRIVATE ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external)

target_link_libraries(Negotio PRIVATE OpenSSL::SSL OpenSSL::Crypto pthread)

# 单元测试目标 NegotioUnitTest
add_executable(NegotioUnitTest
        tests/unit_test.cpp
        src/udp/udp.cpp
        src/unixsocket/unixsocket.cpp
        src/negotiate/negotiate.cpp
        src/hash/hash.cpp
        src/monitor/monitor.cpp
        src/policy/policy.cpp
)

target_include_directories(NegotioUnitTest PRIVATE ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(NegotioUnitTest PRIVATE OpenSSL::SSL OpenSSL::Crypto pthread)

# 性能测试目标 NegotioPerformanceTest
add_executable(NegotioPerformanceTest
        tests/performance_test.cpp
        src/udp/udp.cpp
        src/unixsocket/unixsocket.cpp
        src/negotiate/negotiate.cpp
        src/hash/hash.cpp
        src/monitor/monitor.cpp
        src/policy/policy.cpp
)

target_include_directories(NegotioPerformanceTest PRIVATE ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external)
target_link_libraries(NegotioPerformanceTest PRIVATE OpenSSL::SSL OpenSSL::Crypto pthread)
