#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <gdiplus.h>
#include <commdlg.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

// ################################
// #########=- VARIABLES -=########
// ################################
int xsize = 1500;
int ysize = 1000;
int xpos = 0;
int ypos = 0;
Image* pImage = nullptr;
size_t g_rawSize = 0;
std::wstring g_currentFilePath = L"";
std::wstring g_hiddenMessage = L"";
HWND g_hEditMessage = nullptr;  // Handle pour le champ de texte du message caché

// ################################
// ###=- EXTRACT HIDDEN TEXT -=####
// ################################

// Fonction pour extraire le texte caché après FF FE
std::wstring ExtractHiddenMessage(const unsigned char* buffer, size_t size) {
    // Chercher la séquence FF FE
    for (size_t i = 0; i < size - 1; ++i) {
        if (buffer[i] == 0xFF && buffer[i + 1] == 0xFE) {
            // Lire la longueur (2 octets little endian)
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
    return L"";  // Aucun message trouvé
}

// #############################
// #####=- IMAGE LOADING -=#####
// ############ BINARY #########
unsigned char* LoadFileToArray(const wchar_t* filename, size_t& size) {     // Function to load the image
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

// ################################
// #######=- HEX CONVERSION -=#####
// ################################
std::wstring BufferToHex(const unsigned char* buffer, size_t size, bool& foundFFFE) {   // Convert image to hex and write to txt file
    std::wstringstream ss;
    foundFFFE = false;

    for (size_t i = 0; i < size; ++i) {
        int val = buffer[i];
        if (i < size - 1 && buffer[i] == 0xFF && buffer[i + 1] == 0xFE) {   // Check if we're on "FF FE"
            ss << L"[FF FE] ";   // Textual highlight
            foundFFFE = true;    // Flag activated
            i++;
        }
        else {                                                              // Otherwise
            ss << std::hex << std::setw(2) << std::setfill(L'0') << val << L" ";
        }

        if ((i + 1) % 16 == 0) ss << L"\n"; // Line break every 16 values
    }
    return ss.str();
}

// ################################
// #####=- FF FE FUNCTIONS -=######
// ################################

// Function to check if FF FE already exists in the buffer
bool ContainsFFFE(const unsigned char* buffer, size_t size) {
    for (size_t i = 0; i < size - 1; ++i) {                     // Iterate through all bytes
        if (buffer[i] == 0xFF && buffer[i + 1] == 0xFE) {       // Until FF FE is found
            return true;    // If found, says there is a comment
        }
    }
    return false;           // Otherwise says there is no comment
}

// Function to find a safe insertion position
size_t FindSafeInsertionPosition(const unsigned char* buffer, size_t size) {
    // Look after JFIF/EXIF markers (usually after the first 20 bytes)
    if (size > 2 && buffer[0] == 0xFF && buffer[1] == 0xD8) {
        // It's a JPEG, insert after header (safe position: around 100-200 bytes)
        return (size > 150) ? 150 : (size / 2);
    }
    return size / 2;
}

bool InsertFFFeAndSave(const wchar_t* inputFile, const wchar_t* outputFile, size_t insertPos, const wchar_t* userText) {
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

// ################################
// ###########=- MAIN -=###########
// ################################
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    const wchar_t CLASS_NAME[] = L"Sample Window Class";        // Window name

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(         // Create the main window
        0,                              // Optional style
        CLASS_NAME,
        L"Steganography",               // Text displayed at the top of the window
        WS_OVERLAPPEDWINDOW,            // Window style
        xpos, ypos, xsize, ysize,       // Window configuration
        nullptr, nullptr, hInstance, nullptr
    );

    CreateWindow(               // Create an object
        L"BUTTON",              // Button type object
        L"Add a file",          // Text on the button
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,     // Style | PUSHBUTTON = button that sends a command
        5, 5, 150, 30,          // Position X, Y | Size X, Y
        hwnd,
        (HMENU)1,               // Button ID = 1
        hInstance,
        nullptr
    );

    CreateWindow(               // Create the second button
        L"BUTTON",              // Button type object
        L"Add a comment",       // Text on the button
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,     // Style | PUSHBUTTON
        160, 5, 150, 30,        // Position X, Y | Size X, Y
        hwnd,
        (HMENU)2,               // Button ID = 2
        hInstance,
        nullptr
    );

    // Label pour le champ de texte
    CreateWindow(
        L"STATIC",
        L"Secret message:",
        WS_VISIBLE | WS_CHILD,
        5, 145, 120, 20,
        hwnd,
        nullptr,
        hInstance,
        nullptr
    );

    // Champ de texte éditable pour le message caché
    g_hEditMessage = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        130, 143, 400, 24,
        hwnd,
        (HMENU)1001,
        hInstance,
        nullptr
    );

    if (!hwnd) {
        GdiplusShutdown(gdiplusToken);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow); // Display the window

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (pImage) delete pImage;
    GdiplusShutdown(gdiplusToken);
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        // ############## CASE #############
        // ##########=- DESTROY -=##########
        // #################################
    case WM_DESTROY:                            // Stop program if window is closed
        if (pImage) { delete pImage; pImage = nullptr; }
        PostQuitMessage(0);
        return 0;
        // ############## CASE #############
        // ############=- SIZE -=###########
        // #################################
    case WM_SIZE:
        xsize = LOWORD(lParam);                 // Get the X size of the window
        ysize = HIWORD(lParam);                 // Get the Y size of the window
        InvalidateRect(hwnd, nullptr, TRUE);       // Force a repaint
        break;
        // ############## CASE #############
        // ############=- MOVE -=###########
        // #################################
    case WM_MOVE:
        xpos = (int)(short)LOWORD(lParam);      // Get the X position of the window
        ypos = (int)(short)HIWORD(lParam);      // Get the Y position of the window
        InvalidateRect(hwnd, nullptr, TRUE);       // Force a repaint
        break;
        // ############## CASE #############
        // ##########=- COMMAND -=##########
        // #################################

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {  // Button "Add a file"
            OPENFILENAME ofn;

            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);    // Create an HDC object
            wchar_t szFile[260] = { 0 };

            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hwnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"Images\0*.JPG;*.PNG;*.BMP\0";
            ofn.nFilterIndex = 1;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            if (GetOpenFileName(&ofn)) {
                if (pImage) { delete pImage; pImage = nullptr; }
                pImage = new Image(szFile);

                if (pImage->GetLastStatus() != Ok) {
                    MessageBox(hwnd, L"Unable to load the image!", L"Error", MB_OK | MB_ICONERROR);
                    delete pImage; pImage = nullptr;
                }
                else {
                    InvalidateRect(hwnd, nullptr, TRUE);
                    g_currentFilePath = szFile;
                }

                // Also load the raw file in memory
                size_t rawSize = 0;
                unsigned char* rawData = LoadFileToArray(szFile, rawSize);
                if (rawData) {
                    g_rawSize = rawSize;
                    bool foundFFFE = false;
                    std::wstring hexDump = BufferToHex(rawData, rawSize, foundFFFE);

                    // Write to a text file
                    std::wofstream out(L"dump_hex.txt");
                    out << hexDump;
                    out.close();

                    wchar_t buffer[512];
                    if (foundFFFE) {
                        // ====> EXTRAIRE LE MESSAGE CACHÉ
                        std::wstring hiddenMessage = ExtractHiddenMessage(rawData, rawSize);

                        if (!hiddenMessage.empty()) {
                            wsprintf(buffer,
                                L"File loaded (%d bytes).\n\n"
                                L"🔍 SECRET MESSAGE DETECTED!\n\n"
                                L"Hidden text: \"%s\"\n\n"
                                L"FF FE sequence highlighted in dump_hex.txt",
                                (int)rawSize,
                                hiddenMessage.c_str());
                            MessageBox(hwnd, buffer, L"Secret code detected!", MB_OK | MB_ICONWARNING);
                            g_hiddenMessage = hiddenMessage;

                            // Mettre à jour le champ de texte avec le message caché
                            SetWindowText(g_hEditMessage, hiddenMessage.c_str());

                            InvalidateRect(hwnd, nullptr, TRUE);  // demander un repaint
                        }
                        else {
                            wsprintf(buffer,
                                L"File loaded (%d bytes).\n"
                                L"FF FE sequence found but no readable text after it.",
                                (int)rawSize);
                            MessageBox(hwnd, buffer, L"Secret code detected", MB_OK | MB_ICONINFORMATION);
                            SetWindowText(g_hEditMessage, L"");
                        }
                    }
                    else {
                        wsprintf(buffer, L"File loaded (%d bytes).\nNo FF FE sequence found.", (int)rawSize);
                        MessageBox(hwnd, buffer, L"Info", MB_OK | MB_ICONINFORMATION);
                        SetWindowText(g_hEditMessage, L"");
                    }

                    delete[] rawData;
                }
            }
        }
        else if (LOWORD(wParam) == 2) {  // Button "Add a comment"
            if (g_currentFilePath.empty()) {
                MessageBox(hwnd, L"No image loaded yet. Please load an image first with button {Add a file}.",
                    L"Error", MB_OK | MB_ICONERROR);
                break;
            }

            // Récupérer le texte depuis le champ de texte
            wchar_t userText[256] = { 0 };
            GetWindowText(g_hEditMessage, userText, 256);

            if (wcslen(userText) == 0) {
                MessageBox(hwnd, L"Please enter a message in the text field before saving.",
                    L"Error", MB_OK | MB_ICONWARNING);
                break;
            }

            size_t fileSize = 0;
            unsigned char* fileData = LoadFileToArray(g_currentFilePath.c_str(), fileSize);

            if (fileData) {
                size_t insertPos = FindSafeInsertionPosition(fileData, fileSize);
                delete[] fileData;

                std::wstring inputPath = g_currentFilePath;
                size_t lastDot = inputPath.find_last_of(L'.');
                std::wstring outputPath = inputPath.substr(0, lastDot) + L"_modified" + inputPath.substr(lastDot);

                if (InsertFFFeAndSave(g_currentFilePath.c_str(), outputPath.c_str(), insertPos, userText)) {
                    wchar_t msg[512];
                    wsprintf(msg, L"FF FE sequence + text successfully inserted!\n\nFile saved: %s\n\nPosition: %d bytes\n\nInserted text: %s",
                        outputPath.c_str(), (int)insertPos, userText);
                    MessageBox(hwnd, msg, L"Success", MB_OK | MB_ICONINFORMATION);
                }
                else {
                    MessageBox(hwnd, L"Failed to insert data or save file.", L"Error", MB_OK | MB_ICONERROR);
                }
            }
        }
        break;

    case WM_PAINT:
    {
        int imgW;   // Initialize image size | Horizontal
        int imgH;   // Initialize image size | Vertical
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);    // Create an HDC object
        Graphics graphics(hdc);             // Create a Graphics object

        // =-> BACKGROUND
        HBRUSH hBrush = CreateSolidBrush(RGB(27, 27, 27));   // Background color
        FillRect(hdc, &ps.rcPaint, hBrush);                  // Apply the color
        DeleteObject(hBrush);   // Delete the brush

        // =-> TEXT
        SetTextColor(hdc, RGB(255, 255, 255));  // Text color
        SetBkColor(hdc, RGB(0, 0, 0));          // Black background
        SetBkMode(hdc, OPAQUE);                 // Visible background
        wchar_t buffer[100];                    // Array containing a character string forming text

        // =-> IMAGE
        if (pImage) {   // If an image is loaded:
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int clientWidth = rcClient.right - rcClient.left;
            int clientHeight = rcClient.bottom - rcClient.top;

            int imgW = pImage->GetWidth();  // Vertical size of the window
            int imgH = pImage->GetHeight(); // Horizontal size of the window
            double imgRatio = (double)imgW / imgH;                  // Calculate image ratio
            double winRatio = (double)clientWidth / clientHeight;   // Calculate window ratio
            std::wstring fileName = g_currentFilePath.substr(g_currentFilePath.find_last_of(L"\\") + 1);

            int drawW, drawH;
            if (winRatio > imgRatio) {                  // Adjust image size according to horizontal window size
                drawH = clientHeight;
                drawW = (int)(clientHeight * imgRatio);
            }
            else {
                drawW = clientWidth;                    // Adjust image size according to vertical window size
                drawH = (int)(clientWidth / imgRatio);
            }

            int offsetX = (clientWidth - drawW) / 2;    // X offset of the image
            int offsetY = (clientHeight - drawH) / 2;   // Y offset of the image

            graphics.DrawImage(pImage, offsetX, offsetY, drawW, drawH);
            wsprintf(buffer, L"Image size: %d bytes", g_rawSize);       // Put image size in bytes in buffer
            TextOut(hdc, 5, 60, buffer, wcslen(buffer));                // Display image size in bytes
            wsprintf(buffer, L"Image size: %d x %d pixels", imgW, imgH);  // Put size in pixels in buffer
            TextOut(hdc, 5, 80, buffer, wcslen(buffer));                // Display image size in pixels
            wsprintf(buffer, L"File name: %s", fileName.c_str());
            TextOut(hdc, 5, 100, buffer, wcslen(buffer));
        }

        wsprintf(buffer, L"Window size: %d x %d  Position: %d x %d", xsize, ysize, xpos, ypos);    // Text put in buffer
        TextOut(hdc, 5, 40, buffer, wcslen(buffer));    // Display text
        EndPaint(hwnd, &ps);                            // "End" the display
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
