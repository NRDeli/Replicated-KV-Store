#include <grpcpp/grpcpp.h>
#include "rpc_server.h"
#include "node.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

void StartMetricsServer(Node *node, int port)
{
    std::thread([node, port]()
                {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0) {
            perror("socket failed");
            return;
        }

        int opt = 1;
        if (setsockopt(server_fd, SOL_SOCKET,
                       SO_REUSEADDR,
                       &opt, sizeof(opt)) < 0) {
                        
            perror("setsockopt failed");
            close(server_fd);
            return;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd,
                 (struct sockaddr *)&address,
                 sizeof(address)) < 0) {
            perror("bind failed");
            close(server_fd);
            return;
        }

        if (listen(server_fd, 10) < 0) {
            perror("listen failed");
            close(server_fd);
            return;
        }

        std::cout << "Metrics server running at http://localhost:"
                  << port << std::endl;

        while (true)
        {
            int client_socket =
                accept(server_fd, nullptr, nullptr);

            if (client_socket < 0)
                continue;

            std::string body = node->metrics();

            std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n"
                "Connection: close\r\n\r\n" +
                body;

            send(client_socket,
                 response.c_str(),
                 response.size(),
                 0);

            shutdown(client_socket, SHUT_RDWR);
            close(client_socket);
        } })
        .detach();
}

void RunServer(const std::string &address,
               const std::vector<std::string> &peers)
{
    Node node("wal_" + address + ".log",
              peers);

    node.recover();
    node.start();

    int metrics_port = std::stoi(address.substr(address.find(":") + 1)) + 1000;
    StartMetricsServer(&node, metrics_port);

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