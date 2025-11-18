#include "window.h"

LRESULT CALLBACK Window::WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {
    Window* pWindow = nullptr;

    if(msg == WM_CREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*) lParam;
        pWindow = (Window*)pCreate->lpCreateParams;
        SetWindowLongPtr(
            hwnd,
            GWLP_USERDATA,
            (LONG_PTR)pWindow
        );
    } else {
        pWindow = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    if(pWindow) {
        switch(msg) {
            case WM_CREATE:
                if(!pWindow->captureController.init(
                    hwnd,
                    10,
                    10,
                    300,
                    300
                )) {
                    MessageBox(hwnd, L"Failed to init cam!", L"Error", MB_OK);
                }
                break;
            case WM_SIZE:
                {
                    int newWidth = LOWORD(lParam);
                    int newHeight = HIWORD(lParam);
                    pWindow->captureController.resize(
                        20,
                        20,
                        newWidth - 40,
                        newHeight - 40
                    );
                }
                break;
            case WM_PAINT:
                {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hwnd, &ps);
                    
                    RECT rect = { 20, 10, 200, 30 };
                    DrawText(
                        hdc,
                        L"CamValley ULTRA ALPHA BUILD!",
                        -1,
                        &rect,
                        DT_LEFT
                    );
                    EndPaint(hwnd, &ps);
                }
                break;
            case WM_DESTROY:
                pWindow->cleanup();
                PostQuitMessage(0);
                break;
            default:
                return DefWindowProc(
                    hwnd,
                    msg,
                    wParam,
                    lParam
                );
        }
    } else {
        return DefWindowProc(
            hwnd,
            msg,
            wParam,
            lParam
        );
        return 0;
    }
}

/*
** Create Window
*/
void Window::createWindow() {
    wc = {0};
    wc.lpszClassName = L"nameforclass";
    wc.lpfnWndProc = Window::WindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    if(!RegisterClass(&wc)) {
        MessageBox(
            nullptr,
            L"Window Registration failed!",
            L"Error",
            MB_ICONEXCLAMATION | MB_OK
        );
        return;
    }

    hwnd = CreateWindowW(
        wc.lpszClassName,
        L"CamValley",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        nullptr,
        nullptr,
        wc.hInstance,
        this
    );
}

/*
** Show Window
*/
void Window::showWindow() {
    if(!hwnd) return;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while(GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Window::run() {
    createWindow();
    showWindow();
}

void Window::cleanup() {
    captureController.cleanup();
}