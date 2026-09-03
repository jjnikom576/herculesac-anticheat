#include "DriverEnumDetector.h"
#include <windows.h>
#include <winternl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>

typedef NTSTATUS (NTAPI* pfnNtQuerySystemInfo)(ULONG, PVOID, ULONG, PULONG);
static pfnNtQuerySystemInfo g_NtQSI = nullptr;

#ifndef SystemModuleInformation
#define SystemModuleInformation 11
#endif

struct RTL_PROCESS_MODULE_INFORMATION {
    HANDLE Section;
    PVOID  MappedBase;
    PVOID  ImageBase;
    ULONG  ImageSize;
    ULONG  Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    CHAR   FullPathName[256];
};

struct RTL_PROCESS_MODULES {
    ULONG NumberOfModules;
    RTL_PROCESS_MODULE_INFORMATION Modules[1];
};

// Known bad driver file names (lower-case, no path).
static const char* kBadDrivers[] = {
    "dbk64.sys",               // Cheat Engine kernel driver
    "dbk32.sys",
    "gh-injector-x64.sys",
    "gh-injector-x86.sys",
    "kdmapper.sys",            // IQVW64E.SYS via KDMapper
    "iqvw64e.sys",
    "capcom.sys",              // Capcom vulnerable driver
    "procexp152.sys",          // Exploited Process Explorer driver
    "kprocesshacker.sys",      // Process Hacker kernel driver
    "winring0x64.sys",         // Exploited hardware driver
    "winring0.sys",
    "physmem.sys",
    "gdrv.sys",                // Exploited Gigabyte driver
    "gdrv2.sys",
    "rtcore64.sys",            // Exploited MSI Afterburner driver
};

static std::string ToLower(const char* s)
{
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

namespace hac { namespace detectors {

std::string_view DriverEnumDetector::Name() const noexcept { return "driver-enum"; }
hac::reporting::DetectionKind DriverEnumDetector::Kind() const noexcept
{ return hac::reporting::DetectionKind::KnownBadDriver; }
std::chrono::milliseconds DriverEnumDetector::Interval() const noexcept
{ return std::chrono::seconds(60); }

void DriverEnumDetector::Poll(hac::reporting::Reporter& out)
{
    if (!g_NtQSI) {
        g_NtQSI = (pfnNtQuerySystemInfo)GetProcAddress(
            GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
        if (!g_NtQSI) return;
    }

    ULONG size = 0;
    g_NtQSI(SystemModuleInformation, nullptr, 0, &size);
    if (!size) return;

    size += 4096; // grow slightly to avoid TOCTOU truncation
    std::vector<BYTE> buf(size);
    NTSTATUS st = g_NtQSI(SystemModuleInformation, buf.data(), size, &size);
    if (st != 0) return;

    auto* mods = (RTL_PROCESS_MODULES*)buf.data();
    for (ULONG i = 0; i < mods->NumberOfModules; ++i) {
        const char* fullPath = mods->Modules[i].FullPathName;
        // Extract filename from full NT path.
        const char* slash = strrchr(fullPath, '\\');
        const char* name  = slash ? slash + 1 : fullPath;
        std::string nameLower = ToLower(name);

        for (const char* bad : kBadDrivers) {
            if (nameLower == bad) {
                char hexBase[32];
                snprintf(hexBase, sizeof(hexBase), "%016llX",
                         (unsigned long long)(uintptr_t)mods->Modules[i].ImageBase);

                hac::reporting::AntiCheatEvent ev{};
                ev.severity         = hac::reporting::Severity::Critical;
                ev.kind             = Kind();
                ev.detector_version = "driver-enum@1.0.0";
                ev.evidence_json    = "{\"driver\":\"" + nameLower
                                    + "\",\"base\":\"" + hexBase
                                    + "\",\"size\":" + std::to_string(mods->Modules[i].ImageSize) + "}";
                out.Emit(std::move(ev));
                break;
            }
        }
    }
}

}} // namespace hac::detectors
