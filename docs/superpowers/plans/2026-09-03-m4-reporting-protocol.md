# M4 — Reporting Protocol Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver a typed, offline-queued, HTTP-delivered anti-cheat event pipeline from HerculesAC/GameMon to a backend endpoint.

**Architecture:** `AntiCheatEvent` structs serialized to JSON (JsonCpp, already vendored) are enqueued into a SQLite3 circular queue on disk; a background sender thread delivers them via WinHTTP HTTPS with exponential backoff.

**Tech Stack:** C++17, MSVC v143, WinHTTP (system DLL), SQLite3 amalgamation, JsonCpp 1.9.5 (already vendored), libsodium HMAC-SHA256 (already vendored)

## Global Constraints

- C++17, MSVC v143, `/MT` static runtime (Debug: `/MTd`)
- No new DLL dependencies except WinHTTP (ships with Windows, link `winhttp.lib`)
- SQLite amalgamation at `Common/include/sqlite3/sqlite3.{h,c}` (place before building)
- `AntiCheatEvent` JSON keys must match `docs/backend-contract.md` exactly
- All queue writes serialized via a single writer mutex; queue max 10 MB (evict oldest on overflow)
- Private keys never committed; `hac-dev.private.key` stays gitignored
- Commit after each task with `feat(m4):` prefix

---

### Task 1: Vendor SQLite3 amalgamation

**Files:**
- Create: `Common/include/sqlite3/sqlite3.h`
- Create: `Common/include/sqlite3/sqlite3.c`

- [ ] Download SQLite 3.45.2 amalgamation ZIP from https://sqlite.org/2024/sqlite-amalgamation-3450200.zip into `%TEMP%`
- [ ] Extract `sqlite3.h` → `Common/include/sqlite3/sqlite3.h`
- [ ] Extract `sqlite3.c` → `Common/include/sqlite3/sqlite3.c`
- [ ] Verify `SQLITE_VERSION` string in the header contains `3.45.`
- [ ] Commit: `chore(m4): vendor SQLite3 3.45.2 amalgamation`

---

### Task 2: AntiCheatEvent struct and enums

**Files:**
- Create: `Common/Reporting/Event.h`

- [ ] Write `Common/Reporting/Event.h`:

```cpp
#pragma once
#include <cstdint>
#include <string>

namespace hac { namespace reporting {

enum class Severity : uint8_t {
    Info     = 1,
    Warn     = 2,
    Critical = 3,
};

enum class DetectionKind : uint8_t {
    Unknown          = 0,
    ProcName         = 1,
    WinName          = 2,
    ModuleHash       = 3,
    RpmRate          = 4,
    WpmTarget        = 5,
    HookIntegrity    = 6,
    IatHook          = 7,
    EatHook          = 8,
    CodeSectionDiverge = 9,
    ThreadInject     = 10,
    KnownBadDriver   = 11,
};

struct AntiCheatEvent {
    std::string   client_id;          // HMAC(hardware_id, install_secret)
    std::string   session_id;         // UUIDv4, per game-launch
    uint64_t      timestamp_utc_ms;   // GetSystemTimeAsFileTime → Unix ms
    std::string   game_id;            // from manifest
    Severity      severity;
    DetectionKind kind;
    std::string   detector_version;   // e.g. "proc-name@1.0.0"
    std::string   evidence_json;      // kind-specific JSON object
    std::string   signature;          // HMAC-SHA256 hex over fields above
};

}} // namespace hac::reporting
```

- [ ] Commit: `feat(m4): AntiCheatEvent struct + Severity/DetectionKind enums`

---

### Task 3: EventQueue (SQLite circular queue)

**Files:**
- Create: `Common/Reporting/EventQueue.h`
- Create: `Common/Reporting/EventQueue.cpp`

- [ ] Write `Common/Reporting/EventQueue.h`:

