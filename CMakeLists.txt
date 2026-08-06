cmake_minimum_required(VERSION 3.20)

project(HFTMatchingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------
# GoogleTest
# --------------------------------------------------

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# --------------------------------------------------
# Core matching engine library
# --------------------------------------------------

add_library(hft_engine
    src/Order.cpp
    src/OrderBook.cpp
    src/MatchingEngine.cpp
    src/Logger.cpp
    src/OutputWriter.cpp
    src/SnapshotManager.cpp
    src/MarketDataPublisher.cpp
)

target_include_directories(hft_engine
    PUBLIC
        include
)

# --------------------------------------------------
# Matching Engine executable
# --------------------------------------------------

add_executable(matching_engine
    src/main.cpp
)

target_link_libraries(matching_engine
    PRIVATE
        hft_engine
)

# --------------------------------------------------
# Benchmark
# --------------------------------------------------

add_executable(benchmark
    benchmark/benchmark.cpp
)

target_link_libraries(benchmark
    PRIVATE
        hft_engine
)

# --------------------------------------------------
# Unit Tests
# --------------------------------------------------

add_executable(unit_tests
    tests/test_main.cpp
    tests/OrderTests.cpp
    tests/OrderBookTests.cpp
    tests/MatchingEngineTests.cpp
)

target_link_libraries(unit_tests
    PRIVATE
        hft_engine
        GTest::gtest
        GTest::gtest_main
)
