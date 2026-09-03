#pragma once

#ifndef _COMMON_H
#define _COMMON_H

enum IllegalEnum
{
	Illegal_detectedHackTools = 1998,
};

enum cpuid_reg
{
	eax,
	ebx,
	ecx,
	edx
};

namespace Common
{
	struct ProcessInfo
	{
		DWORD processId;
		std::wstring processName;
		std::wstring FullPath;
	};

	extern bool isIntel;
	extern bool isAMD;

	//stringwstring
	std::wstring stringToWideString(const std::string& narrowStr);
	//wstringstring
	std::string wideStringToString(const std::wstring& wideStr);
	//wchar_t*string
	std::string wcharToString(const wchar_t* str);
	//wchar_t*wstring
	std::wstring wcharToWideString(const wchar_t* wcharStr);
	//char*wchar_t*
	std::wstring ConvertCharToWchar(const char* charStr);
	//gbkutf8
	std::string GbkToUTF8(const std::string& gbkString);
	//utf8GBK
	std::string utf8ToGbk(const std::string& utf8String);
	//utf8Unicode
	std::wstring utf8ToUnicode(const std::string& utf8String);
	//std::wstring
	std::wstring ConvertLocalCodePageToWideString(const std::string& str);
	//std::string
	std::string LocalCodePageToUtf8(const std::string& localString);
	//Unicodeutf8
	std::string UnicodeToUtf8(const std::wstring& unicodeString);
	//16
	std::string generateRandomString();
	//
	std::string truncateString(const std::string& input, int length);
	// 
	std::tuple<std::string, std::string> truncateString2(const std::string& input, int length);
	//string
	std::string ToLowerWindows(const std::string& str);
	//wstring
	std::wstring ToLowerWindows(const std::wstring& str);
	//
	std::vector<ProcessInfo> EnumerateProcesses();
	//
	BOOL IsProcessRunning(const std::wstring& processName);
	//
	BOOL FindWindowInfo(LPCWSTR lpClassName, LPCWSTR titleName);
	//
	bool TerminateWindowsProcess(DWORD processId);
	//
	//
	BOOL SingletonPattern(const wchar_t* mutexName);
	//
	void SingletonProgramEnd();
	//intwstring
	std::wstring IntToWString(int value);
	//wstringint
	int WStringToInt(const std::wstring& str);
	//CPU
	void ConfirmCPUVendor();

	bool TerminateProcessTree(DWORD processId);

}


#endif // !_COMMON_H
