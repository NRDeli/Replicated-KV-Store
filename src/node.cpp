#include "node.h"
#include "replication_manager.h"
#include <iostream>
#include <random>

Node::Node(const std::string &wal_file,
           const std::vector<std::string> &peers)
    : wal_(std::make_unique<WALAdapter>(wal_file)),
      peers_(peers),
      last_index_(0),
      commit_index_(0),
      last_applied_(0),
      current_term_(0),
      voted_for_(-1),
      role_(Role::FOLLOWER),
      running_(true),
      last_heartbeat_time_(std::chrono::steady_clock::now().time_since_epoch().count()),
      elections_total_(0),
      replication_failures_total_(0)
{
    nextIndex_.resize(peers.size(), 1);
    matchIndex_.resize(peers.size(), 0);
}

void Node::start()
{
    std::thread(&Node::electionLoop, this).detach();
}

void Node::recover()
{
    std::string snap;
    uint64_t snapIndex;

    if (wal_->loadSnapshot(snap, snapIndex))
    {
        store_.deserialize(snap);
        last_index_ = snapIndex;
        commit_index_ = snapIndex;
        last_applied_ = snapIndex;
    }

    auto ops = wal_->replay();

    for (const auto &op : ops)
    {
        store_.put(op.key, op.value);
        last_index_ = op.index;
    }

    commit_index_.store(last_index_.load());
    last_applied_.store(commit_index_.load());
}

bool Node::get(const std::string &key,
               std::string &value)
{
    return store_.get(key, value);
}

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

void Node::receiveHeartbeat(int64_t term)
{
    if (term >= current_term_)
    {
        role_ = Role::FOLLOWER;
        current_term_ = term;
        last_heartbeat_time_ =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();
    }
}

void Node::appendFromLeader(const Operation &op)
{
    if (op.term < current_term_)
        return;

    // Raft conflict repair:
    if (op.index <= wal_->lastIndex())
    {
        wal_->truncateFrom(op.index - 1);
    }

    wal_->append(op);
    last_index_.store(op.index);
}

void Node::applyUpTo(int64_t commit_index)
{
    const auto &log = wal_->inMemoryLog();

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

void Node::createSnapshot()
{
    std::string serialized = store_.serialize();
    wal_->createSnapshot(serialized, commit_index_.load());
}

void Node::installSnapshot(const std::string &data,
                           uint64_t lastIndex,
                           uint64_t lastTerm)
{
    // Replace KV state
    store_.deserialize(data);

    // Replace WAL below snapshot
    wal_->createSnapshot(data, lastIndex);

    last_index_ = lastIndex;
    commit_index_ = lastIndex;
    last_applied_ = lastIndex;

    current_term_ = lastTerm;
}

/* ============================
   RAFT BACKTRACKING SECTION
============================= */

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

    wal_->append(local_op);

    for (size_t i = 0; i < peers_.size(); ++i)
        replicateToFollower(i);

    updateCommitIndex();

    if (commit_index_.load() >= idx)
    {
        applyUpTo(commit_index_.load());
        if (wal_->inMemoryLog().size() > 1000)
        {
            createSnapshot();
        }
        return true;
    }

    return false;
}

bool Node::replicateToFollower(int followerIndex)
{
    ReplicationManager manager({peers_[followerIndex]});

    const auto &log = wal_->inMemoryLog();
    int64_t nextIdx = nextIndex_[followerIndex];
    // follower behind snapshot?
    uint64_t snapIndex;
    std::string snapData;

    if (wal_->loadSnapshot(snapData, snapIndex))
    {
        if (nextIdx <= snapIndex)
        {
            ReplicationManager mgr({peers_[followerIndex]});
            bool ok = mgr.sendSnapshot(
                peers_[followerIndex],
                snapData,
                snapIndex,
                current_term_.load());
            if (ok)
            {
                nextIndex_[followerIndex] = snapIndex + 1;
                matchIndex_[followerIndex] = snapIndex;
                return true;
            }
            return false;
        }
    }

    kv::Operation proto_op;
    proto_op.set_index(log[nextIdx - 1].index);
    proto_op.set_term(log[nextIdx - 1].term);
    proto_op.set_key(log[nextIdx - 1].key);
    proto_op.set_value(log[nextIdx - 1].value);

    int success = manager.replicate(proto_op,
                                    commit_index_.load());

    if (success > 0)
    {
        matchIndex_[followerIndex] = nextIdx;
        nextIndex_[followerIndex] = nextIdx + 1;
        return true;
    }
    else
    {
        replication_failures_total_++;
        if (nextIndex_[followerIndex] > 1)
            nextIndex_[followerIndex]--;
        return false;
    }
}

void Node::updateCommitIndex()
{
    for (int64_t N = last_index_.load();
         N > commit_index_.load();
         --N)
    {
        int count = 1;

        for (size_t i = 0; i < matchIndex_.size(); ++i)
        {
            if (matchIndex_[i] >= N)
                count++;
        }

        if (count > peers_.size() / 2)
        {
            commit_index_.store(N);
            break;
        }
    }
}

/* ============================
   ELECTION LOOP
============================= */

void Node::electionLoop()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> timeout_dist(150, 300);

    while (running_)
    {
        int timeout_ms = timeout_dist(gen);

        std::this_thread::sleep_for(
            std::chrono::milliseconds(timeout_ms));

        int64_t now =
            std::chrono::steady_clock::now()
                .time_since_epoch()
                .count();

        if (role_ == Role::LEADER)
            continue;

        if (now - last_heartbeat_time_.load() > timeout_ms * 1'000'000)
        {
            startElection();
        }
    }
}

void Node::startElection()
{
    std::lock_guard<std::mutex> lock(election_mutex_);

    role_ = Role::CANDIDATE;
    current_term_++;
    voted_for_ = 0;
    elections_total_++;
    ReplicationManager manager(peers_);

    int votes = manager.requestVotes(
        current_term_.load(),
        0,
        last_index_.load());

    if (votes == -1)
    {
        role_ = Role::FOLLOWER;
        return;
    }

    int majority = (peers_.size() + 1) / 2 + 1;

    if (votes >= majority)
    {
        role_ = Role::LEADER;
        sendHeartbeats();
    }
    else
    {
        role_ = Role::FOLLOWER;
    }
}

void Node::sendHeartbeats()
{
    ReplicationManager manager(peers_);

    kv::Operation empty_op;

    manager.replicate(empty_op,
                      commit_index_.load());
}

std::string Node::metrics()
{
    std::string output;

    output += "raft_role ";
    output += std::to_string(static_cast<int>(role_.load()));
    output += "\n";

    output += "raft_term ";
    output += std::to_string(current_term_.load());
    output += "\n";

    output += "raft_commit_index ";
    output += std::to_string(commit_index_.load());
    output += "\n";

    output += "raft_last_applied ";
    output += std::to_string(last_applied_.load());
    output += "\n";

    output += "raft_log_size ";
    output += std::to_string(wal_->inMemoryLog().size());
    output += "\n";

    output += "raft_elections_total ";
    output += std::to_string(elections_total_.load());
    output += "\n";

    output += "raft_replication_failures_total ";
    output += std::to_string(replication_failures_total_.load());
    output += "\n";

    return output;
}