#ifndef UNICODE
#define UNICODE
#endif 

#include <windows.h>

int xsize = 800;
int ysize = 600;
int xpos = 0;
int ypos = 0;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{

    // initialise la fenètre avec une classe
    const wchar_t CLASS_NAME[] = L"Sample Window Class";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Crée la fenètre
    HWND hwnd = CreateWindowEx(
        0,                              // Style Optionel de la fenètre
        CLASS_NAME,                     // Window class
        L"Stéganographie",              // Nom de la fenètre (affiché en haut)
        WS_OVERLAPPEDWINDOW,            // Style de la fenètre

        // Les deux primière valeur corrésponde a la position de base de la fenètre (x, y)
        // Les deux dernière : a la taille de la fenètre (x, y)
        xpos, ypos, xsize, ysize,

        NULL,       // fenètre parent  
        NULL,       // Menu
        hInstance,  // Instance handle
        NULL        // Additional application data
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    // Affiche la fenètre
    ShowWindow(hwnd, nCmdShow);

    // Run the message loop.
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        DispatchMessage(&msg);  // envoie le message WindowProc (la fonction ou il y a tous les WM_Paint, Size, Destroy)
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:            // PROC de destruction
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:               // PROC de taille
    {
        xsize = LOWORD(lParam);  // largeur client
        ysize = HIWORD(lParam);  // hauteur client

        InvalidateRect(hwnd, NULL, TRUE); // force un repaint
    }

    case WM_MOVE:
    {
        xpos = (int)(short)LOWORD(lParam);  // récupère la position x
        ypos = (int)(short)HIWORD(lParam);  // récupère la position y

        InvalidateRect(hwnd, NULL, TRUE); // force un repaint
    }

    case WM_PAINT:              // PROC d'affichage
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Fond bleu clair
        HBRUSH hBrush = CreateSolidBrush(RGB(xsize, ysize, 255));
        FillRect(hdc, &ps.rcPaint, hBrush);
        DeleteObject(hBrush); // libérer la ressource

        // Couleur du texte
        SetTextColor(hdc, RGB(255, xpos, ypos));
        SetBkMode(hdc, TRANSPARENT);             // fond transparent derrière le texte

        // Affichage du texte
        wchar_t buffer[100];    // Tableau de charactère
        wsprintf(buffer, L"Taille : %d x %d  Position : %d , %d", xsize, ysize, xpos, ypos);  // Ajoute des truc au tableau buffer

        // Affichage du texte
        TextOut(hdc, 0, 0, buffer, wcslen(buffer)); // Appèle l'affichage de texte


        EndPaint(hwnd, &ps);
    }
    return 0;

    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}