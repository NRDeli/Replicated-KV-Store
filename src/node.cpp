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
      commit_index_(0),
      last_applied_(0) {}

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

    // Phase 1: replicate append
    int success = manager.replicate(proto_op,
                                    commit_index_.load());

    int majority = (peers_.size() + 1) / 2 + 1;

    if (success >= majority)
    {
        commit_index_.store(idx);

        // Apply locally after commit
        applyUpTo(commit_index_.load());

        // Phase 2: propagate commit index
        manager.replicate(proto_op,
                          commit_index_.load());

        return true;
    }

    return false;
}

bool Node::get(const std::string &key,
               std::string &value)
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
    last_applied_.store(commit_index_.load());
}

void Node::appendFromLeader(const Operation &op)
{
    wal_.append(op);
    last_index_.store(op.index);
}

void Node::applyUpTo(int64_t commit_index)
{
    while (last_applied_.load() < commit_index)
    {
        int64_t next = last_applied_.load() + 1;

        auto ops = wal_.replay(); // simple but correct

        for (const auto &op : ops)
        {
            if (op.index == next)
            {
                store_.put(op.key, op.value);
                last_applied_.store(next);
                break;
            }
        }
    }
}