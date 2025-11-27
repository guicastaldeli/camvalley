#pragma once
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <evr.h>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>

class CaptureController;
class SourceReader {
    public:
        IMFSourceReader* pReader;

        bool createSourceReader(
            CaptureController* instance, 
            IMFMediaSource* pCaptureSource
        );
        bool createSourceReaderWithRes(
            CaptureController* instance,
            IMFMediaSource* pCaptureSource, 
            UINT32 width, 
            UINT32 height
        );
};