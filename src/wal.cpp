#include "wal.h"
#include <fstream>

WriteAheadLog::WriteAheadLog(const std::string &file)
    : file_(file) {}

void WriteAheadLog::append(const Operation &op)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::ofstream out(file_, std::ios::app);
    out << op.index << "|" << op.key << "|" << op.value << "\n";
}

std::vector<Operation> WriteAheadLog::replay()
{
    std::vector<Operation> ops;
    std::ifstream in(file_);
    std::string line;

    while (std::getline(in, line))
    {
        size_t p1 = line.find("|");
        size_t p2 = line.find("|", p1 + 1);

        Operation op;
        op.index = std::stoll(line.substr(0, p1));
        op.key = line.substr(p1 + 1, p2 - p1 - 1);
        op.value = line.substr(p2 + 1);
        ops.push_back(op);
    }

    return ops;
}