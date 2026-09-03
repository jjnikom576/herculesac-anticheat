#include <iostream>
#include <Windows.h>
#include <string>
#include <codecvt>
#include <random>
#include <tuple>
#include <TlHelp32.h>
#include <vector>
#include <psapi.h>
#include <intrin.h>
#include <array>
#include "Common.h"

namespace Common
{

	HANDLE hMutex; // Prevent multiple openings
	bool isIntel = false;
	bool isAMD = false;

	// Convert string to wstring
	std::wstring stringToWideString(const std::string& narrowStr)
	{
		// Get the length of the wide character string (including the null terminator)
		int wideStrLength = MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, nullptr, 0);

		// Allocate memory to store wide character strings
		wchar_t* wideStr = new wchar_t[wideStrLength];

		// Convert narrow characters to wide characters
		MultiByteToWideChar(CP_UTF8, 0, narrowStr.c_str(), -1, wideStr, wideStrLength);

		// Create a std::wstring object
		std::wstring result(wideStr);

		// Release memory
		delete[] wideStr;

		return result;
	}

	// Convert wstring to string
	std::string wideStringToString(const std::wstring& wideStr)
	{
		int bufferSize = WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string str(bufferSize - 1, 0);
		WideCharToMultiByte(CP_ACP, 0, wideStr.c_str(), -1, &str[0], bufferSize - 1, nullptr, nullptr);
		return str;
	}

	// Convert wchar_t* to string
	std::string wcharToString(const wchar_t* str)
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
		return converter.to_bytes(str);
	}

	// Convert wchar_t* to wstring
	std::wstring wcharToWideString(const wchar_t* wcharStr)
	{
		// Use the constructor to convert wchar_t* to std::wstring
		std::wstring wideStr(wcharStr);

		return wideStr;
	}

	// char* to wchar_t*
	std::wstring ConvertCharToWchar(const char* charStr)
	{
		const int charStrLength = strlen(charStr) + 1; // char length of the string (including the null terminator)

		// Calculate the buffer size required for the wchar_t string
		const int wcharStrSize = MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLength, nullptr, 0);

		// Allocate wchar_t buffer
		wchar_t* wcharStr = new wchar_t[wcharStrSize];

		// Perform the conversion
		MultiByteToWideChar(CP_UTF8, 0, charStr, charStrLength, wcharStr, wcharStrSize);

		// Encapsulate wchar_t string into std::wstring type
		std::wstring result(wcharStr);

		// Release memory
		delete[] wcharStr;

		return result;
	}

	// gbk to utf8
	std::string GbkToUTF8(const std::string& gbkString)
	{
		int bufferSize = MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, nullptr, 0);
		std::wstring wideString(bufferSize - 1, L'\0');
		MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, &wideString[0], bufferSize - 1);

		bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string utf8String(bufferSize - 1, '\0');
		WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], bufferSize - 1, nullptr, nullptr);

		return utf8String;
	}

	// gbk to utf8
	//std::string GbkToUTF8(const std::string& gbkString)
	//{
	//	int bufferSize = MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, nullptr, 0);
	//	std::wstring wideString(bufferSize, L'\0');
	//	MultiByteToWideChar(CP_ACP, 0, gbkString.c_str(), -1, &wideString[0], bufferSize);

	//	bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
	//	std::string utf8String(bufferSize, '\0');
	//	WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], bufferSize, nullptr, nullptr);

	//	return utf8String;
	//}

	// Convert the utf8 encoded string to GBK encoding
	std::string utf8ToGbk(const std::string& utf8String)
	{
		int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
		if (bufferSize == 0)
		{
			// If the conversion fails, you can handle the error according to the actual situation
			return "";
		}

		std::wstring wideString(bufferSize, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &wideString[0], bufferSize);

		bufferSize = WideCharToMultiByte(CP_ACP, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (bufferSize == 0)
		{
			// If the conversion fails, you can handle the error according to the actual situation
			return "";
		}

		std::string gbkString(bufferSize, '\0');
		WideCharToMultiByte(CP_ACP, 0, wideString.c_str(), -1, &gbkString[0], bufferSize, nullptr, nullptr);

		return gbkString;
	}

	// Convert utf8 encoded string to Unicode encoding
	std::wstring utf8ToUnicode(const std::string& utf8String)
	{
		int bufferSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
		std::wstring unicodeString(bufferSize, 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &unicodeString[0], bufferSize);
		return unicodeString;
	}

	// Convert local code page to std::wstring
	std::wstring ConvertLocalCodePageToWideString(const std::string& str)
	{
		int wideStrLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
		if (wideStrLen == 0)
		{
			// If the conversion fails, you can handle the error according to the actual situation
			return L"";
		}

		std::wstring wideStr(wideStrLen, L'\0');
		if (MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wideStr[0], wideStrLen) == 0)
		{
			// If the conversion fails, you can handle the error according to the actual situation
			return L"";
		}

		// Remove the trailing null character
		wideStr.resize(wideStrLen - 1);

		return wideStr;
	}

	// Convert local code page to std::string
	std::string LocalCodePageToUtf8(const std::string& localString)
	{
		int wideCharLength = MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, nullptr, 0);
		if (wideCharLength == 0) {
			// Conversion failed
			return "";
		}

		std::wstring wideString(wideCharLength, L'\0');
		if (MultiByteToWideChar(CP_ACP, 0, localString.c_str(), -1, &wideString[0], wideCharLength) == 0) {
			// Conversion failed
			return "";
		}

		int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (utf8Length == 0) {
			// Conversion failed
			return "";
		}

		std::string utf8String(utf8Length, '\0');
		if (WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) == 0) {
			// Conversion failed
			return "";
		}

		return utf8String;
	}

	// Unicode to Utf8
	std::string UnicodeToUtf8(const std::wstring& unicodeString)
	{
		int utf8Length = WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (utf8Length == 0) {
			// Conversion failed
			return "";
		}

		std::string utf8String(utf8Length, '\0');
		if (WideCharToMultiByte(CP_UTF8, 0, unicodeString.c_str(), -1, &utf8String[0], utf8Length, nullptr, nullptr) == 0) {
			// Conversion failed
			return "";
		}

		return utf8String;
	}

	// Generate a 16-bit random string
	std::string generateRandomString()
	{
		const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		const int length = 16;

		std::random_device rd;
		std::mt19937 generator(rd());
		std::uniform_int_distribution<int> distribution(0, characters.length() - 1);

		std::string randomString;

		for (int i = 0; i < length; ++i) {
			randomString += characters[distribution(generator)];
		}

		return randomString;
	}

	// String interception
	std::string truncateString(const std::string& input, int length)
	{
		if (length >= input.length())
		{
			return input;
		}
		else
		{
			return input.substr(0, length);
		}
	}

	// Intercept the string and the remaining string
	std::tuple<std::string, std::string> truncateString2(const std::string& input, int length)
	{
		if (length >= input.length())
		{
			return std::make_tuple(input, "");
		}
		else
		{
			return std::make_tuple(input.substr(0, length), input.substr(length));
		}
	}

	// Convert string to lowercase
	std::string ToLowerWindows(const std::string& str)
	{
		std::string lowerStr(str);
		CharLowerBuffA(&lowerStr[0], static_cast<DWORD>(lowerStr.size()));

		return lowerStr;
	}

	// Convert wstring to lowercase
	std::wstring ToLowerWindows(const std::wstring& str)
	{
		std::wstring lowerStr(str);
		CharLowerBuffW(&lowerStr[0], static_cast<DWORD>(lowerStr.size()));

		return lowerStr;
	}

	// Enumerate the processes
	std::vector<ProcessInfo> EnumerateProcesses()
	{
		std::vector<ProcessInfo> processes;

		// Get a snapshot of all processes in the system
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot == INVALID_HANDLE_VALUE)
		{
			// Return an empty container
			return processes;
		}

		PROCESSENTRY32W processEntry = { sizeof(PROCESSENTRY32W) };

		// Enumerate process information in the process snapshot
		if (Process32First(hSnapshot, &processEntry))
		{
			do
			{
				ProcessInfo process;
				process.processId = processEntry.th32ProcessID;
				process.processName = processEntry.szExeFile;

				// Open the process
				HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processEntry.th32ProcessID);
				if (hProcess != nullptr)
				{
					TCHAR modulePath[MAX_PATH] = { 0 };
					if (GetModuleFileNameEx(hProcess, NULL, modulePath, MAX_PATH))
					{
						process.FullPath = modulePath;
					}
					CloseHandle(hProcess);
				}
				processes.push_back(process);
			} while (Process32Next(hSnapshot, &processEntry));
		}

		// Close the process snapshot handle
		CloseHandle(hSnapshot);

		return processes;
	}

	// Check if the target process is running
	BOOL IsProcessRunning(const std::wstring& processName)
	{
		BOOL boRet = FALSE;
		PROCESSENTRY32W entry;
		entry.dwSize = sizeof(PROCESSENTRY32W);

		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnapshot != INVALID_HANDLE_VALUE)
		{
			if (Process32FirstW(hSnapshot, &entry))
			{
				do
				{
					std::wstring currentProcessName = Common::ToLowerWindows(entry.szExeFile);
					if (currentProcessName.find(Common::ToLowerWindows(processName)) != std::wstring::npos) // Find substring
					{
						boRet = TRUE;
						break;
					}
				} while (Process32NextW(hSnapshot, &entry));
			}
			CloseHandle(hSnapshot);
		}
		return boRet;
	}

	// Find window information
	BOOL FindWindowInfo(LPCWSTR lpClassName, LPCWSTR titleName)
	{
		if (FindWindow(lpClassName, titleName))
		{
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	// Terminate the process
	bool TerminateWindowsProcess(DWORD processId)
	{
		/*
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
		if (hProcess == NULL)
		{
			// Handle the situation where opening the process fails
			return false;
		}

		// Terminate the process
		bool result = TerminateProcess(hProcess, 0);

		// Close the process handle
		CloseHandle(hProcess);

		return result;
		*/
		return TerminateProcessTree(processId);

	}


	bool TerminateProcessTree(DWORD processId)
	{
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hSnap == INVALID_HANDLE_VALUE)
			return false;

		PROCESSENTRY32 pe;
		pe.dwSize = sizeof(PROCESSENTRY32);
		bool success = true;

		if (Process32First(hSnap, &pe))
		{
			do
			{
				if (pe.th32ParentProcessID == processId)
				{
					// Recursively terminate child processes first
					if (!TerminateProcessTree(pe.th32ProcessID))
					{
						success = false;
					}
				}
			} while (Process32Next(hSnap, &pe));
		}
		CloseHandle(hSnap);

		// Terminate the main process
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processId);
		if (hProcess)
		{
			if (!TerminateProcess(hProcess, 0))
			{
				CloseHandle(hProcess);
				return false;
			}
			CloseHandle(hProcess);
		}
		else
		{
			return false;
		}

		return success;
	}

