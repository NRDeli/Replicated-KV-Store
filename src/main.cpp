#include <grpcpp/grpcpp.h>
#include "rpc_server.h"
#include "node.h"
#include <iostream>

void RunServer(const std::string &address,
               Role role,
               const std::string &leader,
               const std::vector<std::string> &peers)
{

    Node node("wal_" + address + ".log", role, leader, peers);
    node.recover();

    KVServiceImpl service(&node);

    grpc::ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    std::cout << "Server running at " << address << "\n";

    server->Wait();
}

int main(int argc, char **argv)
{

    if (argc < 3)
    {
        std::cout << "Usage: ./server <port> <role>\n";
        return 1;
    }

    std::string port = argv[1];
    std::string role_str = argv[2];

    Role role = (role_str == "leader") ? Role::LEADER : Role::FOLLOWER;

    std::string address = "0.0.0.0:" + port;
    std::string leader = "localhost:50051";

    std::vector<std::string> peers = {
        "localhost:50051",
        "localhost:50052",
        "localhost:50053"};

    RunServer(address, role, leader, peers);

    return 0;
}