#include "HexDump.h"
#include <sstream>
#include <iomanip>

std::wstring HexDump::BufferToHex(const unsigned char* buffer, size_t size, bool& foundFFFE) {
    std::wstringstream ss;
    foundFFFE = false;
    for (size_t i = 0; i < size; ++i) {
        int val = buffer[i];
        if (i < size - 1 && buffer[i] == 0xFF && buffer[i + 1] == 0xFE) {
            ss << L"[FF FE] ";
            foundFFFE = true;
            i++;
        }
        else {
            ss << std::hex << std::setw(2) << std::setfill(L'0') << val << L" ";
        }
        if ((i + 1) % 16 == 0) ss << L"\n";
    }
    return ss.str();
}
