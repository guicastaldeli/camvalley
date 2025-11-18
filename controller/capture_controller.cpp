#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#include <iostream>
#include "capture_controller.h"

CaptureController::CaptureController() : 
    pGraph(nullptr),
    pCapture(nullptr),
    pDevice(nullptr),
    pGrabberFilter(nullptr),
    pGrabber(nullptr),
    pControl(nullptr),
    pEvent(nullptr),
    hwndParent(nullptr),
    hwndVideo(nullptr),
    isRunning(false),
    pbmi(nullptr) 
{
    ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
}
CaptureController::~CaptureController() {
    cleanup();
}

bool CaptureController::init(
    HWND parent,
    int x,
    int y,
    int w,
    int h
) {
    hwndParent = parent;
    hwndVideo = CreateWindow(
        L"Static",
        L"",
        WS_CHILD | WS_VISIBLE,
        x,
        y,
        w,
        h,
        hwndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    if(!hwndVideo) {
        return false;
    }

    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if(FAILED(hr)) return false;
    if(!refreshDevices()) return false;

    if(deviceController.getDeviceCount() > 0) {
        auto* fDevice = deviceController.getDevice(0);
        if(fDevice) return initWithDevice(
            parent,
            fDevice->getId(),
            x,
            y,
            w,
            h
        );
    }

    return false;
}

bool CaptureController::initWithDevice(
    HWND parent,
    const std::wstring& deviceId,
    int x,
    int y,
    int w,
    int h
) {
    hwndParent = parent;
    currentDeviceId = deviceId;
    if(!hwndVideo) {
        hwndVideo = CreateWindow(
            L"Static",
            L"",
            WS_CHILD | WS_VISIBLE,
            w,
            y,
            w,
            h,
            hwndParent,
            NULL,
            GetModuleHandle(NULL),
            NULL
        );
        if(!hwndVideo) {
            return false;
        }
    } else {
        resize(x, y, w, h);
    }

    cleanup();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if(!createGraph()) return false;

    IDevice* device = deviceController.findDevice(deviceId);
    if(!device) return false;

    pDevice = device->createFilter();
    if(!pDevice) return false;
    if(!setupGrabber()) return false;
    if(!renderStream()) return false;

    return true;
}

/*
** Create Graph
*/
bool CaptureController::createGraph() {
    HRESULT hr = CoCreateInstance(
        CLSID_FilterGraph,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IGraphBuilder,
        (void**)&pCapture
    );
    if(FAILED(hr)) {
        return false;
    }

    hr = pCapture->SetFiltergraph(pGraph);
    if(FAILED(hr)) return false;

    hr = pGraph->QueryInterface(IID_IMediaControl, (void**)&pControl);
    if(FAILED(hr)) return false;

    hr = pGraph->QueryInterface(IID_IMediaEvent, (void**)&pEvent);
    return SUCCEEDED(hr);
}

bool CaptureController::setupGrabber() {
    HRESULT hr = CoCreateInstance(
        CLSID_SampleGrabber,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IBaseFilter,
        (void**)&pGrabberFilter
    );
    if(FAILED(hr)) {
        return false;
    }

    hr = pGraph->AddFilter(pGrabberFilter, L"Sample Grabber");
    if(FAILED(hr)) return false;

    hr = pGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&pGrabber);
    if(FAILED(hr)) return false;

    ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = MEDIASUBTYPE_RGB24;
    hr = pGrabber->SetMediaType(&mt);
    if(FAILED(hr)) return false;

    hr = pGrabber->SetCallback(NULL, 0);
    hr = pGrabber->SetOneShot(FALSE);
    hr = pGrabber->SetBufferSamples(TRUE);

    return true;
}

/*
** Render Stream
*/
bool CaptureController::renderStream() {
    if(!pDevice) return false;

    HRESULT hr = pGraph->AddFilter(pDevice, L"Video Capture");
    if(FAILED(hr)) return false;

    hr = pCapture->RenderStream(
        &PIN_CATEGORY_PREVIEW,
        &MEDIATYPE_Video,
        pDevice,
        NULL,
        NULL
    );
    if(FAILED(hr)) {
        return false;
    }

    IVideoWindow* pVideoWindow = NULL;
    hr = pGraph->QueryInterface(IID_IVideoWindow, (void**)&pVideoWindow);
    if(SUCCEEDED(hr)) {
        pVideoWindow->put_Owner((OAHWND)hwndVideo);
        pVideoWindow->put_WindowState(WS_CHILD | WS_CLIPSIBLINGS);

        RECT rect;
        if(GetClientRect(hwndVideo, &rect)) {
            pVideoWindow->SetWindowPosition(0, 0, rect.right, rect.bottom);
        }
        pVideoWindow->Release();
    }

    hr = pGrabber->GetConnectedMediaType(&mt);
    if(SUCCEEDED(hr) && mt.formattype == FORMAT_VideoInfo) {
        VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)mt.pbFormat;
        pbmi = &pVih->bmiHeader;
    }

    if(pControl) {
        hr = pControl->Run();
        if(SUCCEEDED(hr)) isRunning = true;
    }

    return true;
}

bool CaptureController::connectDevice(IBaseFilter* device) {
    return device != nullptr;
}

HRESULT __stdcall CaptureController::sampleCB(
    double sampleTime, 
    IMediaSample* pSample
) {
    return S_OK;
}

void CaptureController::resize(int x, int y, int w, int h) {
    if(hwndVideo) {
        MoveWindow(hwndVideo, x, y, w, h, TRUE);

        IVideoWindow* pVideoWindow = NULL;
        if(
            pGraph &&
            SUCCEEDED(pGraph->QueryInterface(
                IID_IVideoWindow,
                (void**)&pVideoWindow
            ))
        ) {
            pVideoWindow->SetWindowPosition(0, 0, w, h);
            pVideoWindow->Release();
        }
    }
}

bool CaptureController::refreshDevices() {
    return deviceController.setDevices();
}

IDevice* CaptureController::getCurrentDevice() const {
    return deviceController.findDevice(currentDeviceId);
}

void CaptureController::cleanup() {
    if(pControl && isRunning) {
        pControl->Stop();
        isRunning = false;
    }

    if(pControl) {
        pControl->Release();
        pControl = nullptr;
    }
    if(pEvent) {
        pEvent->Release();
        pEvent = nullptr;
    }
    if(pGrabber) {
        pGrabber->Release();
        pGrabber = nullptr;
    }
    if(pGrabberFilter) {
        pGrabberFilter->Release();
        pGrabberFilter = nullptr;
    }
    if(pDevice) {
        pDevice->Release();
        pDevice = nullptr;
    }
    if(pCapture) {
        pCapture->Release();
        pCapture = nullptr;
    }
    if(pGraph) {
        pGraph->Release();
        pGraph = nullptr;
    }

    if(mt.pbFormat) {
        CoTaskMemFree(mt.pbFormat);
        mt.pbFormat = NULL;
    }
    pbmi = nullptr;
    if(hwndVideo) {
        DestroyWindow(hwndVideo);
        hwndVideo = nullptr;
    }
}