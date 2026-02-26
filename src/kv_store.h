#pragma once
#include <unordered_map>
#include <string>
#include <mutex>

class KVStore
{
public:
    void put(const std::string &key, const std::string &value);
    bool get(const std::string &key, std::string &value);

    // Snapshot support
    std::string serialize() const;
    void deserialize(const std::string &data);

private:
    std::unordered_map<std::string, std::string> store_;
    mutable std::mutex mutex_;
};