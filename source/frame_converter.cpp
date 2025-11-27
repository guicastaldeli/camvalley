#include "frame_converter.h"
#include <vector>
#include <iostream>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

std::vector<std::vector<unsigned char>> FrameConverter::convertToGrayscale(
    BYTE* pData,
    DWORD length,
    IMFSourceReader* pReader
) { 
    if(!pData || length == 0) {
        std::wcout << L"Invalid frame data in convertFrameToGrayscale" << std::endl;
        return std::vector<std::vector<unsigned char>>();
    }

    IMFMediaType* pType = nullptr;
    UINT32 width = 0;
    UINT32 height = 0;
    GUID subtype = GUID_NULL;

    HRESULT hr = pReader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        &pType
    );
    if(SUCCEEDED(hr) && pType) {
        hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &width, &height);
        pType->GetGUID(MF_MT_SUBTYPE, &subtype);
        pType->Release();
    }
    
    if(width == 0 || height == 0) {
        width = 640;
        height = 480;
        std::wcout << L"Using default resolution for grayscale: " 
            << width << L"x" << height << std::endl;
    }
    
    std::vector<std::vector<unsigned char>> grayscaleFrame(
        height,
        std::vector<unsigned char>(width, 0)
    );
    
    if(subtype == MFVideoFormat_RGB32) {
        for (UINT32 y = 0; y < height; y++) {
            for (UINT32 x = 0; x < width; x++) {
                DWORD pixelOffset = (y * width + x) * 4;
                if(pixelOffset + 3 < length) {
                    BYTE b = pData[pixelOffset];
                    BYTE g = pData[pixelOffset + 1];
                    BYTE r = pData[pixelOffset + 2];

                    unsigned char gray = static_cast<unsigned char>(
                        0.299f * r + 0.587f * g + 0.114f * b
                    );
                    grayscaleFrame[y][x] = gray;
                }
            }
        }
    } else if(subtype == MFVideoFormat_YUY2) {
        for (UINT32 y = 0; y < height; y++) {
            for (UINT32 x = 0; x < width; x += 2) {
                DWORD pixelOffset = (y * width + x) * 2;
                if(pixelOffset + 3 < length) {
                    BYTE y0 = pData[pixelOffset];
                    BYTE y1 = pData[pixelOffset + 2];
                    
                    grayscaleFrame[y][x] = y0;
                    if(x + 1 < width) {
                        grayscaleFrame[y][x + 1] = y1;
                    }
                }
            }
        }
    } else {
        std::wcout << L"Unsupported pixel format in convertFrameToGrayscale" << std::endl;
        return std::vector<std::vector<unsigned char>>();
    }
    
    return grayscaleFrame;
}