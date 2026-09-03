#pragma once

#ifndef _MD5_H
#define _MD5_H

// Calculate the file md5
std::string calculateMD5(const std::string& filePath);
// Byte stream hash summary
std::string calculateMD5(const std::vector<unsigned char>& data);
// Hash the string
std::string calculateMD5(const TCHAR* inputParam);

#endif // !_MD5_H
