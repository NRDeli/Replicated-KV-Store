cmake_minimum_required(VERSION 3.16)
project(replicated_kv)

set(CMAKE_CXX_STANDARD 17)

find_package(gRPC REQUIRED CONFIG)

add_executable(server
    src/main.cpp
    src/node.cpp
    src/kv_store.cpp
    src/wal.cpp
)

target_link_libraries(server
    gRPC::grpc++
)