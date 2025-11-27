#pragma once
#include <Windows.h>
#include <vector>
#include <iostream>
#include <mfidl.h>
#include <mfreadwrite.h>

class FrameConverter {
    public:
        std::vector<std::vector<unsigned char>> convertToGrayscale(
            BYTE* pData, 
            DWORD length,
            IMFSourceReader* pReader
        );
};