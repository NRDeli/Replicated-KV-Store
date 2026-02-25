#include "node.h"
#include "replication_manager.h"
#include <iostream>

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
      last_applied_(0),
      current_term_(0),
      voted_for_(-1) {}

void Node::updateTerm(int64_t term)
{
    if (term > current_term_)
    {
        current_term_ = term;
        role_ = Role::FOLLOWER;
        voted_for_ = -1;
    }
}

bool Node::requestVote(int64_t term,
                       int64_t candidate_id,
                       int64_t last_log_index)
{

    std::lock_guard<std::mutex> lock(election_mutex_);

    if (term < current_term_)
        return false;

    if (voted_for_ == -1 || voted_for_ == candidate_id)
    {
        voted_for_ = candidate_id;
        current_term_ = term;
        return true;
    }

    return false;
}

void Node::startElection()
{

    role_ = Role::CANDIDATE;
    current_term_++;
    voted_for_ = 0; // self

    int votes = 1;

    // Simplified local vote counting (stub)
    if (votes > peers_.size() / 2)
    {
        role_ = Role::LEADER;
        std::cout << "Became leader in term "
                  << current_term_.load() << "\n";
    }
}

bool Node::replicateAndCommit(const std::string &key,
                              const std::string &value)
{
    if (role_ != Role::LEADER)
        return false;

    int64_t idx = ++last_index_;

    Operation local_op{idx,
                       current_term_.load(),
                       key,
                       value};

    wal_.append(local_op);

    kv::Operation proto_op;
    proto_op.set_index(idx);
    proto_op.set_term(current_term_.load());
    proto_op.set_key(key);
    proto_op.set_value(value);

    ReplicationManager manager(peers_);

    int success = manager.replicate(proto_op,
                                    commit_index_.load());

    int majority = (peers_.size() + 1) / 2 + 1;

    if (success >= majority)
    {
        commit_index_.store(idx);
        applyUpTo(commit_index_.load());
        manager.replicate(proto_op,
                          commit_index_.load());
        return true;
    }

    return false;
}

void Node::appendFromLeader(const Operation &op)
{
    if (op.term < current_term_)
        return;

    wal_.append(op);
    last_index_.store(op.index);
}

void Node::applyUpTo(int64_t commit_index)
{
    const auto &log = wal_.inMemoryLog();

    while (last_applied_.load() < commit_index)
    {
        int64_t next = last_applied_.load();

        if (next >= log.size())
            break;

        const auto &op = log[next];

        store_.put(op.key, op.value);
        last_applied_++;
    }
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