```cpp
#pragma once
#include "Event.h"
#include <string>
#include <vector>

namespace hac { namespace reporting {

class EventQueue {
public:
    // Opens (or creates) the SQLite queue at |db_path|.
    // |max_bytes| is the soft cap before oldest rows are evicted.
    explicit EventQueue(const std::wstring& db_path,
                        uint64_t max_bytes = 10ULL * 1024 * 1024);
    ~EventQueue();

    // Thread-safe: serialized by internal mutex.
    bool Push(const AntiCheatEvent& ev);

    // Retrieve up to |count| rows without removing them.
    std::vector<std::pair<int64_t, std::string>> Peek(int count = 32);

    // Remove a row by its rowid.
    void Remove(int64_t rowid);

    // Bytes currently used by the database file.
    uint64_t ByteSize() const;

private:
    struct Impl;
    Impl* m_impl;
};

}} // namespace hac::reporting
```

- [ ] Write `Common/Reporting/EventQueue.cpp` — open/create DB, create `events` table if missing, `Push` inserts JSON blob + evicts oldest rows when `ByteSize > max_bytes`, `Peek` and `Remove` as plain SELECTs/DELETEs:

```cpp
#include "EventQueue.h"
#include "../include/sqlite3/sqlite3.h"
#include "../include/json/json.h"
#include <mutex>
#include <cstring>
#include <fcntl.h>

namespace hac { namespace reporting {

static std::string SerializeEvent(const AntiCheatEvent& ev)
{
    Json::Value root;
    root["client_id"]       = ev.client_id;
    root["session_id"]      = ev.session_id;
    root["timestamp_utc_ms"]= (Json::UInt64)ev.timestamp_utc_ms;
    root["game_id"]         = ev.game_id;
    root["severity"]        = (int)ev.severity;
    root["kind"]            = (int)ev.kind;
    root["detector_version"]= ev.detector_version;
    root["evidence_json"]   = ev.evidence_json;
    root["signature"]       = ev.signature;
    Json::FastWriter fw;
    return fw.write(root);
}

struct EventQueue::Impl {
    sqlite3*    db     = nullptr;
    uint64_t    maxBytes;
    std::mutex  mu;

    bool exec(const char* sql) {
        char* err = nullptr;
        int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
        if (err) sqlite3_free(err);
        return rc == SQLITE_OK;
    }
};

EventQueue::EventQueue(const std::wstring& db_path, uint64_t max_bytes)
    : m_impl(new Impl)
{
    m_impl->maxBytes = max_bytes;
    int rc = sqlite3_open16(db_path.c_str(), &m_impl->db);
    if (rc != SQLITE_OK) return;
    m_impl->exec("PRAGMA journal_mode=WAL;");
    m_impl->exec("PRAGMA synchronous=NORMAL;");
    m_impl->exec(
        "CREATE TABLE IF NOT EXISTS events("
        "  rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  payload TEXT NOT NULL,"
        "  added_ms INTEGER NOT NULL"
        ");");
}

EventQueue::~EventQueue() {
    if (m_impl->db) sqlite3_close(m_impl->db);
    delete m_impl;
}

bool EventQueue::Push(const AntiCheatEvent& ev)
{
    std::lock_guard<std::mutex> lk(m_impl->mu);
    if (!m_impl->db) return false;

    // evict oldest if over cap
    while (ByteSize() >= m_impl->maxBytes) {
        m_impl->exec("DELETE FROM events WHERE rowid=(SELECT MIN(rowid) FROM events);");
    }

    std::string json = SerializeEvent(ev);
    SYSTEMTIME st; GetSystemTime(&st);
    FILETIME ft; SystemTimeToFileTime(&st, &ft);
    uint64_t ms = ((uint64_t)ft.dwHighDateTime << 32 | ft.dwLowDateTime) / 10000ULL - 11644473600000ULL;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_impl->db,
        "INSERT INTO events(payload, added_ms) VALUES(?,?);",
        -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, json.c_str(), (int)json.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)ms);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<std::pair<int64_t,std::string>> EventQueue::Peek(int count)
{
    std::lock_guard<std::mutex> lk(m_impl->mu);
    std::vector<std::pair<int64_t,std::string>> out;
    if (!m_impl->db) return out;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_impl->db,
        "SELECT rowid, payload FROM events ORDER BY rowid LIMIT ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, count);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        const char* txt = (const char*)sqlite3_column_text(stmt, 1);
        out.emplace_back(id, txt ? txt : "");
    }
    sqlite3_finalize(stmt);
    return out;
}

void EventQueue::Remove(int64_t rowid)
{
    std::lock_guard<std::mutex> lk(m_impl->mu);
    if (!m_impl->db) return;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_impl->db,
        "DELETE FROM events WHERE rowid=?;", -1, &stmt, nullptr);
    sqlite3_bind_int64(stmt, 1, rowid);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

uint64_t EventQueue::ByteSize() const
{
    if (!m_impl->db) return 0;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_impl->db,
        "SELECT page_count * page_size FROM pragma_page_count(), pragma_page_size();",
        -1, &stmt, nullptr);
    uint64_t sz = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        sz = (uint64_t)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return sz;
}

}} // namespace hac::reporting
```

