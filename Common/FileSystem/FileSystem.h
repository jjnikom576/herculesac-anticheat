#pragma once

#ifndef _FILE_SYSTEM_H
#define _FILE_SYSTEM_H

namespace FileSystem
{
    // Create an ini file
    void CreateIniFile(const std::wstring& filename);
    // Delete the ini file
    bool DeleteIniFile(const std::wstring& filename);
    // Traverse the files in the specified directory
    std::vector<std::wstring> TraverseDirectory(const std::wstring& directoryPath);
    std::wstring ReadIniValue(const std::wstring& filename, const std::wstring& section, const std::wstring& key);
    void WriteIniValue(const std::wstring& filename, const std::wstring& section, const std::wstring& key, const std::wstring& value);
    std::wstring GetModuleDirectory(HMODULE hModule);
    std::wstring GetModuleDirectory2(std::wstring path);
    // Get the name of the module itself
    std::string GetSelfModuleName();
}

#endif // !_FILE_SYSTEM_H
