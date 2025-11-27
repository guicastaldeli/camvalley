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

bool D2DRenderer::renderFrame(
    IMFSample* pSample,
    UINT32 frameWidth,
    UINT32 frameHeight,
    GUID pixelFormat
) {
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
        std::wcout << L"Failed to lock buffer or no data!: " << hr << std::endl;
        if(pBuffer) pBuffer->Release();
        return false;
    }

    try {
        UINT32 actualWidth = frameWidth;
        UINT32 actualHeight = frameHeight;
        if(actualWidth == 0 || actualHeight == 0) {
            std::wcout << L"Invalid frame dimensions: " 
                << actualWidth << L"x" << actualHeight << std::endl;
            pBuffer->Unlock();
            pBuffer->Release();
            return false;
        }

        UINT32 expectedSize = 0;
        UINT32 bytesPerPixel = 4;
        UINT32 stride = actualWidth * bytesPerPixel;
        
        if(pixelFormat == MFVideoFormat_YUY2) {
            bytesPerPixel = 2;
            stride = actualWidth * bytesPerPixel;
            expectedSize = stride * actualHeight;
        } else if(pixelFormat == MFVideoFormat_RGB32) {
            bytesPerPixel = 4;
            stride = actualWidth * bytesPerPixel;
            expectedSize = stride * actualHeight;
        } else {
            std::wcout << L"Unsupported pixel format: " << &pixelFormat << std::endl;
            pBuffer->Unlock();
            pBuffer->Release();
            return false;
        }

        if(currentLength < expectedSize) {
            std::wcout << L"Buffer size mismatch. Expected: " << expectedSize 
                      << L" got: " << currentLength << std::endl;
            stride = currentLength / actualHeight;
        }

        std::wcout << L"Rendering frame: " << actualWidth << L"x" << actualHeight 
                  << L" Format: " << (pixelFormat == MFVideoFormat_YUY2 ? L"YUY2" : L"RGB32")
                  << L" data size: " << currentLength << std::endl;

        std::vector<BYTE> rgbData;
        BYTE* renderData = pData;
        if(pixelFormat == MFVideoFormat_YUY2) {
            rgbData.resize(actualWidth * actualHeight * 4);
            convertYUY2ToRGB32(pData, currentLength, rgbData.data(), actualWidth, actualHeight);
            renderData = rgbData.data();
            stride = actualWidth * 4;
        }

        if(
            !pVideoBitmap || 
            actualWidth != width || 
            actualHeight != height
        ) {
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
                renderData,
                stride,
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
            std::wcout << L"Bitmap created: " << width << L"x" << height << std::endl;
        }

        if(pVideoBitmap) {
            D2D1_RECT_U rect = D2D1::RectU(0, 0, width, height);
            hr = pVideoBitmap->CopyFromMemory(&rect, renderData, stride);
            if(FAILED(hr)) {
                std::wcout << L"Failed to copy bitmap data: " << hr << std::endl;
                pBuffer->Unlock();
                pBuffer->Release();
                return false;
            }

            pRenderTarget->BeginDraw();
            pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::Black));
            D2D1_SIZE_F rtSize = pRenderTarget->GetSize();
            FLOAT targetWidth = rtSize.width;
            FLOAT targetHeight = rtSize.height;
            
            FLOAT aspectRatio = (FLOAT)width / (FLOAT)height;
            FLOAT targetAspectRatio = targetWidth / targetHeight;
            
            FLOAT drawWidth = targetWidth;
            FLOAT drawHeight = targetHeight;
            
            if(aspectRatio > targetAspectRatio) {
                drawHeight = targetWidth / aspectRatio;
            } else {
                drawWidth = targetHeight * aspectRatio;
            }
            
            FLOAT xOffset = (targetWidth - drawWidth) / 2.0f;
            FLOAT yOffset = (targetHeight - drawHeight) / 2.0f;
            D2D1_RECT_F destRect = D2D1::RectF(
                xOffset, yOffset,
                xOffset + drawWidth,
                yOffset + drawHeight
            );
            
            pRenderTarget->DrawBitmap(
                pVideoBitmap, 
                destRect,
                1.0f,
                D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
            );
            
            hr = pRenderTarget->EndDraw();
            if(FAILED(hr)) {
                std::wcout << L"Failed to end draw: " << hr << std::endl;
            }
        }
        
        pBuffer->Unlock();
        pBuffer->Release();
        return SUCCEEDED(hr);
        
    } catch (const std::exception& err) {
        std::wcout << L"Exception in renderFrame: " << err.what() << std::endl;
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

void D2DRenderer::convertYUY2ToRGB32(
    const BYTE* yuy2Data, 
    DWORD yuy2Length,
    BYTE* rgbData, 
    UINT32 width, 
    UINT32 height
) {
    for (UINT32 y = 0; y < height; y++) {
        for (UINT32 x = 0; x < width; x += 2) {
            DWORD yuy2Offset = (y * width + x) * 2;
            if(yuy2Offset + 3 >= yuy2Length) break;
            
            BYTE y0 = yuy2Data[yuy2Offset];
            BYTE u  = yuy2Data[yuy2Offset + 1];
            BYTE y1 = yuy2Data[yuy2Offset + 2];
            BYTE v  = yuy2Data[yuy2Offset + 3];
            
            int r0 = y0 + 1.402f * (v - 128);
            int g0 = y0 - 0.344f * (u - 128) - 0.714f * (v - 128);
            int b0 = y0 + 1.772f * (u - 128);
            
            int r1 = y1 + 1.402f * (v - 128);
            int g1 = y1 - 0.344f * (u - 128) - 0.714f * (v - 128);
            int b1 = y1 + 1.772f * (u - 128);
            
            r0 = (std::max)(0, (std::min)(255, r0));
            g0 = (std::max)(0, (std::min)(255, g0));
            b0 = (std::max)(0, (std::min)(255, b0));
            r1 = (std::max)(0, (std::min)(255, r1));
            g1 = (std::max)(0, (std::min)(255, g1));
            b1 = (std::max)(0, (std::min)(255, b1));
            
            DWORD rgbOffset0 = (y * width + x) * 4;
            rgbData[rgbOffset0] = b0;
            rgbData[rgbOffset0 + 1] = g0;
            rgbData[rgbOffset0 + 2] = r0;
            rgbData[rgbOffset0 + 3] = 255;

            if(x + 1 < width) {
                DWORD rgbOffset1 = (y * width + x + 1) * 4;
                rgbData[rgbOffset1] = b1;
                rgbData[rgbOffset1 + 1] = g1;
                rgbData[rgbOffset1 + 2] = r1;
                rgbData[rgbOffset1 + 3] = 255;
            }
        }
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
