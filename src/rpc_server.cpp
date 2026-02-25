#include "rpc_server.h"

KVServiceImpl::KVServiceImpl(Node *node)
    : node_(node) {}

grpc::Status KVServiceImpl::Put(grpc::ServerContext *,
                                const kv::PutRequest *request,
                                kv::PutResponse *response)
{

    bool success = node_->replicateAndCommit(request->key(), request->value());

    response->set_success(success);

    if (!success)
    {
        response->set_leader_target(node_->leaderAddress());
    }

    return grpc::Status::OK;
}

grpc::Status KVServiceImpl::Get(grpc::ServerContext *,
                                const kv::GetRequest *request,
                                kv::GetResponse *response)
{

    std::string value;
    bool found = node_->get(request->key(), value);

    response->set_found(found);
    if (found)
        response->set_value(value);

    return grpc::Status::OK;
}

ReplicationServiceImpl::ReplicationServiceImpl(Node *node)
    : node_(node) {}

grpc::Status ReplicationServiceImpl::Replicate(
    grpc::ServerContext *,
    const kv::ReplicationPacket *request,
    kv::ReplicationAck *response)
{

    if (node_->role() != Role::FOLLOWER)
    {
        response->set_success(false);
        return grpc::Status::OK;
    }

    for (const auto &op : request->ops())
    {

        Operation local_op;
        local_op.index = op.index();
        local_op.key = op.key();
        local_op.value = op.value();

        node_->appendFromLeader(local_op);
    }

    response->set_success(true);
    response->set_last_index(request->ops().rbegin()->index());

    return grpc::Status::OK;
}