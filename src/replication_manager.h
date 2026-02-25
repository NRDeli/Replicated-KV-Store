#pragma once
#include <grpcpp/grpcpp.h>
#include "kv.grpc.pb.h"
#include <vector>
#include <string>
#include <memory>

class ReplicationManager
{
public:
    ReplicationManager(const std::vector<std::string> &peers);

    int replicate(const kv::Operation &op,
                  int64_t commit_index);

private:
    std::vector<std::unique_ptr<kv::ReplicationService::Stub>> stubs_;
};