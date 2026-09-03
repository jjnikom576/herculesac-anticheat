#pragma once
#include "IDetector.h"
#include "../../Common/Reporting/Reporter.h"
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

namespace hac { namespace detectors {

class DetectorScheduler {
public:
    DetectorScheduler();
    ~DetectorScheduler();

    void Add(std::unique_ptr<IDetector> det);

    // Start the scheduler thread; |reporter| must outlive this object.
    void Start(hac::reporting::Reporter& reporter);
    void Stop();

private:
    struct Entry {
        std::unique_ptr<IDetector> det;
        std::chrono::steady_clock::time_point nextRun;
        int consecutiveFailures = 0;
        bool deactivated = false;
    };

    void Run(hac::reporting::Reporter& reporter);

    std::vector<Entry>   m_entries;
    std::thread          m_worker;
    std::atomic<bool>    m_stop{false};
};

}} // namespace hac::detectors
