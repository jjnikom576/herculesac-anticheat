#include "ProcessSigDetector.h"
#include "../../Common/Hash/MD5/MD5.h"
#include <windows.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <sodium.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace hac { namespace detectors {

static std::string Sha256HexOfFile(const wchar_t* path)
{
    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return {};

    crypto_hash_sha256_state st;
    crypto_hash_sha256_init(&st);
    BYTE buf[65536];
    DWORD read;
    while (ReadFile(hf, buf, sizeof(buf), &read, nullptr) && read)
        crypto_hash_sha256_update(&st, buf, read);
    CloseHandle(hf);

    uint8_t hash[32];
    crypto_hash_sha256_final(&st, hash);

    std::ostringstream ss;
    for (int i = 0; i < 32; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

ProcessSigDetector::ProcessSigDetector(std::vector<std::string> known_bad)
    : m_badHashes(std::move(known_bad))
{
    for (auto& h : m_badHashes)
        std::transform(h.begin(), h.end(), h.begin(), ::tolower);
}

std::string_view ProcessSigDetector::Name() const noexcept
{ return "process-sig"; }

hac::reporting::DetectionKind ProcessSigDetector::Kind() const noexcept
{ return hac::reporting::DetectionKind::ModuleHash; }

std::chrono::milliseconds ProcessSigDetector::Interval() const noexcept
{ return std::chrono::seconds(10); }

void ProcessSigDetector::Poll(hac::reporting::Reporter& out)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe; pe.dwSize = sizeof(pe);
    if (!Process32FirstW(snap, &pe)) { CloseHandle(snap); return; }

    do {
        HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                                FALSE, pe.th32ProcessID);
        if (!hp) continue;

        WCHAR imgPath[MAX_PATH] = {};
        GetModuleFileNameExW(hp, nullptr, imgPath, MAX_PATH);
        CloseHandle(hp);

        if (!imgPath[0]) continue;
        std::string sha = Sha256HexOfFile(imgPath);
        if (sha.empty()) continue;
        std::transform(sha.begin(), sha.end(), sha.begin(), ::tolower);

        for (const auto& bad : m_badHashes) {
            if (sha == bad) {
                hac::reporting::AntiCheatEvent ev{};
                ev.severity         = hac::reporting::Severity::Critical;
                ev.kind             = Kind();
                ev.detector_version = "process-sig@1.0.0";
                ev.evidence_json    = "{\"pid\":" + std::to_string(pe.th32ProcessID)
                                    + ",\"sha256\":\"" + sha + "\"}";
                out.Emit(std::move(ev));
                break;
            }
        }
    } while (Process32NextW(snap, &pe));

    CloseHandle(snap);
}

}} // namespace hac::detectors
