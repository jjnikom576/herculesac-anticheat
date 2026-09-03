#pragma once
#include "Event.h"
#include <string>
#include <windows.h>

namespace hac { namespace reporting {

struct ReporterConfig {
    std::wstring endpoint_url;  // https://host/v1/events  (empty = local-only)
    std::wstring db_path;       // path for SQLite queue
    std::string  client_id;
    std::string  session_id;
    std::string  game_id;
};

class Reporter {
public:
    Reporter();
    ~Reporter();

    void Configure(const ReporterConfig& cfg);

    // Thread-safe: enqueues the event and wakes the background sender.
    void Emit(AntiCheatEvent ev);

    // Blocks until the queue drains or |timeout_ms| elapses.
    void Flush(DWORD timeout_ms = 5000);

private:
    struct Impl;
    Impl* m_impl;
};

}} // namespace hac::reporting
