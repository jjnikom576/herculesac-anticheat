#pragma once

#ifndef _HAC_PID_TABLE_H
#define _HAC_PID_TABLE_H

#include <ntifs.h>

namespace hac {

class PidTable
{
public:
    PidTable();
    ~PidTable();

    // Inserts a PID into the protected set. Idempotent — a duplicate
    // insert returns STATUS_SUCCESS. Returns STATUS_INSUFFICIENT_RESOURCES
    // on allocation failure.
    NTSTATUS Insert(ULONG pid);

    // Removes a PID. Returns STATUS_NOT_FOUND if the PID was not present.
    NTSTATUS Remove(ULONG pid);

    // Lookup — used from Ob pre-op callbacks (fires on any thread).
    BOOLEAN Contains(ULONG pid);

    // Snapshot count of protected PIDs.
    ULONG Count();

private:
    RTL_GENERIC_TABLE m_table;
    FAST_MUTEX        m_mutex;
};

} // namespace hac

#endif
