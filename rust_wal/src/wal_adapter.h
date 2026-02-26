#pragma once
#include "../../src/operation.h"
#include <string>
#include <vector>

extern "C"
{

    struct WalEntry
    {
        uint64_t index;
        uint64_t term;
        const uint8_t *key_ptr;
        size_t key_len;
        const uint8_t *val_ptr;
        size_t val_len;
    };

    int wal_open(const char *path);
    int wal_append(uint64_t, uint64_t,
                   const uint8_t *, size_t,
                   const uint8_t *, size_t);
    uint64_t wal_count();
    int wal_read(uint64_t, WalEntry *);
    uint64_t wal_last_index();
    int wal_truncate_from(uint64_t);
    int wal_create_snapshot(const uint8_t *, size_t, uint64_t);
    int wal_load_snapshot(const uint8_t **, size_t *, uint64_t *);
}

class WALAdapter
{
public:
    WALAdapter(const std::string &file);

    void append(const Operation &op);
    std::vector<Operation> replay();

    const std::vector<Operation> &inMemoryLog() const { return cache_; }

    uint64_t lastIndex() const;
    void truncateFrom(uint64_t index);

    void createSnapshot(const std::string &data, uint64_t lastIndex);
    bool loadSnapshot(std::string &data, uint64_t &index);

private:
    std::string file_;
    std::vector<Operation> cache_;
};