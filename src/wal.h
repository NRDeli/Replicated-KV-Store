#pragma once
#include <string>
#include <vector>
#include <mutex>

struct Operation
{
    int64_t index;
    std::string key;
    std::string value;
};

class WriteAheadLog
{
public:
    WriteAheadLog(const std::string &file);
    void append(const Operation &op);
    std::vector<Operation> replay();

private:
    std::string file_;
    std::mutex mutex_;
};