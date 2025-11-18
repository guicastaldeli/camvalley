#pragma once
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <string>
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")
#include "controller/capture_controller.h"

class Window {
    private:
        WNDCLASS wc;
        HWND hwnd;
        CaptureController captureController;

        int width = 800;
        int height = 600;

        void createWindow();
        void showWindow();
        static LRESULT CALLBACK WindowProc(
            HWND hwnd,
            UINT msg,
            WPARAM wParam,
            LPARAM lParam
        );
    
    public:
        void run();
        void cleanup();
};