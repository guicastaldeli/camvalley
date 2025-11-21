#pragma once
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "evr.lib")
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <shlwapi.h>
#include <evr.h>
#include <string>
#include <vector>
#include "device_controller.h"
#include "classifier/renderer.h"

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

        Renderer renderer;
        bool faceDetectionEnabled;

        bool createVideoWindow();
        bool setupDevice(const std::wstring& deviceId);
        bool createSourceReader();
        bool createEVR();
        HRESULT updateVideoWindow();
        HRESULT configFormat(IMFMediaTypeHandler* pHandler);

    public:
        CaptureController();
        ~CaptureController();

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

        void enableFaceDetection(bool enable);
        void loadCascade(const std::string& file);
        std::vector<std::vector<unsigned char>> convertFrameToGrayscale(BYTE* pData, DWORD length);

        void processFrame();
        bool isCapturing() const {
            return isRunning;
        }
        HWND getVideoWindow() const { 
            return hwndVideo; 
        }
        std::wstring getCurrentDeviceId() const {
            return currentDeviceId;
        }
        Renderer& getRenderer() {
            return renderer;
        }
        void cleanup();
};