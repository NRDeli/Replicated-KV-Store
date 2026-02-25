#pragma once
#include <grpcpp/grpcpp.h>
#include "kv.grpc.pb.h"
#include "node.h"

class KVServiceImpl final : public kv::KVService::Service
{
public:
    KVServiceImpl(Node *node);

    grpc::Status Put(grpc::ServerContext *context,
                     const kv::PutRequest *request,
                     kv::PutResponse *response) override;

    grpc::Status Get(grpc::ServerContext *context,
                     const kv::GetRequest *request,
                     kv::GetResponse *response) override;

private:
    Node *node_;
};

class ReplicationServiceImpl final : public kv::ReplicationService::Service
{
public:
    ReplicationServiceImpl(Node *node);

    grpc::Status Replicate(grpc::ServerContext *context,
                           const kv::ReplicationPacket *request,
                           kv::ReplicationAck *response) override;

private:
    Node *node_;
};