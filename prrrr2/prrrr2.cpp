#include <windows.h>
#include <commdlg.h>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include <locale>
#include <codecvt>

using namespace std;

// Глобальные переменные
HBITMAP hBitmap = NULL;
// Определение идентификатора меню
#define IDM_OPEN 1001
// Функция открытия BMP файла через диалоговое окно



bool OpenBMPFile(HWND hWnd, wchar_t** selectedFile)
{
    OPENFILENAME ofn;
    wchar_t szFile[MAX_PATH] = L"";

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"BMP Files\0*.bmp\0";
    ofn.lpstrFile = szFile;
    ofn.lpstrTitle = L"Выберите BMP файл";
    ofn.nMaxFile = sizeof(szFile);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileName(&ofn))
    {
        *selectedFile = _wcsdup(szFile);
        return true;
    }

    return 0;
}


struct ImgInfo {
    std::vector<COLORREF>* pixels;
    int height, width, figXMin, figXMax, figYMin, figYMax;
};

ImgInfo* readPixelColorsFromFile(const wchar_t* filename) {
    ifstream file(filename, ios::binary);
    vector<COLORREF>* pixelColors = new vector<COLORREF>();

    if (file.is_open()) {
        char* header = new char[54];
        file.read(header, 54);

        int width = *(int*)&header[18];
        int height = *(int*)&header[22];

        int dataSize = width * height * 3;

        char* data = new char[dataSize];

        file.read(data, dataSize);

        COLORREF backgroundColor = RGB((unsigned char)data[2], (unsigned char)data[1], (unsigned char)data[0]);

        int figXMin = width, figYMin = height, figXMax = 0, figYMax = 0;

        for (int i = 0; i < dataSize; i += 3) {
            COLORREF color = RGB((unsigned char)data[i + 2], (unsigned char)data[i + 1], (unsigned char)data[i]);
            pixelColors->push_back(color);

            if (color != backgroundColor) {
                int currentX = (i / 3) % width;
                int currentY = (i / 3) / width;

                if (currentX < figXMin) figXMin = currentX;
                if (currentX > figXMax) figXMax = currentX;
                if (currentY < figYMin) figYMin = currentY;
                if (currentY > figYMax) figYMax = currentY;
            }
        }

        delete[] data;
        delete[] header;

        file.close();

        for (COLORREF& color : *pixelColors) {
            if (color == backgroundColor) {
                color = RGB(255, 255, 255);
            }
        }

        ImgInfo* info = new ImgInfo;
        info->pixels = pixelColors;
        info->height = height;
        info->width = width;
        info->figXMin = figXMin;
        info->figXMax = figXMax;
        info->figYMin = figYMin;
        info->figYMax = figYMax;

        return info;
    }
    return nullptr;
}

void DrawImage(const ImgInfo* imageInfo, HDC hdcWindow, HWND hwnd) {
    int figWidth = imageInfo->figXMax - imageInfo->figXMin + 1;
    int figHeight = imageInfo->figYMax - imageInfo->figYMin + 1;
    std::vector<COLORREF>* pixelColors = imageInfo->pixels;

    // Закрашиваем окно белыми пикселями
    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
    RECT windowRect;
    GetClientRect(hwnd, &windowRect);
    FillRect(hdcWindow, &windowRect, whiteBrush);
    DeleteObject(whiteBrush);

    // Вычисляем центр окна
    int windowCenterX = (windowRect.right - windowRect.left) / 2;
    int positionX = windowCenterX - figWidth / 2; // Расчёт координаты для отображения изображения в центре 

    for (int x = 0; x < figWidth; ++x) {
        for (int y = 0; y < figHeight; ++y) {
            int flippedX = x; // зеркалим
            int flippedY = figHeight - 1 - y;  // зеркалим
            SetPixel(hdcWindow, positionX + x, y, pixelColors->at((imageInfo->figYMin + flippedY) * imageInfo->width + imageInfo->figXMin + flippedX));
        }
    }
}





// Функция обработки сообщений
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static wchar_t* selectedFile = nullptr; // Объявление переменной selectedFile
    static ImgInfo* info = nullptr; // Объявление переменной info
    switch (msg)
    {

    case WM_COMMAND:
        // Обработка сообщений меню
        switch (LOWORD(wParam))
        {
        case IDM_OPEN:
            // Открытие BMP файла
            if (OpenBMPFile(hwnd, &selectedFile))
            {
                info = readPixelColorsFromFile(selectedFile);
                if (info)
                {
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
            break;
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);int height = LOWORD(lParam);
        int width = HIWORD(lParam);
        PaintRgn(hdc, CreateRectRgn(0, 0, width, height));
        if (info != nullptr) {
            DrawImage(info, hdc, hwnd);
        }

        EndPaint(hwnd, &ps);

        return 0;
    }
    break;

    case WM_SIZE:
        // Обновление окна при изменении размера
        InvalidateRect(hwnd, NULL, TRUE);
        break;

    case WM_CLOSE:
        // Закрытие окна
        DeleteObject(hBitmap);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    return 0;
}

// Функция создания и отображения окна
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Регистрация класса окна
    const wchar_t* selectedFile = nullptr;
    const wchar_t CLASS_NAME[] = L"MyWindowClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Создание окна
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Пример приложения Windows с отображением BMP",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    // Создание меню
    HMENU hMenu = CreateMenu();
    HMENU hSubMenu = CreatePopupMenu();
    AppendMenu(hSubMenu, MF_STRING, IDM_OPEN, L"Открыть BMP файл");
    AppendMenu(hMenu, MF_STRING | MF_POPUP, (UINT)hSubMenu, L"Файл");

    // Установка меню для окна
    SetMenu(hwnd, hMenu);

    // Отображение окна
    ShowWindow(hwnd, nCmdShow);

    // Цикл обработки сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}