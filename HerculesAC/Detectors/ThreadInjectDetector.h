#pragma once
#include "IDetector.h"
#include <unordered_set>
#include <cstdint>

namespace hac { namespace detectors {

// Enumerates all threads in the current process and checks whether each
// thread's start address falls within a known-loaded module.  Threads whose
// start address is in anonymous/unrecognised memory regions are flagged as
// potential injections.
class ThreadInjectDetector final : public IDetector {
public:
    std::string_view              Name()     const noexcept override;
    hac::reporting::DetectionKind Kind()     const noexcept override;
    std::chrono::milliseconds     Interval() const noexcept override;
    void Poll(hac::reporting::Reporter& out) override;

private:
    std::unordered_set<DWORD> m_seenThreadIds;
};

}} // namespace hac::detectors
