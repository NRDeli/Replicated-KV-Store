#include "replication_manager.h"

ReplicationManager::ReplicationManager(const std::vector<std::string> &peers)
{

    for (const auto &peer : peers)
    {

        auto channel = grpc::CreateChannel(peer,
                                           grpc::InsecureChannelCredentials());

        stubs_.push_back(
            kv::ReplicationService::NewStub(channel));
    }
}

int ReplicationManager::replicate(const kv::Operation &op,
                                  int64_t commit_index)
{

    int success_count = 1; // leader counts itself

    for (auto &stub : stubs_)
    {

        kv::ReplicationPacket packet;

        packet.set_from_index(op.index());
        packet.set_commit_index(commit_index);

        *packet.add_ops() = op;

        kv::ReplicationAck ack;
        grpc::ClientContext context;

        grpc::Status status = stub->Replicate(&context, packet, &ack);

        if (status.ok() && ack.success())
        {
            success_count++;
        }
    }

    return success_count;
}