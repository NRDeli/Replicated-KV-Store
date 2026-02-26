#include "kv_store.h"
#include <sstream>

void KVStore::put(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    store_[key] = value;
}

bool KVStore::get(const std::string &key, std::string &value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end())
        return false;

    value = it->second;
    return true;
}

std::string KVStore::serialize() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::string out;
    for (const auto &kv : store_)
    {
        out += kv.first;
        out.push_back('=');
        out += kv.second;
        out.push_back('\n');
    }
    return out;
}

void KVStore::deserialize(const std::string &data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    store_.clear();

    std::stringstream ss(data);
    std::string line;

    while (std::getline(ss, line))
    {
        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        store_[line.substr(0, pos)] = line.substr(pos + 1);
    }
}