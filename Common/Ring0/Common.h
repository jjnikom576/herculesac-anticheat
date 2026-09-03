#pragma once

#ifndef _COMMON_H
#define _COMMON_H

#define CPUID_1_ECX_VMX (1<<5)
#define CPUID_1_ECX_GUEST_STATUS (1<<31)

#define IA32_FEATURE_CONTROL_CODE		0x03A
#define FEATURE_CONTROL_VMXON_ENABLED_OUTSIDE_SMX	(1 << 2)

enum cpuid_reg
{
	eax,
	ebx,
	ecx,
	edx
};

namespace Common
{

	extern bool isIntel;
	extern bool isAMD;

	
	void ConfirmCPUVendor();

	BOOLEAN CheckVTSupported();

	BOOLEAN CheckVTEnabled();
}

#endif // !_Common_H
