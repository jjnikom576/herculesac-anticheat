#include "ThreadInjectDetector.h"
#include <windows.h>
#include <TlHelp32.h>
#include <psapi.h>
#include <winternl.h>
#include <string>

// NtQueryInformationThread - dynamically resolved.
typedef NTSTATUS (NTAPI* pfnNtQueryInfoThread)(HANDLE, ULONG, PVOID, ULONG, PULONG);
static pfnNtQueryInfoThread g_NtQIT = nullptr;

static bool EnsureNtQIT()
{
    if (g_NtQIT) return true;
    g_NtQIT = (pfnNtQueryInfoThread)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");
    return g_NtQIT != nullptr;
}

// ThreadQuerySetWin32StartAddress = 9
static uintptr_t GetThreadStartAddress(HANDLE hThread)
{
    if (!EnsureNtQIT()) return 0;
    uintptr_t addr = 0;
    g_NtQIT(hThread, 9, &addr, sizeof(addr), nullptr);
    return addr;
}

static bool AddressInAnyModule(uintptr_t addr)
{
    HMODULE mods[1024];
    DWORD needed = 0;
    if (!EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed))
        return false;
    DWORD count = needed / sizeof(HMODULE);
    for (DWORD i = 0; i < count; ++i) {
        MODULEINFO mi{};
        GetModuleInformation(GetCurrentProcess(), mods[i], &mi, sizeof(mi));
        auto base = (uintptr_t)mi.lpBaseOfDll;
        if (addr >= base && addr < base + mi.SizeOfImage) return true;
    }
    return false;
}

// Returns true if the address lives in MEM_PRIVATE memory with executable permission.
// Reflectively-loaded DLLs appear to be in a "module" range but their backing
// pages are MEM_PRIVATE (not MEM_IMAGE), which exposes the technique.
static bool IsPrivateExecutable(uintptr_t addr)
{
    if (!addr) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi))) return false;
    if (mbi.Type != MEM_PRIVATE) return false;
    const DWORD execMask = PAGE_EXECUTE | PAGE_EXECUTE_READ |
                           PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & execMask) != 0;
}

namespace hac { namespace detectors {

std::string_view ThreadInjectDetector::Name() const noexcept { return "thread-inject"; }
hac::reporting::DetectionKind ThreadInjectDetector::Kind() const noexcept
{ return hac::reporting::DetectionKind::ThreadInject; }
std::chrono::milliseconds ThreadInjectDetector::Interval() const noexcept
{ return std::chrono::seconds(10); }

void ThreadInjectDetector::Poll(hac::reporting::Reporter& out)
{
    DWORD pid = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 te; te.dwSize = sizeof(te);
    if (!Thread32First(snap, &te)) { CloseHandle(snap); return; }

    do {
        if (te.th32OwnerProcessID != pid) continue;
        DWORD tid = te.th32ThreadID;

        bool isNew = (m_seenThreadIds.find(tid) == m_seenThreadIds.end());
        m_seenThreadIds.insert(tid);

        if (!isNew) continue;

        HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, tid);
        if (!hThread) continue;

        uintptr_t startAddr = GetThreadStartAddress(hThread);
        CloseHandle(hThread);

        bool suspectNoModule  = startAddr && !AddressInAnyModule(startAddr);
        bool suspectPrivateEx = startAddr && IsPrivateExecutable(startAddr);
        if (suspectNoModule || suspectPrivateEx) {
            char hexAddr[32];
            snprintf(hexAddr, sizeof(hexAddr), "%016llX", (unsigned long long)startAddr);

            hac::reporting::AntiCheatEvent ev{};
            ev.severity         = hac::reporting::Severity::Critical;
            ev.kind             = Kind();
            ev.detector_version = "thread-inject@1.1.0";
            ev.evidence_json    = "{\"tid\":" + std::to_string(tid)
                                + ",\"start_va\":\"" + hexAddr
                                + "\",\"no_module\":" + (suspectNoModule  ? "true" : "false")
                                + ",\"private_exec\":" + (suspectPrivateEx ? "true" : "false")
                                + "}";
            out.Emit(std::move(ev));
        }
    } while (Thread32Next(snap, &te));

    CloseHandle(snap);
}

}} // namespace hac::detectors
