#pragma once
#include "kv_store.h"
#include "wal.h"
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>

enum class Role
{
    FOLLOWER,
    CANDIDATE,
    LEADER
};

class Node
{
public:
    Node(const std::string &wal_file,
         const std::vector<std::string> &peers);

    void start();

    bool replicateAndCommit(const std::string &key,
                            const std::string &value);

    bool get(const std::string &key, std::string &value);

    void recover();

    void appendFromLeader(const Operation &op);

    void applyUpTo(int64_t commit_index);

    void setCommitIndex(int64_t idx)
    {
        commit_index_.store(idx);
    }

    int64_t lastIndex() const
    {
        return last_index_.load();
    }

    int64_t currentTerm() const
    {
        return current_term_.load();
    }

    Role role() const { return role_; }

    void updateTerm(int64_t term);

    bool requestVote(int64_t term,
                     int64_t candidate_id,
                     int64_t last_log_index);

    void receiveHeartbeat(int64_t term);

private:
    void electionLoop();
    void startElection();
    void sendHeartbeats();

    KVStore store_;
    WriteAheadLog wal_;

    std::vector<std::string> peers_;

    std::atomic<int64_t> last_index_;
    std::atomic<int64_t> commit_index_;
    std::atomic<int64_t> last_applied_;

    std::atomic<int64_t> current_term_;
    int64_t voted_for_;

    std::atomic<Role> role_;

    std::atomic<bool> running_;
    std::atomic<int64_t> last_heartbeat_time_;

    std::mutex election_mutex_;
};