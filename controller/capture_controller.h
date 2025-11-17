#pragma once
#include <windows.h>
#include <dshow.h>
#include <qedit.h>

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

        bool createGraph();
        bool setupGrabber();
        bool renderStream();
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
        void start();
        void stop();
        void cleanup();
        void resize(
            int x,
            int y,
            int w,
            int h
        );
};