- [ ] Compile-check: add to the HerculesAC vcxproj temporarily to verify no errors
- [ ] Commit: `feat(m4): EventQueue — SQLite circular queue with 10 MB eviction cap`

---

### Task 4: Reporter class

**Files:**
- Create: `Common/Reporting/Reporter.h`
- Create: `Common/Reporting/Reporter.cpp`

- [ ] Write `Common/Reporting/Reporter.h`:

```cpp
#pragma once
#include "Event.h"
#include <string>

namespace hac { namespace reporting {

struct ReporterConfig {
    std::wstring endpoint_url;      // https://...
    std::wstring db_path;           // %LOCALAPPDATA%\HerculesAC\queue.db
    std::string  client_id;
    std::string  session_id;
    std::string  game_id;
};

class Reporter {
public:
    Reporter();
    ~Reporter();

    void Configure(const ReporterConfig& cfg);

    // Thread-safe: enqueues the event, wakes the background sender.
    void Emit(AntiCheatEvent ev);

    // Blocks until the queue is empty or |timeout_ms| elapses.
    void Flush(DWORD timeout_ms = 5000);

private:
    struct Impl;
    Impl* m_impl;
};

}} // namespace hac::reporting
```

- [ ] Write `Common/Reporting/Reporter.cpp` — Configure opens the EventQueue, starts a background thread that calls `SendBatch` on a loop; SendBatch calls WinHTTP to POST a JSON array of events to the endpoint; on 200-299 removes the rows; on 4xx drops the rows; on 5xx/network error waits with exponential backoff (1s, 5s, 30s, 300s, 1800s cap):

