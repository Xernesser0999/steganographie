#include "HexDump.h"
#include <sstream>
#include <iomanip>

// Converts a byte buffer into a hex string.
// Marks [FF FE] if that sequence is found (UTF?16 BOM).
std::wstring HexDump::BufferToHex(const unsigned char* buffer, size_t size, bool& foundFFFE) {
    std::wstringstream ss;   // Build wide string output
    foundFFFE = false;

    for (size_t i = 0; i < size; ++i) {
        int val = buffer[i];

        // Detect 0xFF 0xFE sequence
        if (i < size - 1 && buffer[i] == 0xFF && buffer[i + 1] == 0xFE) {
            ss << L"[FF FE] ";
            foundFFFE = true;
            i++; // skip next byte
        }
        else {
            // Print byte as two?digit hex
            ss << std::hex << std::setw(2) << std::setfill(L'0') << val << L" ";
        }

        // Newline every 16 bytes
        if ((i + 1) % 16 == 0) ss << L"\n";
    }

    return ss.str();
}
