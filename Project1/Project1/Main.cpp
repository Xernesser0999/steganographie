#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>
#include <gdiplus.h>
#include <commdlg.h>
#include <fstream>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

int xsize = 1500;
int ysize = 1000;
int xpos = 0;
int ypos = 0;
Image* pImage = NULL;  // Variable globale pour stocker l'image GDI+
size_t g_rawSize = 0;   // taille du fichier en octets

// ################################
// #####=- CHARGEMENT IMAGE -=#####
// ############ BINAIRE ###########
unsigned char* LoadFileToArray(const wchar_t* filename, size_t& size) {
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
// #######=- HEXA CONVERSION -=####
// ################################

// Retourne le dump hex et indique si la séquence FF FE est trouvée
std::wstring BufferToHex(const unsigned char* buffer, size_t size, bool& foundSequence) {
    std::wstringstream ss;
    foundSequence = false;

    for (size_t i = 0; i < size; ++i) {
        if (i + 1 < size && buffer[i] == 0xFF && buffer[i + 1] == 0xFE) {
            ss << L"[";
            ss << std::hex << std::setw(2) << std::setfill(L'0') << (int)buffer[i] << L" ";
            ss << std::hex << std::setw(2) << std::setfill(L'0') << (int)buffer[i + 1];
            ss << L"] ";
            i++;
            foundSequence = true; // ✅ séquence détectée
        }
        else {
            ss << std::hex << std::setw(2) << std::setfill(L'0') << (int)buffer[i] << L" ";
        }

        if ((i + 1) % 16 == 0) ss << L"\n";
    }
    return ss.str();
}


// ################################
// ###########=- MAIN -=###########
// ################################

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // =-> INITIALISATION GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    const wchar_t CLASS_NAME[] = L"Sample Window Class";        // Nom de la fenètre

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(         // Création de la fenètre de base
        0,                              // Style optionel
        CLASS_NAME,
        L"Stéganographie",              // Texte affiché en haut de la fenètre
        WS_OVERLAPPEDWINDOW,            // Style de la fenètre
        xpos, ypos, xsize, ysize,       // Configuration de la fenètre
        NULL, NULL, hInstance, NULL
    );

    CreateWindow(               // Création d'un objet
        L"BUTTON",              // Objet de type bouton
        L"Ajouter un fichier",  // Texte sur le bouton
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,     // Style | PUSHBUTON = bouton qui renvoie une commande
        5, 5, 150, 30,          // Position X, Y | Taille X, Y
        hwnd,
        (HMENU)1,               // ID du bouton = 1
        hInstance,
        NULL
    );

    CreateWindow(               // Création du deuxieme bouton
        L"BUTTON",              // Objet de type bouton
        L"Télécharger l'image", // Texte sur le bouton
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,     // Style | PUSHBUTTON
        160, 5, 150, 30,        // Position X, Y | Taille X, Y
        hwnd,
        (HMENU)2,               // ID du bouton = 2
        hInstance,
        NULL
    );

    if (!hwnd) {
        GdiplusShutdown(gdiplusToken);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow); // Affiche la fenètre

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
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
            // Arret du programme si fenètre fermé
    case WM_DESTROY:
        if (pImage) { delete pImage; pImage = NULL; }
        PostQuitMessage(0);
        return 0;

        // ############## CASE #############
        // ############=- SIZE -=###########
        // #################################
    case WM_SIZE:
        xsize = LOWORD(lParam);                 // Récupère la taille X de la fenètre
        ysize = HIWORD(lParam);                 // Récupère la taille Y de la fenètre
        InvalidateRect(hwnd, NULL, TRUE);       // Force un repaint
        break;

        // ############## CASE #############
        // ############=- MOVE -=###########
        // #################################
    case WM_MOVE:
    {
        xpos = (int)(short)LOWORD(lParam);      // Récupère la position X de la fenètre
        ypos = (int)(short)HIWORD(lParam);      // Récupère la position Y de la fenètre
        InvalidateRect(hwnd, NULL, TRUE);       // Force un RePaint
    }
    // ############## CASE #############
    // ##########=- COMMAND -=##########
    // #################################
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            OPENFILENAME ofn;
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
                if (pImage) { delete pImage; pImage = NULL; }
                pImage = new Image(szFile);

                if (pImage->GetLastStatus() != Ok) {
                    MessageBox(hwnd, L"Impossible de charger l'image!", L"Erreur", MB_OK | MB_ICONERROR);
                    delete pImage; pImage = NULL;
                }
                else {
                    InvalidateRect(hwnd, NULL, TRUE);
                }

                // Charger aussi le fichier brut en mémoire
                size_t rawSize = 0;
                unsigned char* rawData = LoadFileToArray(szFile, rawSize);
                if (rawData) {
                    // Conversion en hexadécimal
                    bool foundFFFE = false;
                    std::wstring hexDump = BufferToHex(rawData, rawSize, foundFFFE);

                    // Écriture dans un fichier texte
                    std::wofstream out(L"dump_hex.txt");
                    out << hexDump;
                    out.close();

                    g_rawSize = rawSize;

                    wchar_t buffer[256];
                    wsprintf(buffer, L"Fichier chargé (%d octets). Hexadécimal écrit dans dump_hex.txt", (int)rawSize);
                    MessageBox(hwnd, buffer, L"Info", MB_OK);

                    if (foundFFFE) {
                        // ✅ Séquence trouvée
                        MessageBox(hwnd, L"La séquence FF FE est présente dans le fichier !", L"Alerte", MB_OK | MB_ICONWARNING);
                    }
                    else {
                        // ❌ Séquence absente → proposer à l’utilisateur
                        int response = MessageBox(hwnd,
                            L"La séquence FF FE n’a pas été trouvée.\nVoulez-vous créer un double de l’image avec ce commentaire secret ?",
                            L"Proposition",
                            MB_YESNO | MB_ICONQUESTION);

                        if (response == IDYES) {
                            // Exemple simple : créer une copie brute du fichier avec suffixe "_secret"
                            std::wstring newFile = std::wstring(szFile) + L"_secret.jpg";
                            std::ifstream src(szFile, std::ios::binary);
                            std::ofstream dst(newFile, std::ios::binary);
                            dst << src.rdbuf();

                            // Ici tu pourrais ajouter réellement la séquence FF FE dans les métadonnées
                            // ou à la fin du fichier. Exemple basique :
                            unsigned char marker[2] = { 0xFF, 0xFE };
                            dst.write(reinterpret_cast<char*>(marker), 2);

                            src.close();
                            dst.close();

                            MessageBox(hwnd, L"Copie créée avec le commentaire secret FF FE.", L"Succès", MB_OK | MB_ICONINFORMATION);
                        }
                    }

                    delete[] rawData;
                }
            }
        }
        break;

        // ############## CASE #############
        // ###########=- PAINT -=###########
        // #################################
    case WM_PAINT:
    {
        int imgW;   // Initalise la taille de l'image | Horizontal
        int imgH;   // Initalise la taille de l'image | Vertical
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);    // Création d'un objet HDC
        Graphics graphics(hdc);             // Création d'un objet Graphics

        // =-> FOND
        HBRUSH hBrush = CreateSolidBrush(RGB(70, 98, 75));   // Couleur du fond
        FillRect(hdc, &ps.rcPaint, hBrush);                         // Applique la couleur
        DeleteObject(hBrush);   // Suprimme la brush

        // =-> IMAGE
        if (pImage) {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int clientWidth = rcClient.right - rcClient.left;   // Recupère la taille X de l'image
            int clientHeight = rcClient.bottom - rcClient.top;  // Recupère la Taille Y de l'image

            imgW = pImage->GetWidth();      // Récupère la taille Vertical de l'image
            imgH = pImage->GetHeight();     // Récupère la taille Horizontal de l'image
            double imgRatio = (double)imgW / imgH;                  // Ont obtient le ratio de l'image
            double winRatio = (double)clientWidth / clientHeight;   // Ont obtient le ratio de la fenètre

            int drawW, drawH;
            if (winRatio > imgRatio) {
                drawH = clientHeight;                       // Ont applique le ratio a l'image selon la hauteur de celle-ci
                drawW = (int)(clientHeight * imgRatio);
            }
            else {
                drawW = clientWidth;                        // Ont applique le ratio a l'image selon la largeur de celle-ci
                drawH = (int)(clientWidth / imgRatio);
            }

            int offsetX = (clientWidth - drawW) / 2;        // Décallage X de l'image
            int offsetY = (clientHeight - drawH) / 2;       // Décallage Y de l'image

            graphics.DrawImage(pImage, offsetX, offsetY, drawW, drawH);     // Affichage de l'image
        }

        // =-> TEXTE
        SetTextColor(hdc, RGB(0, 0, 0));  // Couleur du texte
        //SetBkMode(hdc, TRANSPARENT);            // Fond du texte transparent
        wchar_t buffer[100];                    // Tableau contenant une chaine de charactère formant un texte
        wsprintf(buffer, L"Taille fenètre : %d x %d  Position : %d x %d", xsize, ysize, xpos, ypos);    // Texte mis dans le buffer
        TextOut(hdc, 5, 40, buffer, wcslen(buffer));     // Affichage du texte
        if (pImage) {   // Si une image est chargé :
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            int clientWidth = rcClient.right - rcClient.left;
            int clientHeight = rcClient.bottom - rcClient.top;

            int imgW = pImage->GetWidth();
            int imgH = pImage->GetHeight();
            double imgRatio = (double)imgW / imgH;
            double winRatio = (double)clientWidth / clientHeight;

            int drawW, drawH;
            if (winRatio > imgRatio) {
                drawH = clientHeight;
                drawW = (int)(clientHeight * imgRatio);
            }
            else {
                drawW = clientWidth;
                drawH = (int)(clientWidth / imgRatio);
            }

            int offsetX = (clientWidth - drawW) / 2;
            int offsetY = (clientHeight - drawH) / 2;

            graphics.DrawImage(pImage, offsetX, offsetY, drawW, drawH);
            wsprintf(buffer, L"Taille image : %d octets", g_rawSize);       // Affiche la taille de l'image en octet
            TextOut(hdc, 5, 60, buffer, wcslen(buffer));
            wsprintf(buffer, L"Taille image : %d x %d pixel", imgW, imgH);  // Affiche la taille en pixel
            TextOut(hdc, 5, 80, buffer, wcslen(buffer));
        }

        EndPaint(hwnd, &ps);    // "Termine" l'affichage
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}