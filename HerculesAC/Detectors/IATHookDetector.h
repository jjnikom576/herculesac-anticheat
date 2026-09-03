#pragma once
#include "IDetector.h"

namespace hac { namespace detectors {

// Walks the IAT of the current process's critical modules and checks that
// each thunk still points inside the exporting module's mapped range.
class IATHookDetector final : public IDetector {
public:
    std::string_view              Name()     const noexcept override;
    hac::reporting::DetectionKind Kind()     const noexcept override;
    std::chrono::milliseconds     Interval() const noexcept override;
    void Poll(hac::reporting::Reporter& out) override;
};

}} // namespace hac::detectors
