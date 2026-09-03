#include "EATHookDetector.h"
#include <windows.h>
#include <psapi.h>
#include <string>
#include <sstream>
#include <iomanip>

namespace hac { namespace detectors {

static const char* kMonitoredModules[] = {
    "ntdll.dll", "kernel32.dll", "kernelbase.dll", "user32.dll"
};

static bool WithinModule(uintptr_t base, DWORD sizeOfImage, uintptr_t va)
{
    return va >= base && va < base + sizeOfImage;
}

std::string_view EATHookDetector::Name() const noexcept { return "eat-hook"; }
hac::reporting::DetectionKind EATHookDetector::Kind() const noexcept
{ return hac::reporting::DetectionKind::EatHook; }
std::chrono::milliseconds EATHookDetector::Interval() const noexcept
{ return std::chrono::seconds(30); }

void EATHookDetector::Poll(hac::reporting::Reporter& out)
{
    for (const char* dllName : kMonitoredModules) {
        HMODULE hMod = GetModuleHandleA(dllName);
        if (!hMod) continue;

        MODULEINFO mi{};
        GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi));

        auto base = (uintptr_t)mi.lpBaseOfDll;
        auto* dosHdr = (PIMAGE_DOS_HEADER)base;
        auto* ntHdr  = (PIMAGE_NT_HEADERS)(base + dosHdr->e_lfanew);

        auto& expDir = ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!expDir.VirtualAddress) continue;

        auto* eatDesc = (PIMAGE_EXPORT_DIRECTORY)(base + expDir.VirtualAddress);
        auto* funcs   = (DWORD*)(base + eatDesc->AddressOfFunctions);
        auto* names   = (DWORD*)(base + eatDesc->AddressOfNames);
        auto* ordinals = (WORD*)(base + eatDesc->AddressOfNameOrdinals);

        for (DWORD i = 0; i < eatDesc->NumberOfNames; ++i) {
            DWORD rva = funcs[ordinals[i]];
            uintptr_t va = base + rva;

            // Skip forwarders (RVA within export directory range).
            bool isForwarder = (rva >= expDir.VirtualAddress &&
                                rva <  expDir.VirtualAddress + expDir.Size);
            if (isForwarder) continue;

            if (!WithinModule(base, mi.SizeOfImage, va)) {
                const char* apiName = (const char*)(base + names[i]);

                char hexVa[32], hexBase[32];
                snprintf(hexVa,   sizeof(hexVa),   "%016llX", (unsigned long long)va);
                snprintf(hexBase, sizeof(hexBase), "%016llX", (unsigned long long)base);

                hac::reporting::AntiCheatEvent ev{};
                ev.severity         = hac::reporting::Severity::Critical;
                ev.kind             = Kind();
                ev.detector_version = "eat-hook@1.0.0";
                ev.evidence_json    = "{\"module\":\"" + std::string(dllName)
                                    + "\",\"api\":\"" + std::string(apiName)
                                    + "\",\"observed_va\":\"" + hexVa
                                    + "\",\"module_base\":\"" + hexBase + "\"}";
                out.Emit(std::move(ev));
            }
        }
    }
}

}} // namespace hac::detectors
