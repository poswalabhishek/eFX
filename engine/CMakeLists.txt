cmake_minimum_required(VERSION 3.24)
project(efx-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -flto)
    add_link_options(-flto)
endif()

find_package(Boost REQUIRED COMPONENTS system)

include(FetchContent)

# Fetch spdlog with bundled fmt to avoid version conflicts with system fmt
set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.15.3
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(cppzmq
    GIT_REPOSITORY https://github.com/zeromq/cppzmq.git
    GIT_TAG v4.10.0
)
set(CPPZMQ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZeroMQ REQUIRED IMPORTED_TARGET libzmq)
FetchContent_MakeAvailable(cppzmq)

FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)

# Core library (types, timestamps, utilities)
add_library(efx_core
    core/src/types.cpp
    core/src/config.cpp
)
target_include_directories(efx_core PUBLIC core/include)
target_link_libraries(efx_core PUBLIC
    spdlog::spdlog
    tomlplusplus::tomlplusplus
    nlohmann_json::nlohmann_json
)

# Market data library
add_library(efx_market_data
    market_data/src/simulator.cpp
    market_data/src/venue_book.cpp
)
target_include_directories(efx_market_data PUBLIC market_data/include)
target_link_libraries(efx_market_data PUBLIC efx_core)

# Pricing library
add_library(efx_pricing
    pricing/src/fair_value.cpp
    pricing/src/spread_constructor.cpp
    pricing/src/volatility.cpp
    pricing/src/client_pricer.cpp
)
target_include_directories(efx_pricing PUBLIC pricing/include)
target_link_libraries(efx_pricing PUBLIC efx_core efx_market_data)

# Order management library
add_library(efx_order
    order/src/order_manager.cpp
    order/src/client_simulator.cpp
)
target_include_directories(efx_order PUBLIC order/include)
target_link_libraries(efx_order PUBLIC efx_core efx_pricing)

# Hedger library
add_library(efx_hedger
    hedger/src/auto_hedger.cpp
)
target_include_directories(efx_hedger PUBLIC hedger/include)
target_link_libraries(efx_hedger PUBLIC efx_core efx_order efx_market_data)

# Gateway library (ZMQ publisher + QuestDB ILP writer)
add_library(efx_gateway
    gateway/src/zmq_publisher.cpp
    gateway/src/questdb_writer.cpp
)
target_include_directories(efx_gateway PUBLIC gateway/include)
target_link_libraries(efx_gateway PUBLIC efx_core cppzmq PkgConfig::ZeroMQ)

# Main engine executable
add_executable(efx-engine app/main.cpp)
target_link_libraries(efx-engine PRIVATE
    efx_core
    efx_market_data
    efx_pricing
    efx_order
    efx_hedger
    efx_gateway
    Boost::system
)

# Tests
enable_testing()
add_executable(efx_tests
    tests/test_types.cpp
    tests/test_fair_value.cpp
    tests/test_spread.cpp
)
target_link_libraries(efx_tests PRIVATE
    efx_core
    efx_market_data
    efx_pricing
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(efx_tests)
