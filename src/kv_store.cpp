#include "kv_store.h"

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