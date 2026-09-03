#pragma once
#include "IDetector.h"
#include <vector>
#include <string>

namespace hac { namespace detectors {

// Walks every running process and SHA-256 hashes the main module image.
// Flags any process whose image hash matches the known-bad list.
class ProcessSigDetector final : public IDetector {
public:
    explicit ProcessSigDetector(std::vector<std::string> known_bad_sha256_hex);

    std::string_view              Name()     const noexcept override;
    hac::reporting::DetectionKind Kind()     const noexcept override;
    std::chrono::milliseconds     Interval() const noexcept override;
    void Poll(hac::reporting::Reporter& out) override;

private:
    std::vector<std::string> m_badHashes;
};

}} // namespace hac::detectors
