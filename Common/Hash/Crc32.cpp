#include <iostream>
#include <Windows.h>
#include "../vmp.h"
#include "Crc32.h"

////////////////////////////////////////////////////////////////
// Calculate the CRC32 value of a string
// Parameters: the first address and size of the string for which the CRC32 value is to be calculated
// Return value: Returns the CRC32 value

DWORD CRC32(BYTE* first_ptr, DWORD Size)
{
	VMProtectionScope vmpScope;

	DWORD crcTable[256], crcTmp1;

	// Dynamically generate CRC-32 table
	for (int i = 0; i < 256; i++)
	{
		crcTmp1 = i;
		for (int j = 8; j > 0; j--)
		{
			if (crcTmp1 & 1) crcTmp1 = (crcTmp1 >> 1) ^ 0xEDB88320L;
			else crcTmp1 >>= 1;
		}

		crcTable[i] = crcTmp1;
	}
	// Calculate CRC32 value
	DWORD crcTmp2 = 0xFFFFFFFF;
	while (Size--)
	{
		crcTmp2 = ((crcTmp2 >> 8) & 0x00FFFFFF) ^ crcTable[(crcTmp2 ^ (*first_ptr)) & 0xFF];
		first_ptr++;
	}

	return ~(crcTmp2 ^ 0xFFFFFFFF);
}