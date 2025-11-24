#include "window_manager.h"
#include "controller/capture_controller.h"
#include <iostream>

WindowManager::WindowManager(): 
    hwnd(nullptr),
    hwndVideo(nullptr),
    hwndOverlay(nullptr) 
{
    captureController = new CaptureController(*this);
}

WindowManager::~WindowManager() {
    if(captureController) {
        delete captureController;
        captureController = nullptr;
    }
}

/*
**
*** Video Window
**
*/
LRESULT CALLBACK WindowManager::VideoWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {
    WindowManager* pWindow = nullptr;

    if(msg == WM_CREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pWindow = (WindowManager*)pCreate->lpCreateParams;
        if(pWindow) {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pWindow);
        }
    } else {
        pWindow = (WindowManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    switch(msg) {
        case WM_SIZE:
            if(pWindow) {
                if(pWindow->hwnd && IsWindow(hwnd)) {
                    pWindow->updateVideoWindow();
                }
                break;
            }
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                RECT rect;
                GetClientRect(hwnd, &rect);
                HBRUSH blackBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(hdc, &rect, blackBrush);
                DeleteObject(blackBrush);
                
                EndPaint(hwnd, &ps);
            }
        case WM_ERASEBKGND:
            return 1;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool WindowManager::createVideoWindow() {
    if(!hwnd || !IsWindow(hwnd)) {
        std::wcout << L"Invalid parent window handle: " << hwnd << std::endl;
        return false;
    }

    static bool classRegistered = false;
    static const wchar_t* VIDEO_WINDOW_CLASS = L"VideoWindow";
    if(!classRegistered) {
        WNDCLASS wc = {};
        wc.lpfnWndProc = VideoWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = VIDEO_WINDOW_CLASS;
        wc.hbrBackground = NULL;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        if(RegisterClass(&wc)) {
            classRegistered = true;
            std::wcout << L"Video window registered!" << std::endl;
        } else {
            std::wcout << L"Failed to register video window :(" << std::endl;
            VIDEO_WINDOW_CLASS = L"Static";
        }
    }

    hwndVideo = CreateWindowExW(
        WS_EX_NOPARENTNOTIFY,
        VIDEO_WINDOW_CLASS,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        10, 10, 100, 100,
        hwnd,
        NULL,
        GetModuleHandle(NULL),
        this
    );
    if(!hwndVideo) {
        DWORD err = GetLastError();
        std::wcout << L"CreateWindowEx failed erroor: " << err << std::endl;
        return false;
    }
    SetWindowLong(
        hwndVideo, 
        GWL_STYLE,
        GetWindowLong(
            hwndVideo,
            GWL_STYLE
        ) & ~WS_BORDER
    );

    std::wcout << L"Video window created!: " << hwndVideo << std::endl;
    return true;
}

void WindowManager::updateVideoWindow() {
    if(hwndVideo && IsWindow(hwndVideo)) {
        InvalidateRect(hwndVideo, NULL, TRUE);
        UpdateWindow(hwndVideo);
    }
}

/*
**
*** Overlay Window
**
*/
LRESULT CALLBACK WindowManager::OverlayWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {
    WindowManager* pWindow = nullptr;
    if(msg == WM_CREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*) lParam;
        pWindow = (WindowManager*)pCreate->lpCreateParams;
        if(pWindow) {
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pWindow);
        }
    } else {
        pWindow = (WindowManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    switch(msg) {
        case WM_PAINT:
            if(pWindow && pWindow->captureController) {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                auto faces = pWindow->captureController->getCurrentFaces();
                pWindow->captureController->getRenderer().draw(hdc, faces);
                EndPaint(hwnd, &ps);
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool WindowManager::createOverlayWindow() {
    if(!hwndVideo || !IsWindow(hwndVideo)) {
        std::wcout << L"Invalid video window" << std::endl;
        return false;
    }

    static bool classRegistered = false;
    static const wchar_t* OVERLAY_CLASS = L"OverlayWindow";
    if(!classRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = OverlayWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = OVERLAY_CLASS;
        wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        
        if(!RegisterClassExW(&wc)) {
            DWORD err = GetLastError();
            if(err != ERROR_CLASS_ALREADY_EXISTS) {
                std::wcout << L"Failed to register overlay class: " << err << std::endl;
                return false;
            }
        }
        classRegistered = true;
        std::wcout << L"Overlay window class registered" << std::endl;
    }

    RECT videoRect;
    if(!GetClientRect(hwndVideo, &videoRect)) {
        std::wcout << L"Failed to get video client rect" << std::endl;
        return false;
    }
    
    int width = videoRect.right - videoRect.left;
    int height = videoRect.bottom - videoRect.top;
    if(width <= 0 || height <= 0) {
        std::wcout << L"Video window has invalid size: " << width << L"x" << height << std::endl;
        width = 640;
        height = 480;
    }

    if(hwndOverlay && IsWindow(hwndOverlay)) {
        DestroyWindow(hwndOverlay);
        hwndOverlay = nullptr;
    }

    RECT videoScreenRect;
    GetWindowRect(hwndVideo, &videoScreenRect);
    SetLastError(0);
    
    hwndOverlay = CreateWindowExW(
        WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        OVERLAY_CLASS,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        0, 0,
        width, height,
        hwnd,
        NULL,
        GetModuleHandle(NULL),
        this
    );    
    if(!hwndOverlay) {
        DWORD err = GetLastError();
        std::wcout << L"CreateWindowEx failed!! Error: " << err << std::endl;
        return false;
    }

    SetWindowPos(
        hwndOverlay,
        HWND_TOPMOST,
        0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
    );
    
    std::wcout << L"Overlay window created successfully: " << hwndOverlay << std::endl;
    
    ShowWindow(hwndOverlay, SW_SHOW);
    UpdateWindow(hwndOverlay);
    updateOverlayWindow();
    
    return true;
}

void WindowManager::updateOverlayWindow() {
    if(hwndOverlay && IsWindow(hwndOverlay)) {
        InvalidateRect(hwndOverlay, NULL, TRUE);
        UpdateWindow(hwndOverlay);
    }
}

/*
**
*** Main Window
**
*/
LRESULT CALLBACK WindowManager::WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam
) {
    WindowManager* pWindow = nullptr;

    if(msg == WM_CREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*) lParam;
        pWindow = (WindowManager*)pCreate->lpCreateParams;
        SetWindowLongPtr(
            hwnd,
            GWLP_USERDATA,
            (LONG_PTR)pWindow
        );
        pWindow->hwnd = hwnd;
        
        std::wcout << L"Main window created in WM_CREATE: " << hwnd << std::endl;
        
        // Envia mensagem para inicialização tardia
        PostMessage(hwnd, WM_USER + 1, 0, 0);
        return 0;
    } else {
        pWindow = (WindowManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    if(pWindow) {
        auto faces = pWindow->captureController->getCurrentFaces();
        switch(msg) {
            case WM_USER + 1:
                std::wcout << L"Initializing capture controller with window: " << pWindow->hwnd << std::endl;
                if(!pWindow->captureController->init(10, 10, 640, 460)) {
                    MessageBox(hwnd, L"Failed to init capturecontroller", L"Camera Error", MB_OK | MB_ICONERROR);
                } else {
                    std::wcout << L"Capture controller success!" << std::endl;
                    pWindow->captureController->enableFaceDetection(true);
                }
                return 0;
            case WM_SIZE:
                {
                    int newWidth = LOWORD(lParam);
                    int newHeight = HIWORD(lParam);
                    int videoWidth = newWidth - 150;
                    int videoHeight = newHeight - 100;
                    pWindow->resize(
                        50,
                        50,
                        videoWidth,
                        videoHeight
                    );
                }
                return 0;
            case WM_ERASEBKGND:
                return 1;
                
            case WM_UPDATE_FACES:
                if(pWindow->hwndVideo) {
                    RECT videoRect;
                    if(GetWindowRect(pWindow->hwndVideo, &videoRect)) {
                        POINT topLeft = { videoRect.left, videoRect.top };
                        POINT bottomRight = { videoRect.right, videoRect.bottom };
                        ScreenToClient(hwnd, &topLeft);
                        ScreenToClient(hwnd, &bottomRight);

                        RECT faceArea = {
                            topLeft.x - 50, 
                            topLeft.y - 50, 
                            bottomRight.x + 50, 
                            bottomRight.y + 50
                        };
                        InvalidateRect(hwnd, &faceArea, FALSE);
                    }
                }
                pWindow->updateOverlayWindow();
                break;
            case WM_PAINT:
                {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hwnd, &ps);

                    RECT clientRect;
                    GetClientRect(hwnd, &clientRect);
                    HBRUSH bgBrush = CreateSolidBrush(RGB(220, 220, 220));
                    FillRect(hdc, &clientRect, bgBrush);
                    DeleteObject(bgBrush);

                    RECT rect = { 550, 10, 800, 30 };
                    DrawText(hdc, L"Camvalley ULTRA ALPHA BUILD!", -1, &rect, DT_LEFT);
                    
                    RECT faceRect = { 550, 40, 800, 70 };
                    auto faces = pWindow->captureController->getCurrentFaces();
                    std::wstring faceText = L"Faces detected: " + std::to_wstring(faces.size());
                    DrawText(hdc, faceText.c_str(), -1, &faceRect, DT_LEFT);

                    EndPaint(hwnd, &ps);
                }
                return 0;
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
    }
    return 0;
}

void WindowManager::createWindow() {
    wc = {0};
    static const wchar_t* WINDOW_CLASS = L"MainWindow";
    wc.lpszClassName = WINDOW_CLASS;
    wc.lpfnWndProc = WindowManager::WindowProc;
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
        WINDOW_CLASS,
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
    if(hwnd) {
        std::wcout << L"Main window created successfully in createWindow(): " << hwnd << std::endl;
    } else {
        std::wcout << L"Failed to create main window!" << std::endl;
    }
}

void WindowManager::showWindow() {
    if(!hwnd) return;

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while(GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void WindowManager::resize(int x, int y, int w, int h) {
    if(hwndVideo) {
        MoveWindow(hwndVideo, x, y, w, h, TRUE);
        if(hwndOverlay && IsWindow(hwndOverlay)) {
            MoveWindow(hwndOverlay, 0, 0, w, h, TRUE);
        }
        if(captureController->pVideoDisplay) {
            RECT rc { 0, 0, w, h };
            captureController->pVideoDisplay->SetVideoPosition(NULL, &rc);
        }
    }
}

void WindowManager::run() {
    createWindow();
    showWindow();
}

void WindowManager::cleanup() {
    captureController->cleanup();
}