#pragma once
#include "Event.h"
#include <string>
#include <vector>
#include <cstdint>

namespace hac { namespace reporting {

class EventQueue {
public:
    explicit EventQueue(const std::wstring& db_path,
                        uint64_t max_bytes = 10ULL * 1024 * 1024);
    ~EventQueue();

    bool Push(const AntiCheatEvent& ev);

    // Returns up to |count| rows as (rowid, json) pairs, oldest first.
    std::vector<std::pair<int64_t, std::string>> Peek(int count = 32);

    void Remove(int64_t rowid);

    uint64_t ByteSize() const;

private:
    struct Impl;
    Impl* m_impl;
};

}} // namespace hac::reporting
