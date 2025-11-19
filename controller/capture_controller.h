#pragma once
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <evr.h>
#include <string>
#include <vector>
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "evr.lib")
#include "device_controller.h"

class CaptureController {
    private:
        IMFMediaSource* pVideoSource;
        IMFSourceReader* pReader;
        IMFActivate** ppDevices;
        IMFMediaSession* pSession;
        IMFVideoDisplayControl* pVideoDisplay;
        UINT32 deviceCount;
        HWND hwndParent;
        HWND hwndVideo;

        std::wstring currentDeviceId;
        DeviceController deviceController;
        bool isRunning;

        bool createVideoWindow();
        bool setupDevice(const std::wstring& deviceId);
        bool createSourceReader();
        bool createEVR();
        HRESULT updateVideoWindow();

    public:
        CaptureController();
        ~CaptureController();
        HWND getVideoWindow() const {return hwndVideo;}

        bool init(
            HWND parent,
            int x,
            int y,
            int w,
            int h
        );
        bool initWithDevice(
            HWND parent,
            const std::wstring& deviceId,
            int x,
            int y,
            int w,
            int h
        );
        void resize(int x, int y, int w, int h);
        bool refreshDevices();
        IDevice* getCurrentDevice() const;

        bool isCapturing() const {
            return isRunning;
        }
        std::wstring getCurrentDeviceId() const {
            return currentDeviceId;
        }
        void cleanup();
};