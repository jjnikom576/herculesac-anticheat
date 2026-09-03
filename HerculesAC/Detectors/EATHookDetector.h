#pragma once
#include "IDetector.h"

namespace hac { namespace detectors {

// Walks the EAT of monitored system modules and checks that each exported
// function RVA resolves to an address within the exporting module's own range.
class EATHookDetector final : public IDetector {
public:
    std::string_view              Name()     const noexcept override;
    hac::reporting::DetectionKind Kind()     const noexcept override;
    std::chrono::milliseconds     Interval() const noexcept override;
    void Poll(hac::reporting::Reporter& out) override;
};

}} // namespace hac::detectors
