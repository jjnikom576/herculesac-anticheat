#pragma once

#ifndef _CRC32_H
#define _CRC32_H

////////////////////////////////////////////////////////////////
// Calculate the CRC32 value of a string
// Parameters: the first address and size of the string for which the CRC32 value is to be calculated
// Return value: Returns the CRC32 value

DWORD CRC32(BYTE* first_ptr, DWORD Size);

#endif // !_CRC32_H
