#include "Reporter.h"
#include "EventQueue.h"
#include <winhttp.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

#pragma comment(lib, "winhttp.lib")

namespace hac { namespace reporting {

struct Reporter::Impl {
    ReporterConfig            cfg;
    EventQueue*               queue = nullptr;
    std::thread               worker;
    std::atomic<bool>         stop{false};
    std::mutex                wakeMu;
    std::condition_variable   wakeCv;

    bool SendBatch();
    void WorkerLoop();
};

Reporter::Reporter() : m_impl(new Impl) {}

Reporter::~Reporter()
{
    if (m_impl->worker.joinable()) {
        m_impl->stop = true;
        m_impl->wakeCv.notify_all();
        m_impl->worker.join();
    }
    delete m_impl->queue;
    delete m_impl;
}

void Reporter::Configure(const ReporterConfig& cfg)
{
    // Stop any running worker before reconfiguring.
    if (m_impl->worker.joinable()) {
        m_impl->stop = true;
        m_impl->wakeCv.notify_all();
        m_impl->worker.join();
    }
    m_impl->cfg  = cfg;
    delete m_impl->queue;
    m_impl->queue = cfg.db_path.empty() ? nullptr : new EventQueue(cfg.db_path);
    m_impl->stop  = false;
    m_impl->worker = std::thread([this] { m_impl->WorkerLoop(); });
}

void Reporter::Emit(AntiCheatEvent ev)
{
    ev.client_id  = m_impl->cfg.client_id;
    ev.session_id = m_impl->cfg.session_id;
    ev.game_id    = m_impl->cfg.game_id;
    if (!ev.timestamp_utc_ms) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ev.timestamp_utc_ms =
            (((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime)
            / 10000ULL - 11644473600000ULL;
    }
    if (m_impl->queue) {
        m_impl->queue->Push(ev);
        m_impl->wakeCv.notify_one();
    }
}

void Reporter::Flush(DWORD timeout_ms)
{
    m_impl->wakeCv.notify_one();
    DWORD deadline = GetTickCount() + timeout_ms;
    while (GetTickCount() < deadline) {
        if (!m_impl->queue || m_impl->queue->Peek(1).empty()) return;
        Sleep(50);
    }
}

void Reporter::Impl::WorkerLoop()
{
    // Exponential backoff delays in ms: 1s 5s 30s 5m 30m
    static const DWORD kBackoff[] = {1000, 5000, 30000, 300000, 1800000};
    int backoffIdx = 0;

    while (!stop) {
        {
            std::unique_lock<std::mutex> lk(wakeMu);
            wakeCv.wait_for(lk, std::chrono::milliseconds(5000), [this] {
                return stop.load() ||
                       (queue && !queue->Peek(1).empty());
            });
        }
        if (stop) break;
        if (!queue || queue->Peek(1).empty()) continue;

        if (SendBatch()) {
            backoffIdx = 0;
        } else {
            DWORD wait = kBackoff[backoffIdx < 5 ? backoffIdx++ : 4];
            // Sleep in small chunks so we can wake on stop.
            DWORD slept = 0;
            while (!stop && slept < wait) {
                Sleep(200);
                slept += 200;
            }
        }
    }
}

bool Reporter::Impl::SendBatch()
{
    if (!queue || cfg.endpoint_url.empty()) return true;
    auto rows = queue->Peek(32);
    if (rows.empty()) return true;

    // Build JSON array body.
    std::string body = "[";
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i) body += ",";
        body += rows[i].second;
    }
    body += "]";

    // Crack the URL into components.
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    WCHAR host[256] = {}, path[2048] = {};
    uc.lpszHostName  = host; uc.dwHostNameLength  = 256;
    uc.lpszUrlPath   = path; uc.dwUrlPathLength   = 2048;
    if (!WinHttpCrackUrl(cfg.endpoint_url.c_str(),
                         (DWORD)cfg.endpoint_url.size(), 0, &uc))
        return false;

    bool secure = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hSession = WinHttpOpen(L"HerculesAC-Reporter/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"POST", path,
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        secure ? WINHTTP_FLAG_SECURE : 0);
    if (!hReq) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    bool success = false;
    if (WinHttpSendRequest(hReq,
            L"Content-Type: application/json\r\n",
            (DWORD)-1L,
            (LPVOID)body.c_str(),
            (DWORD)body.size(),
            (DWORD)body.size(), 0)
        && WinHttpReceiveResponse(hReq, nullptr))
    {
        DWORD statusCode = 0, len = sizeof(statusCode);
        WinHttpQueryHeaders(hReq,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode, &len, WINHTTP_NO_HEADER_INDEX);

        if (statusCode >= 200 && statusCode < 300) {
            for (auto& r : rows) queue->Remove(r.first);
            success = true;
        } else if (statusCode >= 400 && statusCode < 500) {
            // 4xx — drop without retry.
            for (auto& r : rows) queue->Remove(r.first);
            success = true;
        }
        // 5xx / other: leave rows in queue, return false → backoff.
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

}} // namespace hac::reporting
