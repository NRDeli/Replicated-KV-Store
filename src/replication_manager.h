#pragma once
#include <grpcpp/grpcpp.h>
#include "kv.grpc.pb.h"
#include <vector>
#include <string>

class ReplicationManager
{
public:
    ReplicationManager(const std::vector<std::string> &peers);

    int replicate(const kv::Operation &op);

private:
    std::vector<std::unique_ptr<kv::ReplicationService::Stub>> stubs_;
};