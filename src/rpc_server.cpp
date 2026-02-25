#include "rpc_server.h"

KVServiceImpl::KVServiceImpl(Node *node)
    : node_(node) {}

grpc::Status KVServiceImpl::Put(grpc::ServerContext *,
                                const kv::PutRequest *request,
                                kv::PutResponse *response)
{

    bool success = node_->put(request->key(), request->value());

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