#pragma once
#include <Windows.h>
#include <iostream>

class Window {
    private: 
        WNDCLASS wc;
        HWND hwnd;
        
        int width = 800;
        int height = 600;

        void createWindow();
        void showWindow();

    public:
        void run();
};