// Singleton mode
// Prevent multiple programs from opening
	BOOL SingletonPattern(const wchar_t* mutexName)
	{
		BOOL boRet = FALSE;

		// Create a mutex
		hMutex = CreateMutexW(nullptr, TRUE, mutexName);

		// Check if the mutex already exists
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			// Close the mutex handle and exit the program
			CloseHandle(hMutex);
		}
		else
		{
			boRet = TRUE;;
		}
		return boRet;
	}

	// Exit the singleton
	void SingletonProgramEnd()
	{
		// Close the mutex handle
		if (hMutex)
		{
			CloseHandle(hMutex);
		}		
	}

	// int to wstring
	std::wstring IntToWString(int value)
	{
		return std::to_wstring(value);
	}

	// Convert wstring to int
	int WStringToInt(const std::wstring& str)
	{
		return std::stoi(str);
	}

	// Confirm the CPU model
	void ConfirmCPUVendor()
	{
		std::array<int, 4> cpui;

		// Calling __cpuid with 0x0 as the function_id argument
		// gets the number of the highest valid function ID.
		__cpuid(cpui.data(), 0);

		// Capture vendor string
		char vendor[0x20];
		memset(vendor, 0, sizeof(vendor));
		*reinterpret_cast<int*>(vendor) = cpui[ebx];
		*reinterpret_cast<int*>(vendor + 4) = cpui[edx];
		*reinterpret_cast<int*>(vendor + 8) = cpui[ecx];
		std::string vendor_ = vendor;
		if (vendor_ == "GenuineIntel")
		{
			isIntel = true;
		}
		else if (vendor_ == "AuthenticAMD")
		{
			isAMD = true;
		}
	}

}