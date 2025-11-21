#include "StegCodec.h"
#include <fstream>
#include <sstream>
#include <cstring>

// Extracts a hidden message from a buffer if it contains the marker 0xFF 0xFE.
// The next two bytes : message length.
// Each character is stored as one byte, encoded by adding 3 (caesar).
std::wstring StegCodec::ExtractHiddenMessage(const unsigned char* buffer, size_t size) {
    for (size_t i = 0; i < size - 1; ++i) {
        if (buffer[i] == 0xFF && buffer[i + 1] == 0xFE) {
            if (i + 3 >= size) return L""; // not enough room for length
            uint16_t textLen = (buffer[i + 2] << 8) | buffer[i + 3];
            size_t textStart = i + 4;
            if (textStart + textLen <= size) {
                std::wstring result;
                result.reserve(textLen);
                for (size_t j = 0; j < textLen; ++j) {
                    unsigned char coded = buffer[textStart + j];
                    unsigned char decoded = static_cast<unsigned char>((coded - 3) & 0xFF);
                    result.push_back(static_cast<wchar_t>(decoded));
                }
                return result;
            }
        }
    }
    return L""; // no marker
}

// Checks if the buffer contains the marker FF FE.
bool StegCodec::ContainsFFFE(const unsigned char* buffer, size_t size) {
    for (size_t i = 0; i < size - 1; ++i) {
        if (buffer[i] == 0xFF && buffer[i + 1] == 0xFE) return true;
    }
    return false;
}

// Chooses a safe insertion position for hidden data.
// If file starts with (FF D8), insert near the beginning (150 bytes in).
// Else, insert in the middle.
size_t StegCodec::FindSafeInsertionPosition(const unsigned char* buffer, size_t size) {
    if (size > 2 && buffer[0] == 0xFF && buffer[1] == 0xD8) {
        return (size > 150) ? 150 : (size / 2);
    }
    return size / 2;
}

// Loads a file into memory as byte array.
unsigned char* StegCodec::LoadFileToArray(const wchar_t* filename, size_t& size) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        size = 0;
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    unsigned char* buffer = new unsigned char[size];
    file.read(reinterpret_cast<char*>(buffer), size);
    return buffer;
}

// Inserts a hidden message into a file at a given position and saves to a new file.
// Message is stored as: [FF FE][length][hidden message].
bool StegCodec::InsertFFFeAndSave(const wchar_t* inputFile, const wchar_t* outputFile,
    size_t insertPos, const wchar_t* userText) {
    // Load original file
    size_t originalSize = 0;
    unsigned char* originalData = LoadFileToArray(inputFile, originalSize);
    if (!originalData) return false;

    // If marker already exists, remove old message and reuse position
    size_t existingPos = SIZE_MAX;
    for (size_t i = 0; i < originalSize - 1; ++i) {
        if (originalData[i] == 0xFF && originalData[i + 1] == 0xFE) {
            existingPos = i;
            break;
        }
    }
    if (existingPos != SIZE_MAX) {
        uint16_t oldLen = (originalData[existingPos + 2] << 8) | originalData[existingPos + 3];
        size_t oldBlockSize = 4 + oldLen;
        memmove(originalData + existingPos,
            originalData + existingPos + oldBlockSize,
            originalSize - (existingPos + oldBlockSize));
        originalSize -= oldBlockSize;
        insertPos = existingPos;
    }

    // Ensure insertion position is valid
    if (insertPos > originalSize) insertPos = originalSize;

    // Convert text to  ASCII ( +3 encoding )
    size_t textLen = wcslen(userText);
    char* asciiText = new char[textLen + 1];
    for (size_t i = 0; i < textLen; ++i) {
        unsigned char c = static_cast<unsigned char>(userText[i] & 0xFF);
        asciiText[i] = static_cast<char>((c + 3) & 0xFF);
    }
    asciiText[textLen] = '\0';
    size_t textBytes = textLen;

    // new buffer with marker + length + text inserted
    size_t newSize = originalSize + 4 + textBytes;
    unsigned char* newData = new unsigned char[newSize];
    memcpy(newData, originalData, insertPos);
    newData[insertPos] = 0xFF;
    newData[insertPos + 1] = 0xFE;
    newData[insertPos + 2] = (textLen >> 8) & 0xFF;
    newData[insertPos + 3] = textLen & 0xFF;
    memcpy(&newData[insertPos + 4], asciiText, textBytes);
    memcpy(newData + insertPos + 4 + textBytes,
        originalData + insertPos,
        originalSize - insertPos);

    // Save to output file
    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile) {
        delete[] originalData;
        delete[] newData;
        delete[] asciiText;
        return false;
    }
    outFile.write(reinterpret_cast<char*>(newData), newSize);
    outFile.close();

    // Clean up
    delete[] originalData;
    delete[] newData;
    delete[] asciiText;
    return true;
}
