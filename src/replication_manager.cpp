#include "replication_manager.h"

ReplicationManager::ReplicationManager(
    const std::vector<std::string> &peers)
{
    for (const auto &peer : peers)
    {
        auto channel = grpc::CreateChannel(
            peer,
            grpc::InsecureChannelCredentials());

        replication_stubs_.push_back(
            kv::ReplicationService::NewStub(channel));

        election_stubs_.push_back(
            kv::ElectionService::NewStub(channel));
    }
}

int ReplicationManager::replicate(
    const kv::Operation &op,
    int64_t commit_index)
{
    int success_count = 0;

    for (auto &stub : replication_stubs_)
    {
        kv::ReplicationPacket packet;

        packet.set_commit_index(commit_index);

        if (op.index() != 0)
        {
            packet.set_from_index(op.index());
            *packet.add_ops() = op;
        }

        kv::ReplicationAck ack;
        grpc::ClientContext context;

        grpc::Status status =
            stub->Replicate(&context, packet, &ack);

        if (status.ok() && ack.success())
            success_count++;
    }

    return success_count;
}

int ReplicationManager::requestVotes(
    int64_t term,
    int64_t candidate_id,
    int64_t last_log_index)
{
    int votes = 1; // self vote

    for (auto &stub : election_stubs_)
    {
        kv::VoteRequest request;
        request.set_term(term);
        request.set_candidate_id(candidate_id);
        request.set_last_log_index(last_log_index);

        kv::VoteResponse response;
        grpc::ClientContext context;

        grpc::Status status =
            stub->RequestVote(&context, request, &response);

        if (!status.ok())
            continue;

        if (response.term() > term)
        {
            // higher term detected
            return -1;
        }

        if (response.vote_granted())
            votes++;
    }

    return votes;
}

bool ReplicationManager::sendSnapshot(
    const std::string &peer,
    const std::string &data,
    uint64_t lastIndex,
    uint64_t lastTerm)
{
    auto channel = grpc::CreateChannel(
        peer, grpc::InsecureChannelCredentials());

    std::unique_ptr<kv::ReplicationService::Stub> stub =
        kv::ReplicationService::NewStub(channel);

    kv::InstallSnapshotRequest req;
    kv::InstallSnapshotResponse resp;

    req.set_data(data);
    req.set_last_index(lastIndex);
    req.set_last_term(lastTerm);

    grpc::ClientContext ctx;

    auto status = stub->InstallSnapshot(&ctx, req, &resp);

    if (!status.ok())
        return false;

    return resp.success();
}