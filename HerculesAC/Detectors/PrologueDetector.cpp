#include "PrologueDetector.h"
#include <windows.h>
#include <string>

namespace hac { namespace detectors {

// APIs to monitor for inline hooks.
static const struct { const char* module; const char* api; } kApis[] = {
    { "ntdll.dll",       "NtOpenProcess"           },
    { "ntdll.dll",       "NtAllocateVirtualMemory" },
    { "ntdll.dll",       "NtWriteVirtualMemory"    },
    { "ntdll.dll",       "NtReadVirtualMemory"     },
    { "ntdll.dll",       "NtCreateThreadEx"        },
    { "kernel32.dll",    "VirtualAllocEx"          },
    { "kernel32.dll",    "WriteProcessMemory"      },
    { "kernel32.dll",    "ReadProcessMemory"       },
    { "kernel32.dll",    "CreateRemoteThread"      },
    { "kernelbase.dll",  "VirtualAllocEx"          },
    { "kernelbase.dll",  "WriteProcessMemory"      },
};

// Returns true if the first bytes look like a common hook trampoline.
static bool LooksHooked(const BYTE* p)
{
    // JMP rel32
    if (p[0] == 0xE9) return true;
    // JMP [RIP+disp32]
    if (p[0] == 0xFF && p[1] == 0x25) return true;
    // MOV RAX, imm64 + JMP RAX  (48 B8 ... FF E0)
    if (p[0] == 0x48 && p[1] == 0xB8) return true;
    // MOV RCX, imm64 (sometimes used for hook thunks)
    if (p[0] == 0x48 && p[1] == 0xB9) return true;
    // PUSH imm32 + RET (older 32-bit style hook in 64-bit procs)
    if (p[0] == 0x68) return true;
    return false;
}

std::string_view PrologueDetector::Name() const noexcept { return "prologue-hook"; }
hac::reporting::DetectionKind PrologueDetector::Kind() const noexcept
{ return hac::reporting::DetectionKind::HookIntegrity; }
std::chrono::milliseconds PrologueDetector::Interval() const noexcept
{ return std::chrono::seconds(5); }

void PrologueDetector::Poll(hac::reporting::Reporter& out)
{
    for (const auto& entry : kApis) {
        HMODULE hMod = GetModuleHandleA(entry.module);
        if (!hMod) continue;

        auto* fn = (const BYTE*)GetProcAddress(hMod, entry.api);
        if (!fn) continue;

        if (LooksHooked(fn)) {
            char hexFn[32];
            snprintf(hexFn, sizeof(hexFn), "%016llX", (unsigned long long)(uintptr_t)fn);

            // Capture first 8 bytes as hex for evidence.
            char firstBytes[24] = {};
            for (int i = 0; i < 8; ++i)
                snprintf(firstBytes + i * 2, 3, "%02X", fn[i]);

            hac::reporting::AntiCheatEvent ev{};
            ev.severity         = hac::reporting::Severity::Critical;
            ev.kind             = Kind();
            ev.detector_version = "prologue-hook@1.0.0";
            ev.evidence_json    = "{\"module\":\"" + std::string(entry.module)
                                + "\",\"api\":\"" + std::string(entry.api)
                                + "\",\"va\":\"" + hexFn
                                + "\",\"bytes\":\"" + firstBytes + "\"}";
            out.Emit(std::move(ev));
        }
    }
}

}} // namespace hac::detectors
