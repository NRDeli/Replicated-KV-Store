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

    int requestVotes(int64_t term,
                     int64_t candidate_id,
                     int64_t last_log_index);

private:
    std::vector<std::unique_ptr<kv::ReplicationService::Stub>> replication_stubs_;
    std::vector<std::unique_ptr<kv::ElectionService::Stub>> election_stubs_;
};