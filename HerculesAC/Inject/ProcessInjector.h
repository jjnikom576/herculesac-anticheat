#pragma once
#include <Windows.h>

namespace hac { namespace inject {

// Opens the process by pid, allocates memory in it, writes dllPath, and
// creates a remote thread that calls LoadLibraryW(dllPath).
// Returns true if the remote thread was started successfully.
// Non-fatal on failure — caller decides whether to abort the session.
bool InjectDll(DWORD pid, const WCHAR* dllPath);

}} // namespace hac::inject
