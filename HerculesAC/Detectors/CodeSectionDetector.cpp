#include "CodeSectionDetector.h"
#include <windows.h>
#include <sodium.h>
#include <sstream>
#include <iomanip>

namespace hac { namespace detectors {

static std::string Sha256Hex(const BYTE* data, size_t size)
{
    uint8_t hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(hash, data, size);

    std::ostringstream ss;
    for (int i = 0; i < 32; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

// Locate the .text section of the main executable module.
static bool FindTextSection(uintptr_t& outBase, size_t& outSize)
{
    HMODULE hMain = GetModuleHandleW(nullptr);
    if (!hMain) return false;

    auto base    = (uintptr_t)hMain;
    auto* dosHdr = (PIMAGE_DOS_HEADER)base;
    auto* ntHdr  = (PIMAGE_NT_HEADERS)(base + dosHdr->e_lfanew);

    auto* section = IMAGE_FIRST_SECTION(ntHdr);
    for (WORD i = 0; i < ntHdr->FileHeader.NumberOfSections; ++i, ++section) {
        char name[9] = {};
        memcpy(name, section->Name, 8);
        if (strcmp(name, ".text") == 0) {
            outBase = base + section->VirtualAddress;
            outSize = section->Misc.VirtualSize;
            return true;
        }
    }
    return false;
}

std::string_view CodeSectionDetector::Name() const noexcept { return "code-section"; }
hac::reporting::DetectionKind CodeSectionDetector::Kind() const noexcept
{ return hac::reporting::DetectionKind::CodeSectionDiverge; }
std::chrono::milliseconds CodeSectionDetector::Interval() const noexcept
{ return std::chrono::seconds(30); }

void CodeSectionDetector::Poll(hac::reporting::Reporter& out)
{
    if (!m_textBase || !m_textSize) {
        if (!FindTextSection(m_textBase, m_textSize)) return;
    }

    std::string current = Sha256Hex((const BYTE*)m_textBase, m_textSize);

    if (!m_baselineSet) {
        m_baselineHex = current;
        m_baselineSet = true;
        return;
    }

    if (current != m_baselineHex) {
        char hexBase[32];
        snprintf(hexBase, sizeof(hexBase), "%016llX", (unsigned long long)m_textBase);

        hac::reporting::AntiCheatEvent ev{};
        ev.severity         = hac::reporting::Severity::Critical;
        ev.kind             = Kind();
        ev.detector_version = "code-section@1.0.0";
        ev.evidence_json    = "{\"section\":\".text\""
                            ",\"base\":\"" + std::string(hexBase)
                            + "\",\"size\":" + std::to_string(m_textSize)
                            + ",\"baseline\":\"" + m_baselineHex
                            + "\",\"observed\":\"" + current + "\"}";
        out.Emit(std::move(ev));
    }
}

}} // namespace hac::detectors
