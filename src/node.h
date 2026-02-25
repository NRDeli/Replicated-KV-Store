#pragma once
#include "kv_store.h"
#include "wal.h"
#include <atomic>

class Node
{
public:
    Node(const std::string &wal_file);

    void put(const std::string &key, const std::string &value);
    bool get(const std::string &key, std::string &value);

    void recover();

private:
    KVStore store_;
    WriteAheadLog wal_;
    std::atomic<int64_t> last_index_;
};