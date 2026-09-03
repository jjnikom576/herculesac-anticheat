#include "AntiDebug.h"
#include <winternl.h>
#include <intrin.h>
#include <cstdint>

// NtQueryInformationProcess — dynamically resolved to avoid import table hint.
typedef NTSTATUS (NTAPI* pfnNtQIP)(HANDLE, ULONG, PVOID, ULONG, PULONG);
static pfnNtQIP g_NtQIP = nullptr;

static bool EnsureNtQIP()
{
    if (g_NtQIP) return true;
    g_NtQIP = (pfnNtQIP)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess");
    return g_NtQIP != nullptr;
}

// ── Individual checks ────────────────────────────────────────────────────────

static bool Check_IsDebuggerPresent()
{
    return ::IsDebuggerPresent() != FALSE;
}

static bool Check_RemoteDebugger()
{
    BOOL remote = FALSE;
    ::CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote);
    return remote != FALSE;
}

static bool Check_DebugPort()
{
    if (!EnsureNtQIP()) return false;
    HANDLE port = nullptr;
    // ProcessDebugPort = 7
    NTSTATUS st = g_NtQIP(GetCurrentProcess(), 7, &port, sizeof(port), nullptr);
    return NT_SUCCESS(st) && port != nullptr;
}

static bool Check_NtGlobalFlag()
{
    // The PEB's NtGlobalFlag word is set to 0x70 under a debugger.
#ifdef _WIN64
    auto* peb = (BYTE*)__readgsqword(0x60);
    DWORD ntGlobalFlag = *(DWORD*)(peb + 0xBC);
#else
    auto* peb = (BYTE*)__readfsdword(0x30);
    DWORD ntGlobalFlag = *(DWORD*)(peb + 0x68);
#endif
    return (ntGlobalFlag & 0x70) != 0;
}

static bool Check_HeapFlags()
{
    // Under a debugger, heap ForceFlags contains HEAP_TAIL_CHECKING_ENABLED etc.
    HANDLE heap = GetProcessHeap();
    if (!heap) return false;
#ifdef _WIN64
    // Heap header at base+0x70 = Flags, +0x74 = ForceFlags (Windows 10+)
    auto* hdr = (BYTE*)heap;
    DWORD flags      = *(DWORD*)(hdr + 0x70);
    DWORD forceFlags = *(DWORD*)(hdr + 0x74);
#else
    auto* hdr = (BYTE*)heap;
    DWORD flags      = *(DWORD*)(hdr + 0x40);
    DWORD forceFlags = *(DWORD*)(hdr + 0x44);
#endif
    return (flags & ~2) != 0 || forceFlags != 0;
}

static bool Check_Timing()
{
    // Two consecutive RDTSC reads with a known-light loop in between.
    // Under a VM or debugger with single-stepping the delta is anomalously large.
    uint64_t t0 = __rdtsc();
    // ~50 ns of real work; a single-stepped debugger will be orders of magnitude slower.
    volatile int sink = 0;
    for (int i = 0; i < 100; ++i) sink += i;
    uint64_t t1 = __rdtsc();
    // Threshold: > 10 million cycles is suspicious on any modern CPU.
    return (t1 - t0) > 10'000'000ULL;
}

static bool Check_ExceptionHijack()
{
    // Raise a single-step exception. If a debugger handles it, the flag
    // won't propagate back to our handler through the standard SEH path.
    bool ok = false;
    __try {
        __debugbreak();  // INT 3; causes EXCEPTION_BREAKPOINT
        ok = true;       // Reached only if exception is handled by our own handler
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        ok = true;       // Our handler caught it — no debugger interception
    }
    // If ok == false, something ate the exception before us (a debugger).
    return !ok;
}

static bool Check_HardwareBreakpoints()
{
    // Read the debug registers of the current thread via GetThreadContext.
    // DR0-DR3 hold hardware breakpoint addresses; DR7 is the control word.
    // Any non-zero DR0-DR3 paired with DR7 != 0 indicates an active hardware BP.
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!GetThreadContext(GetCurrentThread(), &ctx))
        return false;
    // DR7 low bit enables local breakpoints; bits 1,3,5,7 enable DR0-DR3.
    if (ctx.Dr7 != 0 && (ctx.Dr0 || ctx.Dr1 || ctx.Dr2 || ctx.Dr3))
        return true;
    return false;
}

// ────────────────────────────────────────────────────────────────────────────

namespace hac { namespace security {

bool IsDebuggerAttached()
{
    if (Check_IsDebuggerPresent())    return true;
    if (Check_RemoteDebugger())       return true;
    if (Check_DebugPort())            return true;
    if (Check_NtGlobalFlag())         return true;
    if (Check_HeapFlags())            return true;
    if (Check_Timing())               return true;
    if (Check_ExceptionHijack())      return true;
    if (Check_HardwareBreakpoints())  return true;
    return false;
}

void AssertNoDebugger()
{
    if (IsDebuggerAttached()) {
        // Write to Event Log, then terminate the whole process group.
        HANDLE h = RegisterEventSourceW(nullptr, L"HerculesAC");
        if (h) {
            LPCWSTR msg = L"Debugger detected at startup — aborting.";
            ReportEventW(h, EVENTLOG_ERROR_TYPE, 0, 1003, nullptr, 1, 0, &msg, nullptr);
            DeregisterEventSource(h);
        }
        TerminateProcess(GetCurrentProcess(), 0xDEAD);
    }
}

}} // namespace hac::security
