#pragma once
#include "IDetector.h"
#include <string>
#include <cstdint>

namespace hac { namespace detectors {

// SHA-256 hashes the game module's .text section on first call to establish a
// baseline, then re-hashes on every subsequent poll and reports divergence.
class CodeSectionDetector final : public IDetector {
public:
    std::string_view              Name()     const noexcept override;
    hac::reporting::DetectionKind Kind()     const noexcept override;
    std::chrono::milliseconds     Interval() const noexcept override;
    void Poll(hac::reporting::Reporter& out) override;

private:
    bool        m_baselineSet = false;
    std::string m_baselineHex;
    uintptr_t   m_textBase = 0;
    size_t      m_textSize = 0;
};

}} // namespace hac::detectors
