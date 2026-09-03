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
    // ── Cheat Engine ────────────────────────────────────────────────────
    "dbk64.sys",
    "dbk32.sys",

    // ── Injectors ───────────────────────────────────────────────────────
    "gh-injector-x64.sys",
    "gh-injector-x86.sys",
    "kdmapper.sys",

    // ── Exploited WHQL drivers (BYOVD) ──────────────────────────────────
    "iqvw64e.sys",              // Intel Network Adapter (KDMapper)
    "capcom.sys",               // Capcom (arbitrary kernel code exec)
    "procexp152.sys",           // Sysinternals Process Explorer (old)
    "procexp153.sys",
    "kprocesshacker.sys",       // Process Hacker
    "kprocesshacker2.sys",
    "winring0x64.sys",          // OpenHardwareMonitor
    "winring0.sys",
    "physmem.sys",              // Physical memory access
    "gdrv.sys",                 // Gigabyte (EasyAntiCheat bypass)
    "gdrv2.sys",
    "rtcore64.sys",             // MSI Afterburner
    "aswarpot.sys",             // Avast (CVE-2020-9715 — used in BYOVD)
    "aswvmm.sys",               // Avast
    "nvoclock.sys",             // NVIDIA (CVE-2008-4611)
    "speedfan.sys",             // SpeedFan hardware monitor
    "sysdrv3s.sys",             // SYS3
    "alsysio64.sys",            // AL Sys I/O
    "atillk64.sys",             // ATI unlocked driver
    "ene.sys",                  // ENE Tech (used in cheats)
    "glckio2.sys",              // ASRock
    "zam64.sys",                // Zemana AM (CVE-2021-31727)
    "zamguard64.sys",
    "pchunter64a.sys",          // PCHunter
    "mhyprotect.sys",           // Mihoyo (used as bypass vector)
    "hackintool.sys",
    "lenovodiagnosticsdriver.sys", // Lenovo BYOVD
    "hvack.sys",
    "driverhive64.sys",
    "blackbone.sys",            // BlackBone memory library
    "mimidrv.sys",              // Mimikatz kernel driver
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
{ return std::chrono::seconds(15); }

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
