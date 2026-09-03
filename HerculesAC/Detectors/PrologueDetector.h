#pragma once
#include "IDetector.h"

namespace hac { namespace detectors {

// Checks the first N bytes of a curated set of high-value WinAPI functions for
// common inline-hook patterns (JMP rel32, JMP [RIP+disp], MOV RAX+JMP).
class PrologueDetector final : public IDetector {
public:
    std::string_view              Name()     const noexcept override;
    hac::reporting::DetectionKind Kind()     const noexcept override;
    std::chrono::milliseconds     Interval() const noexcept override;
    void Poll(hac::reporting::Reporter& out) override;
};

}} // namespace hac::detectors
