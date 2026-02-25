#include <grpcpp/grpcpp.h>
#include "rpc_server.h"
#include "node.h"
#include <iostream>

void RunServer(const std::string &address,
               const std::vector<std::string> &peers)
{
    Node node("wal_" + address + ".log",
              peers);

    node.recover();
    node.start();

    KVServiceImpl kv_service(&node);
    ReplicationServiceImpl replication_service(&node);
    ElectionServiceImpl election_service(&node);

    grpc::ServerBuilder builder;

    builder.AddListeningPort(address,
                             grpc::InsecureServerCredentials());

    builder.RegisterService(&kv_service);
    builder.RegisterService(&replication_service);
    builder.RegisterService(&election_service);

    std::unique_ptr<grpc::Server> server(
        builder.BuildAndStart());

    std::cout << "Server running at "
              << address << "\n";

    server->Wait();
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: ./server <port>\n";
        return 1;
    }

    std::string port = argv[1];

    std::string address = "0.0.0.0:" + port;

    std::vector<std::string> peers = {
        "localhost:50051",
        "localhost:50052",
        "localhost:50053"};

    RunServer(address, peers);

    return 0;
}