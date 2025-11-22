#include "window.h"
#include <iostream>

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
                    640,
                    460
                )) {
                    MessageBox(hwnd, L"Failed to init cam", L"Camera Error", MB_OK | MB_ICONERROR);
                } else {
                    std::wcout << L"Capture controller success!" << std::endl;
                    pWindow->captureController.enableFaceDetection(true);
                    SetTimer(hwnd, 1, 200, NULL);
                }
                return 0;
            case WM_TIMER:
                if(wParam == 1) {
                    pWindow->captureController.processFrame();
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                return 0;
            case WM_SIZE:
                {
                    int newWidth = LOWORD(lParam);
                    int newHeight = HIWORD(lParam);
                    int videoWidth = newWidth - 150;
                    int videoHeight = newHeight - 100;
                    pWindow->captureController.resize(
                        50,
                        50,
                        videoWidth,
                        videoHeight
                    );
                }
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
                {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hwnd, &ps);

                    RECT clientRect;
                    GetClientRect(hwnd, &clientRect);
                    HBRUSH bgBrush = CreateSolidBrush(RGB(220, 220, 220));
                    FillRect(hdc, &clientRect, bgBrush);
                    DeleteObject(bgBrush);

                    HWND hwndVideo = pWindow->captureController.getVideoWindow();
                    if(hwndVideo && IsWindow(hwndVideo)) {
                        RECT videoRect;
                        if(GetWindowRect(hwndVideo, &videoRect)) {
                            POINT topLeft = { videoRect.left, videoRect.top };
                            POINT bottomRight = { videoRect.right, videoRect.bottom };
                            ScreenToClient(hwnd, &topLeft);
                            ScreenToClient(hwnd, &bottomRight);

                            RECT borderRect = {
                                topLeft.x - 2,
                                topLeft.y - 2,
                                bottomRight.x + 2,
                                bottomRight.y + 2
                            };
                            HBRUSH borderBrush = CreateSolidBrush(RGB(0, 200, 0));
                            FrameRect(hdc, &borderRect, borderBrush);
                            DeleteObject(borderBrush);
                        }
                    }

                    pWindow->captureController.getRenderer().draw(hdc);

                    RECT rect = { 550, 10, 800, 30 };
                    DrawText(
                        hdc,
                        L"Camvalley ULTRA ALPHA BUILD!",
                        -1,
                        &rect,
                        DT_LEFT
                    );

                    EndPaint(hwnd, &ps);
                }
                return 0;
            case WM_DESTROY:
                KillTimer(hwnd, 1);
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
    }
    return 0;
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