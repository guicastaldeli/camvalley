#pragma once
#include <d2d1.h>
#include <d2d1helper.h>
#include <wincodec.h>
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include "../controller/capture_controller.h"
#include "../window_manager.h"

class D2DRenderer {
    private:
        ID2D1Factory* pFactory;
        ID2D1HwndRenderTarget* pRenderTarget;
        ID2D1Bitmap* pVideoBitmap;
        HWND hwndVideo;
        UINT32 width;
        UINT32 height;
        void convertYUY2ToRGB32(
            const BYTE* yuy2Data, 
            DWORD yuy2Length,
            BYTE* rgbData, 
            UINT32 width, 
            UINT32 height
        );

    public:
        D2DRenderer();
        ~D2DRenderer();

        bool init(HWND hwnd);
        bool renderFrame(
            IMFSample* pSample,
            UINT32 frameWidth,
            UINT32 frameHeight,
            GUID pixelFormat
        );
        void resize(UINT32 newWidth, UINT32 newHeight);
        void cleanup();
};