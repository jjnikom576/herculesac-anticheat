#include "WinMain.h"
#include "HookCallSet/functionSet.h"
 #include "Globals.h"

PFN_BASETHREADINITTHUNK Original_BaseThreadInitThunk;
PFN_BASETHREADINITTHUNK BaseThreadInitThunk;
PFN_RTLEXITUSERTHREAD RtlExitUserThread;
PFN_CREATETHREAD Sys_CreateThread;