```cpp
#include "Reporter.h"
#include "EventQueue.h"
#include <windows.h>
#include <winhttp.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

#pragma comment(lib, "winhttp.lib")

namespace hac { namespace reporting {

struct Reporter::Impl {
    ReporterConfig    cfg;
    EventQueue*       queue    = nullptr;
    std::thread       worker;
    std::atomic<bool> stop    {false};
    std::mutex        wakeMu;
    std::condition_variable wakeCv;

    void WorkerLoop();
    bool SendBatch();
};

Reporter::Reporter() : m_impl(new Impl) {}

Reporter::~Reporter() {
    if (m_impl->worker.joinable()) {
        m_impl->stop = true;
        m_impl->wakeCv.notify_all();
        m_impl->worker.join();
    }
    delete m_impl->queue;
    delete m_impl;
}

void Reporter::Configure(const ReporterConfig& cfg) {
    m_impl->cfg = cfg;
    delete m_impl->queue;
    m_impl->queue = new EventQueue(cfg.db_path);
    m_impl->stop = false;
    m_impl->worker = std::thread([this]{ m_impl->WorkerLoop(); });
}

void Reporter::Emit(AntiCheatEvent ev) {
    ev.client_id  = m_impl->cfg.client_id;
    ev.session_id = m_impl->cfg.session_id;
    ev.game_id    = m_impl->cfg.game_id;
    if (!ev.timestamp_utc_ms) {
        FILETIME ft; GetSystemTimeAsFileTime(&ft);
        ev.timestamp_utc_ms =
            (((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime)
            / 10000ULL - 11644473600000ULL;
    }
    if (m_impl->queue) {
        m_impl->queue->Push(ev);
        m_impl->wakeCv.notify_one();
    }
}

void Reporter::Flush(DWORD timeout_ms) {
    // Signal the worker and wait briefly.
    m_impl->wakeCv.notify_one();
    DWORD deadline = GetTickCount() + timeout_ms;
    while (GetTickCount() < deadline) {
        if (!m_impl->queue || m_impl->queue->Peek(1).empty()) break;
        Sleep(50);
    }
}

void Reporter::Impl::WorkerLoop() {
    static const DWORD backoff[] = {1000, 5000, 30000, 300000, 1800000};
    int backoffIdx = 0;
    while (!stop) {
        {
            std::unique_lock<std::mutex> lk(wakeMu);
            wakeCv.wait_for(lk, std::chrono::milliseconds(5000),
                [this]{ return stop.load() || (queue && !queue->Peek(1).empty()); });
        }
        if (stop) break;
        if (!queue || queue->Peek(1).empty()) continue;

        bool ok = SendBatch();
        if (ok) {
            backoffIdx = 0;
        } else {
            DWORD wait = backoff[backoffIdx < 5 ? backoffIdx++ : 4];
            Sleep(wait);
        }
    }
}

bool Reporter::Impl::SendBatch() {
    if (!queue || cfg.endpoint_url.empty()) return true;
    auto rows = queue->Peek(32);
    if (rows.empty()) return true;

    // Build JSON array body
    std::string body = "[";
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i) body += ",";
        body += rows[i].second;
    }
    body += "]";

    // Parse URL
    URL_COMPONENTS uc = {};
    uc.dwStructSize    = sizeof(uc);
    WCHAR host[256]={}, path[1024]={};
    uc.lpszHostName    = host; uc.dwHostNameLength    = 256;
    uc.lpszUrlPath     = path; uc.dwUrlPathLength     = 1024;
    if (!WinHttpCrackUrl(cfg.endpoint_url.c_str(),
                         (DWORD)cfg.endpoint_url.size(), 0, &uc))
        return false;

    bool secure = uc.nScheme == INTERNET_SCHEME_HTTPS;
    HINTERNET hSession = WinHttpOpen(L"HerculesAC/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;
    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"POST", path,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        secure ? WINHTTP_FLAG_SECURE : 0);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    bool sent = WinHttpSendRequest(hReq, L"Content-Type: application/json\r\n",
        (DWORD)-1L, (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0)
        && WinHttpReceiveResponse(hReq, nullptr);

    bool success = false;
    if (sent) {
        DWORD statusCode = 0, len = sizeof(statusCode);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &len, WINHTTP_NO_HEADER_INDEX);
        if (statusCode >= 200 && statusCode < 300) {
            for (auto& r : rows) queue->Remove(r.first);
            success = true;
        } else if (statusCode >= 400 && statusCode < 500) {
            // 4xx: drop without retry
            for (auto& r : rows) queue->Remove(r.first);
            success = true;
        }
        // 5xx/other: leave rows, return false → backoff
    }
    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

}} // namespace hac::reporting
```

- [ ] Commit: `feat(m4): Reporter class — WinHTTP sender + exponential backoff`

---

### Task 5: Reporting.vcxproj (static library)

**Files:**
- Create: `Common/Reporting/Reporting.vcxproj`

