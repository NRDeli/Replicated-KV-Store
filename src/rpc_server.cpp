#include "rpc_server.h"
#include <iostream>

/* ===============================
   KV SERVICE
=================================*/

KVServiceImpl::KVServiceImpl(Node *node)
    : node_(node) {}

grpc::Status KVServiceImpl::Put(
    grpc::ServerContext *,
    const kv::PutRequest *request,
    kv::PutResponse *response)
{
    if (node_->role() != Role::LEADER)
    {
        response->set_success(false);
        response->set_leader_target("UNKNOWN"); // No fixed leader anymore
        return grpc::Status::OK;
    }

    bool success = node_->replicateAndCommit(
        request->key(),
        request->value());

    response->set_success(success);

    return grpc::Status::OK;
}

grpc::Status KVServiceImpl::Get(
    grpc::ServerContext *,
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

/* ===============================
   REPLICATION SERVICE
=================================*/

ReplicationServiceImpl::ReplicationServiceImpl(Node *node)
    : node_(node) {}

grpc::Status ReplicationServiceImpl::Replicate(
    grpc::ServerContext *,
    const kv::ReplicationPacket *request,
    kv::ReplicationAck *response)
{
    // Heartbeat
    if (request->ops_size() == 0)
    {
        node_->receiveHeartbeat(request->term());
        response->set_success(true);
        response->set_term(node_->currentTerm());
        return grpc::Status::OK;
    }

    // If leader term is outdated
    if (request->term() < node_->currentTerm())
    {
        response->set_success(false);
        response->set_term(node_->currentTerm());
        return grpc::Status::OK;
    }

    node_->updateTerm(request->term());

    for (const auto &op : request->ops())
    {
        if (op.index() != node_->lastIndex() + 1)
        {
            response->set_success(false);
            response->set_term(node_->currentTerm());
            return grpc::Status::OK;
        }

        Operation local_op;
        local_op.index = op.index();
        local_op.term = op.term();
        local_op.key = op.key();
        local_op.value = op.value();

        node_->appendFromLeader(local_op);
    }

    node_->setCommitIndex(request->commit_index());
    node_->applyUpTo(request->commit_index());

    response->set_success(true);
    response->set_last_index(node_->lastIndex());
    response->set_term(node_->currentTerm());

    return grpc::Status::OK;
}

/* ===============================
   ELECTION SERVICE
=================================*/

ElectionServiceImpl::ElectionServiceImpl(Node *node)
    : node_(node) {}

grpc::Status ElectionServiceImpl::RequestVote(
    grpc::ServerContext *,
    const kv::VoteRequest *request,
    kv::VoteResponse *response)
{
    node_->updateTerm(request->term());

    bool granted = node_->requestVote(
        request->term(),
        request->candidate_id(),
        request->last_log_index());

    response->set_term(node_->currentTerm());
    response->set_vote_granted(granted);

    return grpc::Status::OK;
}