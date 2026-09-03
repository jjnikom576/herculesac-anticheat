#pragma once

#ifndef _CALLBACKS_H
#define _CALLBACKS_H

VOID SetThreadCallbacks(IN PDRIVER_OBJECT pDriver_Object);
VOID SetProcessCallbacks(IN PDRIVER_OBJECT pDriver_Object);


VOID UnThreadCallbacks();
VOID UnProcessCallbacks();

#endif // !_CALLBACKS_H
