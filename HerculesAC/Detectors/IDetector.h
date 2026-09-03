#pragma once
#include <chrono>
#include <string_view>
#include "../../Common/Reporting/Event.h"
#include "../../Common/Reporting/Reporter.h"

namespace hac { namespace detectors {

class IDetector {
public:
    virtual ~IDetector() = default;

    virtual std::string_view              Name()     const noexcept = 0;
    virtual hac::reporting::DetectionKind Kind()     const noexcept = 0;
    virtual std::chrono::milliseconds     Interval() const noexcept = 0;

    // Called by the scheduler at each Interval(). Emit zero or more events
    // via |out|.  Must not throw — catch internally and return.
    virtual void Poll(hac::reporting::Reporter& out) = 0;
};

}} // namespace hac::detectors
