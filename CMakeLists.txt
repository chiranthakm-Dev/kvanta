cmake_minimum_required(VERSION 3.28)
project(memdb VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find dependencies
find_package(spdlog REQUIRED)
find_package(CLI11 REQUIRED)
find_package(GTest REQUIRED)
find_package(benchmark REQUIRED)

# Include directories
include_directories(src)

# Source files
set(SOURCES
    src/main.cpp
    src/server/tcp_server.cpp
    src/server/connection_handler.cpp
    src/protocol/resp_parser.cpp
    src/protocol/resp_writer.cpp
    src/storage/string_store.cpp
    src/storage/list_store.cpp
    src/storage/hash_store.cpp
    src/expiry/expiry_manager.cpp
    src/wal/write_ahead_log.cpp
    src/wal/wal_recovery.cpp
)

# Executables
add_executable(memdb-server ${SOURCES})
target_link_libraries(memdb-server PRIVATE spdlog::spdlog CLI11::CLI11)

add_executable(memdb-tests ${SOURCES} tests/test_rcu_store.cpp tests/test_lru.cpp tests/test_wal_recovery.cpp tests/test_ttl.cpp tests/test_resp_parser.cpp tests/test_thread_pool.cpp)
target_link_libraries(memdb-tests PRIVATE spdlog::spdlog CLI11::CLI11 GTest::gtest_main)

add_executable(memdb-benchmarks ${SOURCES} benchmarks/bench_get_set.cpp)
target_link_libraries(memdb-benchmarks PRIVATE spdlog::spdlog CLI11::CLI11 benchmark::benchmark_main)

# Compiler flags
target_compile_options(memdb-server PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(memdb-tests PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(memdb-benchmarks PRIVATE -Wall -Wextra -Wpedantic)

# For sanitizers, add flags in build
