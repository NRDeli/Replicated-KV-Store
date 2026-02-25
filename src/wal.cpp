#include "wal.h"
#include <fstream>

WriteAheadLog::WriteAheadLog(const std::string &file)
    : file_(file)
{

    // Load existing log once
    std::ifstream in(file_);
    std::string line;

    while (std::getline(in, line))
    {

        size_t p1 = line.find("|");
        size_t p2 = line.find("|", p1 + 1);
        size_t p3 = line.find("|", p2 + 1);

        Operation op;
        op.index = std::stoll(line.substr(0, p1));
        op.term = std::stoll(line.substr(p1 + 1, p2 - p1 - 1));
        op.key = line.substr(p2 + 1, p3 - p2 - 1);
        op.value = line.substr(p3 + 1);

        log_.push_back(op);
    }
}

void WriteAheadLog::append(const Operation &op)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::ofstream out(file_, std::ios::app);
    out << op.index << "|"
        << op.term << "|"
        << op.key << "|"
        << op.value << "\n";

    log_.push_back(op);
}

std::vector<Operation> WriteAheadLog::replay()
{
    return log_;
}