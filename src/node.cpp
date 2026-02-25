#include "node.h"
#include "replication_manager.h"

Node::Node(const std::string &wal_file,
           Role role,
           const std::string &leader_addr,
           const std::vector<std::string> &peers)
    : wal_(wal_file),
      role_(role),
      leader_addr_(leader_addr),
      peers_(peers),
      last_index_(0),
      commit_index_(0) {}

bool Node::replicateAndCommit(const std::string &key,
                              const std::string &value)
{

    if (role_ != Role::LEADER)
        return false;

    int64_t idx = ++last_index_;

    Operation local_op{idx, key, value};
    wal_.append(local_op);

    kv::Operation proto_op;
    proto_op.set_index(idx);
    proto_op.set_key(key);
    proto_op.set_value(value);

    ReplicationManager manager(peers_);

    int success = manager.replicate(proto_op);

    int majority = (peers_.size() + 1) / 2 + 1;

    if (success >= majority)
    {

        store_.put(key, value);
        commit_index_ = idx;

        return true;
    }

    return false;
}

bool Node::get(const std::string &key, std::string &value)
{
    return store_.get(key, value);
}

void Node::recover()
{
    auto ops = wal_.replay();

    for (const auto &op : ops)
    {
        store_.put(op.key, op.value);
        last_index_ = op.index;
    }

    commit_index_.store(last_index_.load());
}

void Node::appendFromLeader(const Operation &op)
{
    wal_.append(op);
    store_.put(op.key, op.value);
    last_index_ = op.index;
}