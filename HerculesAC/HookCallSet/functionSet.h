#ifndef _FUNCTION_SET_H
#define _FUNCTION_SET_H

void Hook_BaseThreadInitThunk();
void UnHook_BaseThreadInitThunk();

void Hook_CreateThread();
void UnHook_CreateThread();

void SetupHook();
void UnHook();
void InitFunctionPointer();

#endif // !_FUNCTION_SET_H