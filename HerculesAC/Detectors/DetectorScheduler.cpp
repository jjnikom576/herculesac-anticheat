#include "DetectorScheduler.h"
#include <chrono>
#include <thread>

namespace hac { namespace detectors {

DetectorScheduler::DetectorScheduler() = default;

DetectorScheduler::~DetectorScheduler()
{
    Stop();
}

void DetectorScheduler::Add(std::unique_ptr<IDetector> det)
{
    Entry e;
    e.det     = std::move(det);
    e.nextRun = std::chrono::steady_clock::now();
    m_entries.push_back(std::move(e));
}

void DetectorScheduler::Start(hac::reporting::Reporter& reporter)
{
    m_stop = false;
    m_worker = std::thread([this, &reporter] { Run(reporter); });
}

void DetectorScheduler::Stop()
{
    m_stop = true;
    if (m_worker.joinable())
        m_worker.join();
}

void DetectorScheduler::Run(hac::reporting::Reporter& reporter)
{
    while (!m_stop) {
        auto now = std::chrono::steady_clock::now();

        for (auto& entry : m_entries) {
            if (entry.deactivated || m_stop) continue;
            if (now < entry.nextRun) continue;

            try {
                entry.det->Poll(reporter);
                entry.consecutiveFailures = 0;
            } catch (...) {
                ++entry.consecutiveFailures;
                if (entry.consecutiveFailures >= 3) {
                    entry.deactivated = true;

                    hac::reporting::AntiCheatEvent ev{};
                    ev.severity         = hac::reporting::Severity::Warn;
                    ev.kind             = hac::reporting::DetectionKind::Unknown;
                    ev.detector_version = std::string(entry.det->Name()) + "@error";
                    ev.evidence_json    = "detector deactivated after 3 consecutive failures";
                    reporter.Emit(std::move(ev));
                }
            }

            entry.nextRun = now + entry.det->Interval();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

}} // namespace hac::detectors
