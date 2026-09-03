#include "DriverClient.h"
#include "../../Common/Shared/IOCTLs.h"
#include <cstring>

namespace hac { namespace driver {

bool StartProtect(DWORD pid)
{
    HANDLE h = CreateFileW(
        HAC_DEVICE_USERMODE_PATH,
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    IOCTL_START_PROTECT_REQUEST req = {};
    req.pid = static_cast<ULONG>(pid);

    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(
        h, IOCTL_START_PROTECT,
        &req, sizeof(req),
        nullptr, 0,
        &bytesReturned, nullptr);
    CloseHandle(h);
    return ok != FALSE;
}

}} // namespace hac::driver
