#include "../WinMain.h"
#include "ActiveThread.h"
#include "../../Common/Hash/MD5/MD5.h"

struct LANGANDCODEPAGE {
	WORD wLanguage;
	WORD wCodePage;
} *lpTranslate;

std::map<std::string, std::vector<std::string>> detectedHackMd5 = {
	{"ghinjector",{"0332C78554F9B4E9E08FB40B34A3DB4C"}},
	{"processhacker", {"3EA3E1CF4FE7F7E8D9F859CD5702EFDF"}},
	{"x64dbg",{"A9E393FE90F31F1EFAA75CF93E0DB0BE"}},
	{"ce",{"B1BD4F48A2C5550220EC09D82EB26E30"}},
	{"ida", {"1A09161BF4FC5DD8AEB99014E6925D6E"}},
	{"custom-cult-ce", {"7C0258C74E3D5B320CBDD607DA7027E2"}},
}; 

std::map<std::string, std::vector<std::string>> FileVersionInfoHash{
	{"OpenArk", {"AD28915D44D2622BF1CA1B899DA290E5", 
	"4D0F8AAD712D4E893CDEF426196C9647",
	"DA4CCD23B0371ECAC917C0C6BC02BB03",
	"FD0004DA38B96B1F537A8EBE57500B06",
	"FD0004DA38B96B1F537A8EBE57500B06"}},

	{"GH-Injector", {"9BA7E8FC8A34812DABEBED7013D0C951",
	"47994BD109A87320D2503EC3A07BCD15",
	"0873540FA25D7EEC5C483F849B874B4B",
	"1D9B5AE99EEEA6C78510755A6D25D1D7",
	"141B706D047A172172BA395675211E81",
	"E15F8B3F4011245D2944021129D4826C"}}
};


namespace CheckSum {
	DWORD FindWindowW_CheckSum;
	DWORD BaseThreadInitThunk_CheckSum;
	DWORD CreateThread_CheckSum;
}

// Initialize checksum
void InitCheckSum()
{
	CheckSum::FindWindowW_CheckSum = CRC32((BYTE*)FindWindowW, 5);
	CheckSum::BaseThreadInitThunk_CheckSum = CRC32((BYTE*)Original_BaseThreadInitThunk, 5);
	CheckSum::CreateThread_CheckSum = CRC32((BYTE*)CreateThread, 5);
}

// Verify the integrity of the code
BOOL CheckHookCode()
{
	if ((CheckSum::FindWindowW_CheckSum != CRC32((BYTE*)FindWindowW, 5)) ||
		(CheckSum::BaseThreadInitThunk_CheckSum != CRC32((BYTE*)Original_BaseThreadInitThunk, 5)) ||
		(CheckSum::CreateThread_CheckSum != CRC32((BYTE*)CreateThread, 5))
		)
	{
		// If the checksum is not equal, it means the function has been modified. Exit the game
		ExitGameProcess();
		return TRUE;
	}
	return FALSE;
}

std::string calculateResBinMD5(HMODULE hModule, LPWSTR lpName, LPCWSTR lpType)
{
	std::string sMd5;
	if (hModule)
	{
		HRSRC hResource = FindResource(hModule, lpName, lpType);
		if (hResource)
		{
			HGLOBAL hResourceData = LoadResource(hModule, hResource);
			if (hResourceData)
			{
				LPVOID pResourceData = LockResource(hResourceData);
				if (pResourceData)
				{
					DWORD resourceSize = SizeofResource(hModule, hResource);
					std::vector<BYTE> data(static_cast<BYTE*>(pResourceData), static_cast<BYTE*>(pResourceData) + resourceSize);
					sMd5 = calculateMD5(data);
					FreeResource(hResourceData);
				}
			}
		}
	}
	return sMd5;
}

BOOL CheckHackToolsByResBinHash(HMODULE hModule, LPWSTR lpName, LPCWSTR lpType)
{
	std::string sMd5 = calculateResBinMD5(hModule, lpName, lpType);
	for (const auto& pair : detectedHackMd5)
	{
		const std::vector<std::string>& values = pair.second;
		if (std::any_of(values.begin(), values.end(), [sMd5](const std::string& hash) {
			return hash == sMd5;
			}))
		{
			ReportIllegalInfo("hashCheck: " + pair.first);
			ExitGameProcess();
			return TRUE;
		}
	}
	return FALSE;
}

BOOL EnumResNameCallback(HMODULE hModule,
	LPCWSTR lpType,
	LPWSTR lpName,
	LONG_PTR lParam
)
{
	if (CheckHackToolsByResBinHash(hModule, lpName, lpType))
	{
		*(BOOL*)lParam = TRUE;
		return FALSE;
	}
	else
	{
		*(BOOL*)lParam = FALSE;
	}
	return TRUE;
}

