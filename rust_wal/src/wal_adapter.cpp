#include "wal_adapter.h"

WALAdapter::WALAdapter(const std::string &file)
    : file_(file)
{
    wal_open(file_.c_str());
    cache_ = replay(); // load existing WAL into memory
}

void WALAdapter::append(const Operation &op)
{
    wal_append(
        op.index,
        op.term,
        (const uint8_t *)op.key.data(),
        op.key.size(),
        (const uint8_t *)op.value.data(),
        op.value.size());

    cache_.push_back(op);
}

std::vector<Operation> WALAdapter::replay()
{
    std::vector<Operation> out;

    uint64_t n = wal_count();

    for (uint64_t i = 0; i < n; i++)
    {
        WalEntry e;
        wal_read(i, &e);

        Operation op;
        op.index = e.index;
        op.term = e.term;
        op.key.assign((char *)e.key_ptr, e.key_len);
        op.value.assign((char *)e.val_ptr, e.val_len);

        out.push_back(op);
    }
    return out;
}