#pragma once
#include "kv_store.h"
#include "wal.h"
#include <atomic>
#include <vector>
#include <string>

enum class Role
{
    LEADER,
    FOLLOWER
};

class Node
{
public:
    Node(const std::string &wal_file,
         Role role,
         const std::string &leader_addr,
         const std::vector<std::string> &peers);

    bool put(const std::string &key, const std::string &value);
    bool get(const std::string &key, std::string &value);

    void recover();

    Role role() const { return role_; }
    std::string leaderAddress() const { return leader_addr_; }

private:
    KVStore store_;
    WriteAheadLog wal_;

    Role role_;
    std::string leader_addr_;
    std::vector<std::string> peers_;

    std::atomic<int64_t> last_index_;
    std::atomic<int64_t> commit_index_;
};