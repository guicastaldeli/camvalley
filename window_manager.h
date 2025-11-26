#pragma once
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <string>
#include <mfreadwrite.h>

class CaptureController;
#define WM_UPDATE_FACES (WM_USER + 100)

class WindowManager {
    public:
        WNDCLASS wc;
        HWND hwnd;
        HWND hwndVideo;
        HWND hwndOverlay;
        CaptureController* captureController;

        WindowManager();
        ~WindowManager();

        int width = 800;
        int height = 600;

        static LRESULT CALLBACK WindowProc(
            HWND hwnd,
            UINT msg,
            WPARAM wParam,
            LPARAM lParam
        );
        void createWindow();
        void showWindow();

        static LRESULT CALLBACK VideoWndProc(
            HWND hwnd,
            UINT msg,
            WPARAM wParam,
            LPARAM lParam
        );
        bool createVideoWindow();
        void updateVideoWindow();
        
        static LRESULT CALLBACK OverlayWndProc(
            HWND hwnd,
            UINT msg,
            WPARAM wParam,
            LPARAM lParam
        );
        bool createOverlayWindow();
        void updateOverlayWindow();
    
        void resize(int x, int y, int w, int h);
        void run();
        void cleanup();
};