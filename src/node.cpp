#include "node.h"

Node::Node(const std::string &wal_file)
    : wal_(wal_file), last_index_(0) {}

void Node::put(const std::string &key, const std::string &value)
{
    int64_t idx = ++last_index_;

    Operation op{idx, key, value};
    wal_.append(op);

    store_.put(key, value);
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
}