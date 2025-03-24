cmake_minimum_required(VERSION 3.22.1)
project(Negotio)

set(CMAKE_CXX_STANDARD 20)

# 如果是 Windows 平台，则手动设置 OpenSSL 路径
if(WIN32)
    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64")
    set(OPENSSL_CRYPTO_LIBRARY "C:/Program Files/OpenSSL-Win64/lib/libcrypto.lib")
    set(OPENSSL_SSL_LIBRARY "C:/Program Files/OpenSSL-Win64/lib/libssl.lib")
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_LIBRARIES}")
else()
    message(FATAL_ERROR "OpenSSL not found!")
endif()

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
        tests/unit_test.cpp
        tests/performance_test.cpp
        include/common.h
        src/hash/hash.cpp
        src/hash/hash.h
        src/policy/policy.cpp
        include/json_support.h
)

target_link_libraries(Negotio PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(Negotio PRIVATE ${CMAKE_SOURCE_DIR}/external)

