#include "d2d_renderer.h"
#include <iostream>
#include <d2d1.h>
#include <d2d1helper.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "windowscodecs.lib")

D2DRenderer::D2DRenderer() :
    pFactory(nullptr),
    pRenderTarget(nullptr),
    pVideoBitmap(nullptr),
    hwndVideo(nullptr),
    width(0),
    height(0) 
{}

D2DRenderer::~D2DRenderer() {
    cleanup();
}

bool D2DRenderer::init(HWND hwnd) {
    HRESULT hr = S_OK;
    hwndVideo = hwnd;

    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create factory" << hr << std::endl;
        return false;
    }

    RECT clientRect;
    if(!GetClientRect(hwndVideo, &clientRect)) {
        std::wcout << L"Failed to get client rect" << std::endl;
        return false;
    }
    width = clientRect.right - clientRect.left;
    height = clientRect.bottom - clientRect.top;
    if(width == 0 || height == 0) {
        width = 640;
        height = 480;
    }

    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties();
    D2D1_HWND_RENDER_TARGET_PROPERTIES hwndRtProps =
        D2D1::HwndRenderTargetProperties(hwndVideo, D2D1::SizeU(width, height));

    hr = pFactory->CreateHwndRenderTarget(rtProps, hwndRtProps, &pRenderTarget);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create render target: " << hr << std::endl;
        return false;
    }

    std::wcout << L"Direct2D renderer initialized!" << std::endl;
    return true;
}

bool D2DRenderer::renderFrame(IMFSample* pSample) {
    if(!pSample || !pRenderTarget) {
        std::wcout << L"Invalid sample or render target" << std::endl;
        return false;
    }

    HRESULT hr = S_OK;
    IMFMediaBuffer* pBuffer = nullptr;
    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if(FAILED(hr)) {
        std::wcout << L"Failed to convert buffer: " << hr << std::endl;
        return false;
    }

    BYTE* pData = nullptr;
    DWORD maxLength;
    DWORD currentLength;
    hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
    if(FAILED(hr) || !pData || currentLength == 0) {
        std::wcout << L"Failed to lock buffer or no data: " << hr << std::endl;
        if(pBuffer) pBuffer->Release();
        return false;
    }

    try {
        UINT32 actualWidth = width;
        UINT32 actualHeight = height;
        UINT32 expectedSize = width * height * 4;
        if (currentLength < expectedSize) {
            std::wcout << L"Buffer size mismatch. Expected: " << expectedSize 
                      << L", Got: " << currentLength << std::endl;
            if (currentLength >= 4) {
                actualHeight = currentLength / (width * 4);
                if (actualHeight == 0) actualHeight = 1;
            }
        }

        if (actualWidth == 0) actualWidth = 640;
        if (actualHeight == 0) actualHeight = 480;

        std::wcout << L"Rendering frame: " << actualWidth << L"x" << actualHeight 
                  << L", Data size: " << currentLength << std::endl;

        if(!pVideoBitmap || actualWidth != width || actualHeight != height) {
            if(pVideoBitmap) {
                pVideoBitmap->Release();
                pVideoBitmap = nullptr;
            }
            
            D2D1_BITMAP_PROPERTIES bitmapProps =
                D2D1::BitmapProperties(
                    D2D1::PixelFormat(
                        DXGI_FORMAT_B8G8R8A8_UNORM, 
                        D2D1_ALPHA_MODE_IGNORE
                    )
                );
            
            hr = pRenderTarget->CreateBitmap(
                D2D1::SizeU(actualWidth, actualHeight),
                pData,
                actualWidth * 4,
                bitmapProps,
                &pVideoBitmap
            );
            if(FAILED(hr)) {
                std::wcout << L"Failed to create bitmap: " << hr << std::endl;
                pBuffer->Unlock();
                pBuffer->Release();
                return false;
            }
    
            width = actualWidth;
            height = actualHeight;
        }
        if(pVideoBitmap) {
            D2D1_RECT_U rect = D2D1::RectU(0, 0, width, height);
            hr = pVideoBitmap->CopyFromMemory(&rect, pData, width * 4);
            if(FAILED(hr)) {
                std::wcout << L"Failed to copy bitmap data: " << hr << std::endl;
                pBuffer->Unlock();
                pBuffer->Release();
                return false;
            }
            pRenderTarget->BeginDraw();
            pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
            
            D2D1_RECT_F destRect = D2D1::RectF(
                0, 0,
                (FLOAT)width,
                (FLOAT)height
            );
            
            pRenderTarget->DrawBitmap(pVideoBitmap, destRect);
            hr = pRenderTarget->EndDraw();
            
            if(FAILED(hr)) {
                std::wcout << L"Failed to draw bitmap: " << hr << std::endl;
            }
        }
        
        pBuffer->Unlock();
        pBuffer->Release();
        return SUCCEEDED(hr);
        
    } catch (const std::exception& e) {
        std::wcout << L"Exception in renderFrame: " << e.what() << std::endl;
        if(pData) pBuffer->Unlock();
        if(pBuffer) pBuffer->Release();
        return false;
    } catch (...) {
        std::wcout << L"Unknown exception in renderFrame" << std::endl;
        if(pData) pBuffer->Unlock();
        if(pBuffer) pBuffer->Release();
        return false;
    }
}

void D2DRenderer::resize(UINT32 newWidth, UINT32 newHeight) {
    if(pRenderTarget && newWidth > 0 && newHeight > 0) {
        width = newWidth;
        height = newHeight;
        pRenderTarget->Resize(D2D1::SizeU(width, height));
        if(pVideoBitmap) {
            pVideoBitmap->Release();
            pVideoBitmap = nullptr;
        } 
    }
}

void D2DRenderer::cleanup() {
    if(pVideoBitmap) {
        pVideoBitmap->Release();
        pVideoBitmap = nullptr;
    }
    if(pRenderTarget) {
        pRenderTarget->Release();
        pRenderTarget = nullptr;
    }
    if(pFactory) {
        pFactory->Release();
        pFactory = nullptr;
    }
}
