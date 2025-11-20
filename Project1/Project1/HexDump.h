#pragma once
#include <string>

class HexDump {
public:
    static std::wstring BufferToHex(const unsigned char* buffer, size_t size, bool& foundFFFE);
};
