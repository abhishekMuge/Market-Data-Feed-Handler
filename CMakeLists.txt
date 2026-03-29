cmake_minimum_required(VERSION 3.10)
project(HFT_Exchange)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -Wall -Wextra -pthread")

# Include paths
include_directories(includes)
include_directories(includes/common)
include_directories(includes/client)
include_directories(src/common)

find_package(Threads REQUIRED)


# Server Library
add_library(exchange_server STATIC 
    src/server/tick_generator.cpp 
    src/server/client_manager.cpp 
    src/server/exchange_simulator.cpp
)

# Main Executable
add_executable(exchange_sim src/server/main.cpp)
target_link_libraries(exchange_sim 
    exchange_server 
    Threads::Threads
    rt
)

add_library(feed_handler_lib STATIC 
    src/client/market_data_socket.cpp
    src/client/binary_parser.cpp
    src/client/symbol_cache.cpp
    includes/common/latency_tracker.cpp
)

# Building the main client orchestrator
add_executable(feed_processor src/client/main_client.cpp)
target_link_libraries(feed_processor 
    feed_handler_lib 
    Threads::Threads
    rt
)

# # Unit Tests (requires GTest installed)
# enable_testing()
# add_executable(unit_tests tests/server_test.cpp)
# target_link_libraries(unit_tests exchange_server gtest gtest_main pthread)
# add_test(NAME ServerUnitTests COMMAND unit_tests)

set_target_properties(exchange_sim PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler flags: ${CMAKE_CXX_FLAGS}")