- [ ] Write the vcxproj as a StaticLibrary targeting v143 / x64 / /MT(d):
  - Include dirs: `..\include\sqlite3`, `..\include\json`, `..\include\libsodium\include`, `..\`
  - `ClCompile`: `Event.cpp` (empty — header-only struct), `EventQueue.cpp`, `Reporter.cpp`
  - Also compile `sqlite3.c` as a C file (use `<CompileAs>CompileAsC</CompileAs>`)
- [ ] Add `Reporting.vcxproj` reference to `HerculesAC.vcxproj` `<ProjectReference>`
- [ ] Add `Reporting.lib` to `HerculesAC.vcxproj` `AdditionalDependencies` (or let ProjectReference handle it)
- [ ] Build `HerculesAC Debug|x64`, verify zero errors
- [ ] Commit: `feat(m4): Reporting static library project + HerculesAC link`

---

### Task 6: Wire Reporter into HerculesAC

**Files:**
- Modify: `HerculesAC/WinMain.h`
- Modify: `HerculesAC/WinMain.cpp`

- [ ] In `WinMain.h`, add `#include "../Common/Reporting/Reporter.h"` and declare `extern hac::reporting::Reporter g_reporter;`
- [ ] In `WinMain.cpp`, define `g_reporter` and call `g_reporter.Configure(cfg)` in `WinMain` after manifest loads, pulling `endpoint_url` from a new `manifest.reporting.endpoint` field (use empty string as default if field absent); add `g_reporter.Flush()` before exit
- [ ] Replace one existing detection path (`ScannerThread` PROC_NAME hit) with `g_reporter.Emit(ev)` to smoke-test the pipeline
- [ ] Commit: `feat(m4): wire Reporter into HerculesAC WinMain + ScannerThread emit`

---

### Task 7: Backend contract document

**Files:**
- Create: `docs/backend-contract.md`

- [ ] Write `docs/backend-contract.md` covering:
  - Endpoint URL structure: `POST /v1/events`, `Content-Type: application/json`, body = JSON array of `AntiCheatEvent` objects
  - HTTP status semantics: 2xx=accepted+removed, 4xx=drop-do-not-retry, 5xx=retry with backoff, 429=use `Retry-After` header if present else treat as 5xx
  - Rate limits: client caps at 10 events/s sustained, burst 50
  - Authentication: `X-HAC-Client-ID` header = `client_id` field
  - JSON field table matching `AntiCheatEvent` struct
  - `evidence_json` object schema per `DetectionKind`
  - Certificate rotation: `reporting.cert_sha256` in manifest; client pins and refuses connections not matching
- [ ] Commit: `docs(m4): backend contract — endpoint, HTTP semantics, JSON schema`

---

### Task 8: Unit tests for EventQueue

**Files:**
- Create: `Common/Reporting/tests/test_EventQueue.cpp`

- [ ] Write gtest cases:
  - `PushAndPeek`: push 3 events, peek returns 3 in order
  - `Remove`: push 3, remove first rowid, peek returns 2
  - `EvictionAtCap`: push events until queue exceeds 1 KB cap (tiny cap for test), confirm oldest are evicted
  - `PersistAcrossReopen`: push 2 events, destroy queue, reopen same file, peek returns 2
- [ ] Add `test_EventQueue.cpp` to existing gtest project or a new `Reporting.Tests.vcxproj`
- [ ] Run tests, confirm 4/4 pass
- [ ] Commit: `test(m4): EventQueue fill/evict/persist unit tests (4/4 pass)`

---

### Task 9: Mock server + README + tag

**Files:**
- Create: `tools/mock-server/server.py`
- Create: `tools/mock-server/requirements.txt`
- Modify: `README.md`
- Modify: `.superpowers/sdd/progress.md`

- [ ] Write `tools/mock-server/server.py` (Flask): `POST /v1/events` logs body, returns 200; `GET /v1/events` returns logged events as JSON; supports `?fail=4xx|5xx|429` query param to simulate error responses
- [ ] Write `tools/mock-server/requirements.txt`: `flask>=3.0`
- [ ] Update README: add "Reporting pipeline (M4)" section to Known issues / Recent history
- [ ] Update `.superpowers/sdd/progress.md` with M4 tasks completed and commits
- [ ] Commit: `docs(m4): mock server + README update`
- [ ] Tag: `git tag m4-reporting-protocol`
