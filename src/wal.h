#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>

struct Operation
{
    int64_t index;
    int64_t term;
    std::string key;
    std::string value;
};

class WriteAheadLog
{
public:
    WriteAheadLog(const std::string &file);

    void append(const Operation &op);
    std::vector<Operation> replay();

    const std::vector<Operation> &inMemoryLog() const
    {
        return log_;
    }

private:
    std::string file_;
    std::mutex mutex_;

    std::vector<Operation> log_; // OPTIMIZATION
};