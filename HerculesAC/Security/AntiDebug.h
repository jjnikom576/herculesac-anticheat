#pragma once
#include <windows.h>

namespace hac { namespace security {

// Returns true if ANY debugger-presence indicator fires.
// Call from StartServer() loop; terminate game process on detection.
bool IsDebuggerAttached();

// Check once at startup; aborts process if debugger found during init.
void AssertNoDebugger();

}} // namespace hac::security
