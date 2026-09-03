#pragma once
#include "IDetector.h"

namespace hac { namespace detectors {

// Enumerates loaded kernel modules via NtQuerySystemInformation and flags any
// that match a curated list of known cheat-engine driver names.
class DriverEnumDetector final : public IDetector {
public:
    std::string_view              Name()     const noexcept override;
    hac::reporting::DetectionKind Kind()     const noexcept override;
    std::chrono::milliseconds     Interval() const noexcept override;
    void Poll(hac::reporting::Reporter& out) override;
};

}} // namespace hac::detectors
