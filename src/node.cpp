#include "node.h"

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

bool Node::put(const std::string &key, const std::string &value)
{
    if (role_ != Role::LEADER)
    {
        return false;
    }

    int64_t idx = ++last_index_;
    Operation op{idx, key, value};

    wal_.append(op);

    // Stage 2: apply immediately (no quorum yet)
    store_.put(key, value);
    commit_index_ = idx;

    return true;
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