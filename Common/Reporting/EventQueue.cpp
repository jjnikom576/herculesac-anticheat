#include "EventQueue.h"
#include "../include/sqlite3/sqlite3.h"
#include "../include/json/json.h"
#include <windows.h>
#include <mutex>

namespace hac { namespace reporting {

static std::string SerializeEvent(const AntiCheatEvent& ev)
{
    Json::Value root;
    root["client_id"]        = ev.client_id;
    root["session_id"]       = ev.session_id;
    root["timestamp_utc_ms"] = (Json::UInt64)ev.timestamp_utc_ms;
    root["game_id"]          = ev.game_id;
    root["severity"]         = (int)ev.severity;
    root["kind"]             = (int)ev.kind;
    root["detector_version"] = ev.detector_version;
    root["evidence_json"]    = ev.evidence_json;
    root["signature"]        = ev.signature;
    Json::FastWriter fw;
    return fw.write(root);
}

struct EventQueue::Impl {
    sqlite3*   db       = nullptr;
    uint64_t   maxBytes = 0;
    std::mutex mu;

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
    if (sqlite3_open16(db_path.c_str(), &m_impl->db) != SQLITE_OK) {
        sqlite3_close(m_impl->db);
        m_impl->db = nullptr;
        return;
    }
    m_impl->exec("PRAGMA journal_mode=WAL;");
    m_impl->exec("PRAGMA synchronous=NORMAL;");
    m_impl->exec(
        "CREATE TABLE IF NOT EXISTS events("
        "  rowid   INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  payload TEXT    NOT NULL,"
        "  added_ms INTEGER NOT NULL"
        ");");
}

EventQueue::~EventQueue()
{
    if (m_impl->db) sqlite3_close(m_impl->db);
    delete m_impl;
}

bool EventQueue::Push(const AntiCheatEvent& ev)
{
    std::lock_guard<std::mutex> lk(m_impl->mu);
    if (!m_impl->db) return false;

    // evict oldest rows while over cap
    while (ByteSize() >= m_impl->maxBytes) {
        m_impl->exec(
            "DELETE FROM events WHERE rowid=(SELECT MIN(rowid) FROM events);");
    }

    std::string json = SerializeEvent(ev);
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    uint64_t ms = (((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime)
                  / 10000ULL - 11644473600000ULL;

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

std::vector<std::pair<int64_t, std::string>> EventQueue::Peek(int count)
{
    std::lock_guard<std::mutex> lk(m_impl->mu);
    std::vector<std::pair<int64_t, std::string>> out;
    if (!m_impl->db) return out;

    sqlite3_stmt* stmt = nullptr;
    sqlite3_prepare_v2(m_impl->db,
        "SELECT rowid, payload FROM events ORDER BY rowid LIMIT ?;",
        -1, &stmt, nullptr);
    sqlite3_bind_int(stmt, 1, count);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t id      = sqlite3_column_int64(stmt, 0);
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
    // SQLite page_count * page_size gives current DB size
    sqlite3_prepare_v2(m_impl->db,
        "SELECT page_count * page_size "
        "FROM pragma_page_count() AS pc, pragma_page_size() AS ps;",
        -1, &stmt, nullptr);
    uint64_t sz = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        sz = (uint64_t)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return sz;
}

}} // namespace hac::reporting