BOOL CheckHackTools()
{
	BOOL boFoundHackTools = FALSE;
	std::vector<Common::ProcessInfo> processes = Common::EnumerateProcesses();
	if (processes.empty())
	{

	}
	else
	{
		for (const auto& process : processes)
		{
			if (_wcsicmp(process.processName.c_str(), _T("HerculesAC.aes")) != 0)  
			{
				HMODULE hModule = LoadLibraryEx(process.FullPath.c_str(), NULL, LOAD_LIBRARY_AS_DATAFILE);  
				if (hModule)
				{
					if (!boFoundHackTools)
					{
 						EnumResourceNames(hModule, RT_ICON, EnumResNameCallback, (LONG_PTR)&boFoundHackTools);
					}

					if (!boFoundHackTools)
					{
 						EnumResourceNames(hModule, RT_BITMAP, EnumResNameCallback, (LONG_PTR)&boFoundHackTools);
					}

					FreeLibrary(hModule);

					if (boFoundHackTools)
					{
						break;
					}
				}
			}
		}
	}
	return boFoundHackTools;
}

std::wstring GetVersionContent(const std::wstring& filePath, const std::wstring stringName)
{
	DWORD dwHandle;
	DWORD versionInfoSize = GetFileVersionInfoSize(filePath.c_str(), &dwHandle);
	if (versionInfoSize == 0) {
		return L"";
	}

	std::vector<BYTE> versionInfoBuffer(versionInfoSize);
	if (!GetFileVersionInfo(filePath.c_str(), 0, versionInfoSize, versionInfoBuffer.data())) {
		return L"";
	}

	UINT cbTranslate;


	VerQueryValue(versionInfoBuffer.data(),
		TEXT("\\VarFileInfo\\Translation"),
		(LPVOID*)&lpTranslate,
		&cbTranslate);

	// Read the file description for each language and code page.

	for (int i = 0; i < (cbTranslate / sizeof(struct LANGANDCODEPAGE)); i++)
	{
		WCHAR buffer[100];
		std::wstring SubBlock = L"\\StringFileInfo\\%04x%04x\\" + stringName;
		HRESULT hr = StringCchPrintf(buffer, _countof(buffer),
			SubBlock.c_str(),
			lpTranslate[i].wLanguage,
			lpTranslate[i].wCodePage);
		if (FAILED(hr))
		{
			// TODO: write error handler.
		}

		LPVOID fileDescriptionPtr;
		UINT fileDescriptionSize;
		if (VerQueryValue(versionInfoBuffer.data(), buffer, &fileDescriptionPtr, &fileDescriptionSize)) {
			std::wstring fileDescription(static_cast<const wchar_t*>(fileDescriptionPtr), fileDescriptionSize);
			return fileDescription;
		}
	}
	return L"";
}

BOOL CheckVersionContentHash(std::string target_hash)
{
	BOOL found = FALSE;
	for (const auto& pair : FileVersionInfoHash)
	{
		const std::vector<std::string>& values = pair.second;
		if (std::any_of(values.begin(), values.end(), [target_hash](const std::string& hash) {
			return hash == target_hash;
			}))
		{
			ReportIllegalInfo("VersionHashCheck: " + pair.first);
			ExitGameProcess();
			found = TRUE;
			break;
		}
	}
	return found;
}

BOOL CheckStringFileInfo(const std::wstring& filePath)
{
	std::vector<std::wstring> StringFileInfo;
	std::wstring Comments = GetVersionContent(filePath, L"Comments");
	std::wstring CompanyName = GetVersionContent(filePath, L"CompanyName");
	std::wstring FileDescription = GetVersionContent(filePath, L"FileDescription");
	std::wstring InternalName = GetVersionContent(filePath, L"InternalName");
	std::wstring LegalCopyright = GetVersionContent(filePath, L"LegalCopyright");
	std::wstring OriginalFilename = GetVersionContent(filePath, L"OriginalFilename");
	std::wstring ProductName = GetVersionContent(filePath, L"ProductName");

	StringFileInfo.push_back(Comments);
	StringFileInfo.push_back(CompanyName);
	StringFileInfo.push_back(FileDescription);
	StringFileInfo.push_back(InternalName);
	StringFileInfo.push_back(LegalCopyright);
	StringFileInfo.push_back(OriginalFilename);
	StringFileInfo.push_back(ProductName);

	for (const auto& FileInfo : StringFileInfo)
	{
		if (CheckVersionContentHash(calculateMD5(FileInfo.c_str())))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CheckVersionInfo()
{
	std::vector<Common::ProcessInfo> processes = Common::EnumerateProcesses();
	if (processes.empty())
	{

	}
	else
	{
		for (const auto& process : processes)
		{
			if (_wcsicmp(process.processName.c_str(), _T("HerculesAC.aes")) != 0)  // 不区分大小写比较
			{
				if (CheckStringFileInfo(process.FullPath))
				{
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

unsigned __stdcall ScannerThread(PVOID pArgList)
{
	for (;;)
	{
		if (CheckHackTools()){
			break;
		}

		if (CheckVersionInfo()) {
			break;
		}

		Sleep(100);
	}
	return 0;
}



// Detect illegal behavior
unsigned __stdcall DetectIllegalActivities(PVOID pArgList)
{
	for (;;)
	{
		if (CheckHookCode())
		{
			break;
		}
		Sleep(100);
	}
	return 0;
}



void InitThread()
{
	HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, DetectIllegalActivities, nullptr, 0, nullptr);
	CloseHandle(hThread);

	hThread = (HANDLE)_beginthreadex(nullptr, 0, ScannerThread, nullptr, 0, nullptr);
	CloseHandle(hThread);

}

void UnInitThread()
{

}