#include "IATHookDetector.h"
#include <windows.h>
#include <imagehlp.h>
#include <psapi.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "imagehlp.lib")

namespace hac { namespace detectors {

static const wchar_t* kMonitoredModules[] = {
    L"ntdll.dll", L"kernel32.dll", L"kernelbase.dll", L"user32.dll"
};

// Check whether |ptr| lies within the mapped region of |hMod|.
static bool WithinModule(HMODULE hMod, uintptr_t ptr)
{
    MODULEINFO mi{};
    GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi));
    auto base = (uintptr_t)mi.lpBaseOfDll;
    return ptr >= base && ptr < base + mi.SizeOfImage;
}

std::string_view IATHookDetector::Name() const noexcept { return "iat-hook"; }
hac::reporting::DetectionKind IATHookDetector::Kind() const noexcept
{ return hac::reporting::DetectionKind::IatHook; }
std::chrono::milliseconds IATHookDetector::Interval() const noexcept
{ return std::chrono::seconds(30); }

void IATHookDetector::Poll(hac::reporting::Reporter& out)
{
    HANDLE hProc = GetCurrentProcess();
    // Use a static anchor within this translation unit to locate our own module.
    static const BYTE s_anchor = 0;
    HMODULE hSelf = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&s_anchor, &hSelf);
    if (!hSelf) return;

    auto* dosHdr = (PIMAGE_DOS_HEADER)hSelf;
    auto* ntHdr  = (PIMAGE_NT_HEADERS)((BYTE*)hSelf + dosHdr->e_lfanew);
    auto& impDir = ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!impDir.VirtualAddress) return;

    auto* desc = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)hSelf + impDir.VirtualAddress);
    for (; desc->Name; ++desc) {
        const char* dllName = (const char*)((BYTE*)hSelf + desc->Name);

        // Check only monitored modules.
        bool monitored = false;
        for (auto* m : kMonitoredModules) {
            char narrow[64] = {};
            WideCharToMultiByte(CP_ACP, 0, m, -1, narrow, 63, nullptr, nullptr);
            if (_stricmp(dllName, narrow) == 0) { monitored = true; break; }
        }
        if (!monitored) continue;

        HMODULE hImported = GetModuleHandleA(dllName);
        if (!hImported) continue;

        auto* thunk     = (PIMAGE_THUNK_DATA)((BYTE*)hSelf + desc->FirstThunk);
        auto* origThunk = desc->OriginalFirstThunk
                        ? (PIMAGE_THUNK_DATA)((BYTE*)hSelf + desc->OriginalFirstThunk)
                        : nullptr;

        for (int idx = 0; thunk->u1.Function; ++thunk, ++idx) {
            uintptr_t fn = (uintptr_t)thunk->u1.Function;
            if (!WithinModule(hImported, fn)) {
                std::string apiName;
                if (origThunk && !(origThunk[idx].u1.Ordinal & IMAGE_ORDINAL_FLAG64)) {
                    auto* byName = (PIMAGE_IMPORT_BY_NAME)(
                        (BYTE*)hSelf + origThunk[idx].u1.AddressOfData);
                    apiName = byName->Name;
                }

                char hexFn[32], hexBase[32];
                snprintf(hexFn,   sizeof(hexFn),   "%016llX", (unsigned long long)fn);
                MODULEINFO mi{}; GetModuleInformation(hProc, hImported, &mi, sizeof(mi));
                snprintf(hexBase, sizeof(hexBase), "%016llX", (unsigned long long)mi.lpBaseOfDll);

                hac::reporting::AntiCheatEvent ev{};
                ev.severity         = hac::reporting::Severity::Critical;
                ev.kind             = Kind();
                ev.detector_version = "iat-hook@1.0.0";
                ev.evidence_json    = "{\"module\":\"" + std::string(dllName)
                                    + "\",\"api\":\"" + apiName
                                    + "\",\"observed_va\":\"" + hexFn
                                    + "\",\"module_base\":\"" + hexBase + "\"}";
                out.Emit(std::move(ev));
            }
        }
    }
}

}} // namespace hac::detectors
