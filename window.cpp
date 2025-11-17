#define UNICODE
#define _UNICODE
#include "window.h"
#include <windows.h>

/*
** Create Window
*/
void Window::createWindow() {
    wc = {0};
    wc.lpszClassName = L"nameforclass";
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandle(NULL);
    RegisterClass(&wc);

    hwnd = CreateWindow(
        L"nameforclass",
        L"Hello World!",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        NULL,
        NULL,
        wc.hInstance,
        NULL
    );
}

/*
** Show Window
*/
void Window::showWindow() {
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    MSG msg = {0};
    while(GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

/*
** Run
*/
void Window::run() {
    createWindow();
    showWindow();
}
