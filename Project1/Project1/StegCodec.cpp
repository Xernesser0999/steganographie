#include "StegCodec.h"
#include <fstream>
#include <sstream>
#include <cstring>

std::wstring StegCodec::ExtractHiddenMessage(const unsigned char* buffer, size_t size) {
    for (size_t i = 0; i < size - 1; ++i) {
        if (buffer[i] == 0xFF && buffer[i + 1] == 0xFE) {
            if (i + 3 >= size) return L"";
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
    return L"";
}

bool StegCodec::ContainsFFFE(const unsigned char* buffer, size_t size) {
    for (size_t i = 0; i < size - 1; ++i) {
        if (buffer[i] == 0xFF && buffer[i + 1] == 0xFE) return true;
    }
    return false;
}

size_t StegCodec::FindSafeInsertionPosition(const unsigned char* buffer, size_t size) {
    if (size > 2 && buffer[0] == 0xFF && buffer[1] == 0xD8) {
        return (size > 150) ? 150 : (size / 2);
    }
    return size / 2;
}

unsigned char* StegCodec::LoadFileToArray(const wchar_t* filename, size_t& size) {     // Function to load the image
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

bool StegCodec::InsertFFFeAndSave(const wchar_t* inputFile, const wchar_t* outputFile, size_t insertPos, const wchar_t* userText) {
    // 1. Load the original file
    size_t originalSize = 0;
    unsigned char* originalData = LoadFileToArray(inputFile, originalSize);

    if (!originalData) {
        return false;
    }

    // 2. Chercher si FF FE existe déjà
    size_t existingPos = SIZE_MAX;
    for (size_t i = 0; i < originalSize - 1; ++i) {
        if (originalData[i] == 0xFF && originalData[i + 1] == 0xFE) {
            existingPos = i;
            break;
        }
    }

    // Si trouvé, supprimer l'ancien message
    if (existingPos != SIZE_MAX) {
        uint16_t oldLen = (originalData[existingPos + 2] << 8) | originalData[existingPos + 3];
        size_t oldBlockSize = 4 + oldLen;

        memmove(originalData + existingPos,
            originalData + existingPos + oldBlockSize,
            originalSize - (existingPos + oldBlockSize));

        originalSize -= oldBlockSize;
        insertPos = existingPos; // reuse same position
    }


    // 3. Limit insertion position
    if (insertPos > originalSize) {
        insertPos = originalSize;
    }

    // ========== CONVERSION wchar_t -> ASCII ==========
    size_t textLen = wcslen(userText);
    char* asciiText = new char[textLen + 1];

    // Convertir caractère par caractère (prend seulement l'octet de poids faible)
    for (size_t i = 0; i < textLen; ++i) {
        unsigned char c = static_cast<unsigned char>(userText[i] & 0xFF);
        asciiText[i] = static_cast<char>((c + 3) & 0xFF); // ajout de 3 et masque sur 1 octet
    }

    asciiText[textLen] = '\0';

    size_t textBytes = textLen;  // Maintenant c'est 1 octet par caractère
    // =================================================

    // 5. Create a new buffer with 2 extra bytes + text
    size_t newSize = originalSize + 4 + textBytes;
    unsigned char* newData = new unsigned char[newSize];

    // 6. Copy data before insertion position
    memcpy(newData, originalData, insertPos);

    // 7. Insert FF FE marker
    newData[insertPos] = 0xFF;
    newData[insertPos + 1] = 0xFE;
    uint16_t textLen16 = static_cast<uint16_t>(textBytes);
    newData[insertPos + 2] = (textLen >> 8) & 0xFF;
    newData[insertPos + 3] = textLen16 & 0xFF;

    // 8. Insert ASCII text after FF FE
    memcpy(&newData[insertPos + 4], asciiText, textBytes);

    // 9. Copy the rest of the data
    memcpy(newData + insertPos + 4 + textBytes, originalData + insertPos, originalSize - insertPos);

    // 10. Save to new file
    std::ofstream outFile(outputFile, std::ios::binary);
    if (!outFile) {
        delete[] originalData;
        delete[] newData;
        delete[] asciiText;
        return false;
    }

    outFile.write(reinterpret_cast<char*>(newData), newSize);
    outFile.close();

    // 11. Clean memory
    delete[] originalData;
    delete[] newData;
    delete[] asciiText;

    return true;
}
