#pragma once
#include <windows.h>
#include <dshow.h>
#include <qedit.h>
#include "device_controller.h"

class CaptureController {
    private:
        IGraphBuilder* pGraph;
        ICaptureGraphBuilder2* pCapture;
        IBaseFilter* pDevice;
        IBaseFilter* pGrabberFilter;
        ISampleGrabber* pGrabber;
        IMediaControl* pControl;
        IMediaEvent* pEvent;
        HWND hwndParent;
        HWND hwndVideo;
        bool isRunning;

        AM_MEDIA_TYPE mt;
        BITMAPINFOHEADER* pbmi;

        DeviceController deviceController;
        bool isRunning;
        std::wstring currentDeviceId;

        bool createGraph();
        bool setupGrabber();
        bool renderStream();
        bool connectDevice(IBaseFilter* device);
        static HRESULT __stdcall sampleCB(
            double sampleTime, 
            IMediaSample* pSample
        );

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
        void resize(
            int x,
            int y,
            int w,
            int h
        );
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