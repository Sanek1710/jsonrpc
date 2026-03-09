cmake_minimum_required(VERSION 3.10.0)
project(json-rpc VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/archive/refs/tags/v3.12.0.tar.gz
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/nlohmann_json"
  # URL_HASH "SHA256=4b92eb0c06d10683f7447ce9406cb97cd4b453be18d7279320f7b2f025c10187"
)

set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
    FetchContent_Populate(nlohmann_json)
    add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()


FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
    FetchContent_Populate(nlohmann_json)
    add_subdirectory(${nlohmann_json_SOURCE_DIR} ${nlohmann_json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_library(json-rpc STATIC
    json-rpc.cpp
    include/json-rpc/json-rpc.h
    include/json-rpc/transport.h
)

target_link_libraries(json-rpc PUBLIC nlohmann_json::nlohmann_json)

target_include_directories(json-rpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)


if(JSONRPC_BUILD_TESTS)
    message(STATUS "Building with tests...")
    add_executable(jsonrpc_tests tests/main.cpp)
    target_link_libraries(jsonrpc_tests PRIVATE json-rpc)
    
    enable_testing()
    add_test(NAME rpc_unit_tests COMMAND jsonrpc_tests)
endif()
