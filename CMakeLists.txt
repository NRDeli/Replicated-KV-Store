cmake_minimum_required(VERSION 3.16)
project(replicated_kv)

set(CMAKE_CXX_STANDARD 17)

find_package(gRPC REQUIRED CONFIG)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/rust_wal/src)

add_executable(server
    src/main.cpp
    src/node.cpp
    src/kv_store.cpp
    rust_wal/src/wal_adapter.cpp
    src/rpc_server.cpp
    src/replication_manager.cpp
    src/kv.pb.cc
    src/kv.grpc.pb.cc
)

# ---- Build Rust WAL ----
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/rust_wal/target/release/libreplicated_wal.a
    COMMAND cargo build --release
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/rust_wal
)

add_custom_target(rust_wal ALL
    DEPENDS ${CMAKE_SOURCE_DIR}/rust_wal/target/release/libreplicated_wal.a
)

add_dependencies(server rust_wal)

# ---- Link libraries ----
target_link_libraries(server
    gRPC::grpc++
    ${CMAKE_SOURCE_DIR}/rust_wal/target/release/libreplicated_wal.a
)