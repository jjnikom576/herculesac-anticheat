#include "Driver.h"
#include "Globals.h"
#include "Protect/Callback/Callbacks.h"

// WDM drivers have no CRT/atexit support. Global C++ objects with non-trivial
// destructors (e.g. hac::PidTable below) need atexit to register their
// destructor with the (nonexistent) exit sequence. Kernel drivers are unloaded
// by simply freeing the image, so an orderly static-destructor run is neither
// possible nor required — this stub only satisfies the linker.
extern "C" int __cdecl atexit(void(__cdecl*)(void))
{
    return 0;
}

PVOID g_obProcessHandle;
PVOID g_obThreadHandle;
hac::PidTable g_pidTable;
PDEVICE_OBJECT g_deviceObject = nullptr;
//PROTECT_OBJECT g_ProtectObjectList;

 