#include <gtest/gtest.h>
#include "../EventQueue.h"
#include <windows.h>
#include <cstdio>

using namespace hac::reporting;

static std::wstring TempQueuePath(const char* suffix)
{
    WCHAR tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring path = std::wstring(tmp) + L"hac_test_queue_"
                        + std::wstring(suffix, suffix + strlen(suffix))
                        + L".db";
    DeleteFileW(path.c_str());
    return path;
}

static AntiCheatEvent MakeEvent(const char* detail)
{
    AntiCheatEvent ev{};
    ev.client_id        = "test-client";
    ev.session_id       = "test-session";
    ev.timestamp_utc_ms = 0;
    ev.game_id          = "cs";
    ev.severity         = Severity::Info;
    ev.kind             = DetectionKind::ProcName;
    ev.detector_version = "test@1.0";
    ev.evidence_json    = detail;  // plain string, no inner escaping
    return ev;
}

TEST(EventQueue, PushAndPeek)
{
    auto path = TempQueuePath("push");
    EventQueue q(path);
    ASSERT_TRUE(q.Push(MakeEvent("a")));
    ASSERT_TRUE(q.Push(MakeEvent("b")));
    ASSERT_TRUE(q.Push(MakeEvent("c")));

    auto rows = q.Peek(10);
    ASSERT_EQ(rows.size(), 3u);
    // Rows come back in insertion order; evidence_json content is present.
    EXPECT_NE(rows[0].second.find("\"a\""), std::string::npos);
    EXPECT_NE(rows[1].second.find("\"b\""), std::string::npos);
    EXPECT_NE(rows[2].second.find("\"c\""), std::string::npos);
}

TEST(EventQueue, Remove)
{
    auto path = TempQueuePath("remove");
    EventQueue q(path);
    q.Push(MakeEvent("x"));
    q.Push(MakeEvent("y"));
    q.Push(MakeEvent("z"));

    auto rows = q.Peek(10);
    ASSERT_EQ(rows.size(), 3u);
    int64_t first_rowid = rows[0].first;
    q.Remove(first_rowid);

    rows = q.Peek(10);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_NE(rows[0].second.find("\"y\""), std::string::npos);  // oldest remaining
}

TEST(EventQueue, EvictionAtCap)
{
    auto path = TempQueuePath("evict");
    // tiny cap: 4 KB — should force eviction after a few events
    EventQueue q(path, 4096);

    // push events until eviction occurs
    for (int i = 0; i < 500; ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "ev%d", i);
        q.Push(MakeEvent(buf));
    }

    // Queue must not be empty (some rows remain), and byte size must be <= cap * small factor
    auto rows = q.Peek(1000);
    EXPECT_FALSE(rows.empty());
    // All remaining rows are recent, not the very first
    bool found_early = false;
    for (auto& r : rows) {
        if (r.second.find("\"ev0\"") != std::string::npos ||
            r.second.find("\"ev1\"") != std::string::npos) {
            found_early = true;
        }
    }
    EXPECT_FALSE(found_early) << "Oldest rows should have been evicted";
}

TEST(EventQueue, PersistAcrossReopen)
{
    auto path = TempQueuePath("persist");
    {
        EventQueue q(path);
        ASSERT_TRUE(q.Push(MakeEvent("persist1")));
        ASSERT_TRUE(q.Push(MakeEvent("persist2")));
    }

    // Reopen the same file
    EventQueue q2(path);
    auto rows = q2.Peek(10);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_NE(rows[0].second.find("persist1"), std::string::npos);
    EXPECT_NE(rows[1].second.find("persist2"), std::string::npos);
}
