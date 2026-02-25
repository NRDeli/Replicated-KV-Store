cmake_minimum_required(VERSION 3.16)
project(replicated_kv)

set(CMAKE_CXX_STANDARD 17)

find_package(gRPC REQUIRED CONFIG)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(server
    src/main.cpp
    src/node.cpp
    src/kv_store.cpp
    src/wal.cpp
    src/rpc_server.cpp
    src/replication_manager.cpp
    src/kv.pb.cc
    src/kv.grpc.pb.cc
)

target_link_libraries(server
    gRPC::grpc++
)