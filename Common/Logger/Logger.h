#ifndef _LOGGER_H
#define _LOGGER_H

// Logger — thin wrapper over rotating file output.
//
// Drop-in replacement for the original ad-hoc Logger: same public API
// (outDebug / Log), adds daily rotation with a 7-file cap and forwards
// CRITICAL-level messages to the Windows Event Log.
//
// No external dependencies: rotation is implemented directly so that no
// additional library is needed for project builds that don't vendor spdlog.

#include <windows.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <algorithm>
#include <fstream>
#include <mutex>
#include <ctime>
#include <cstdarg>
#include <sstream>
#include <iomanip>

// Forward declaration from Common — not re-included to avoid circular headers.
namespace FileSystem { std::string GetSelfModuleName(); }
namespace Common    { std::wstring stringToWideString(const std::string&); }

class Logger {
public:
    explicit Logger(const std::string& filename) : m_basename(filename)
    {
        OpenRotated();
    }

    Logger() = default;

    ~Logger()
    {
        if (m_file.is_open()) m_file.close();
    }

    // ── Debug output (OutputDebugString) ────────────────────────────────
    void _outDebug(const TCHAR* sText)
    {
        std::lock_guard<std::mutex> lk(m_mu);
        if (m_modName.empty())
            m_modName = Common::stringToWideString(FileSystem::GetSelfModuleName());

        TCHAR buf[1024] = {};
        _tcsncat(buf, _T("["), _countof(buf) - 1);
        _tcsncat(buf, m_modName.c_str(), _countof(buf) - 1);
        _tcsncat(buf, _T("] "), _countof(buf) - 1);
        _tcsncat(buf, sText, _countof(buf) - 1);
        OutputDebugString(buf);
        OutputDebugString(_T("\n"));
    }

    int outDebug(const TCHAR* fmt, ...)
    {
        __try {
            TCHAR buf[1024] = {};
            va_list args;
            va_start(args, fmt);
            int r = vswprintf(buf, _countof(buf), fmt, args);
            va_end(args);
            _outDebug(buf);
            return r;
        }
        __except (1) {}
        return 0;
    }

    // ── File log ─────────────────────────────────────────────────────────
    void Log(const char* fmt, ...)
    {
        std::lock_guard<std::mutex> lk(m_mu);
        if (m_basename.empty()) return;

        RotateIfNeeded();

        std::time_t now = std::time(nullptr);
        std::tm* lt = std::localtime(&now);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y/%m/%d %H:%M:%S", lt);

        char msg[1024] = {};
        va_list args;
        va_start(args, fmt);
        vsnprintf(msg, sizeof(msg), fmt, args);
        va_end(args);

        if (m_file.is_open()) {
            m_file << "[" << ts << "] " << msg << "\n";
            m_file.flush();
        }

        // Forward critical messages (containing "hack" or "detected") to Event Log.
        std::string s(msg);
        if (s.find("hack") != std::string::npos ||
            s.find("Hack") != std::string::npos ||
            s.find("Detected") != std::string::npos ||
            s.find("detected") != std::string::npos)
        {
            WriteEventLog(msg);
        }
    }

private:
    std::ofstream m_file;
    std::string   m_basename;
    std::wstring  m_modName;
    std::mutex    m_mu;
    int           m_logDay = -1;

    // Open (or reopen) the log file, stamped with today's date.
    void OpenRotated()
    {
        if (m_basename.empty()) return;
        if (m_file.is_open()) m_file.close();

        std::time_t now = std::time(nullptr);
        std::tm* lt     = std::localtime(&now);
        m_logDay        = lt->tm_mday;

        char stamp[16];
        std::strftime(stamp, sizeof(stamp), "%Y%m%d", lt);

        // Insert date before the last extension: "foo.txt" → "foo_20260903.txt"
        std::string name = m_basename;
        auto dot = name.rfind('.');
        if (dot != std::string::npos)
            name = name.substr(0, dot) + "_" + stamp + name.substr(dot);
        else
            name += "_" + std::string(stamp);

        m_file.open(name, std::ios::out | std::ios::app);
        PruneOldLogs();
    }

    // Reopen the log file when the calendar day changes.
    void RotateIfNeeded()
    {
        std::time_t now = std::time(nullptr);
        std::tm* lt     = std::localtime(&now);
        if (lt->tm_mday != m_logDay) OpenRotated();
    }

    // Keep at most 7 dated log files; delete the oldest extras.
    void PruneOldLogs()
    {
        if (m_basename.empty()) return;
        auto dot = m_basename.rfind('.');
        std::string prefix = (dot != std::string::npos) ? m_basename.substr(0, dot) : m_basename;
        std::string ext    = (dot != std::string::npos) ? m_basename.substr(dot) : "";
        std::string pattern = prefix + "_????????" + ext;

        std::vector<std::string> files;
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;
        do { files.push_back(fd.cFileName); } while (FindNextFileA(h, &fd));
        FindClose(h);

        std::sort(files.begin(), files.end());
        while (files.size() > 7) {
            DeleteFileA(files.front().c_str());
            files.erase(files.begin());
        }
    }

    static void WriteEventLog(const char* msg)
    {
        HANDLE h = RegisterEventSourceW(nullptr, L"HerculesAC");
        if (!h) return;
        // Convert to wide for the Event Log API.
        int wlen = MultiByteToWideChar(CP_ACP, 0, msg, -1, nullptr, 0);
        std::wstring wmsg(wlen, 0);
        MultiByteToWideChar(CP_ACP, 0, msg, -1, wmsg.data(), wlen);
        LPCWSTR strs[1] = { wmsg.c_str() };
        ReportEventW(h, EVENTLOG_WARNING_TYPE, 0, 2001, nullptr, 1, 0, strs, nullptr);
        DeregisterEventSource(h);
    }
};

#endif // !_LOGGER_H
