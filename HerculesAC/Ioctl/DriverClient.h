#pragma once

#include <Windows.h>

namespace hac { namespace driver {

// Opens \\.\HerculesAC and sends IOCTL_START_PROTECT for the given PID.
// Returns true on success.  Returns false (non-fatal) if the driver is not
// loaded, the caller lacks SeDebugPrivilege, or any OS error occurs.
bool StartProtect(DWORD pid);

}} // namespace hac::driver
