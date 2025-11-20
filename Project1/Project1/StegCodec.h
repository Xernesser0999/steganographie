#pragma once
#include <string>
#include <vector>

class StegCodec {
public:
    static std::wstring ExtractHiddenMessage(const unsigned char* buffer, size_t size);
    static bool ContainsFFFE(const unsigned char* buffer, size_t size);
    static size_t FindSafeInsertionPosition(const unsigned char* buffer, size_t size);
    static bool InsertFFFeAndSave(const wchar_t* inputFile, const wchar_t* outputFile, size_t insertPos, const wchar_t* userText);
    static unsigned char* LoadFileToArray(const wchar_t* filename, size_t& size);

};
