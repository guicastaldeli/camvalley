#include <iostream>
#include "capture_controller.h"

CaptureController::CaptureController() :
    pVideoSource(nullptr),
    pReader(nullptr),
    ppDevices(nullptr),
    deviceCount(0),
    hwndParent(nullptr),
    hwndVideo(nullptr),
    isRunning(false)
{
    HRESULT hr = MFStartup(MF_VERSION);
    if(FAILED(hr)) std::wcout << L"MFStartup failed: " << hr << std::endl;
}
CaptureController::~CaptureController() {
    cleanup();
    MFShutdown();
}

bool CaptureController::init(
    HWND parent,
    int x,
    int y,
    int w,
    int h
) {
    hwndParent = parent;
    if(!createVideoWindow()) return false;
    resize(x, y, w, h);

    if(!refreshDevices()) return false;
    if(deviceController.getDeviceCount() > 0) {
        auto* fDevice = deviceController.getDevice(1);
        if(fDevice) {
            return initWithDevice(
                parent,
                fDevice->getId(),
                x,
                y,
                w,
                h
            );
        }
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
        if(!createVideoWindow()) {
            return false;
        }
    }

    resize(x, y, w, h);
    cleanup();
    return setupDevice(deviceId);
}

/*
** Create Video Window
*/
bool CaptureController::createVideoWindow() {
    hwndVideo = CreateWindow(
        L"Static",
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        0, 0, 100, 100,
        hwndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    return hwndVideo != nullptr;
}

/*
** Setup Device
*/
bool CaptureController::setupDevice(const std::wstring& deviceId) {
    std::wcout << L"Setting up device: " << deviceId << std::endl;
    IDevice* device = deviceController.findDevice(deviceId);
    if(!device) {
        std::wcout << L"Device not found in controller" << std::endl;
        return false;
    }
    std::wcout << L"Found device: " << device->getName() << std::endl;

    IMFAttributes* pAttrs = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttrs, 1);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes: " << hr << std::endl;
        return false;
    }

    hr = pAttrs->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set source type GUID: " << hr << std::endl;
        pAttrs->Release();
        return false;
    }

    hr = pAttrs->SetString(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
        deviceId.c_str()
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set symbolic link: " << hr << std::endl;
        pAttrs->Release();
        return false;
    }

    std::wcout << L"Creating device source..." << std::endl;
    hr = MFCreateDeviceSource(pAttrs, &pVideoSource);
    pAttrs->Release();
    if(FAILED(hr)) {
        std::wcout << L"MFCreateDeviceSource failed: " << hr << std::endl;
        return false;
    }

    std::wcout << L"MFCreateDeviceSource failed: " << hr << std::endl;
    return createSourceReader();
}

bool CaptureController::createSourceReader() {
    std::wcout << L"Creating source reader..." << std::endl;
    IMFAttributes* pAttrs = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttrs, 1);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create reader attributes: " << hr << std::endl;
        return false;
    }

    hr = pAttrs->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, nullptr);
    hr = pAttrs->SetUINT32(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, TRUE);
    hr = MFCreateSourceReaderFromMediaSource(pVideoSource, pAttrs, &pReader);
    pAttrs->Release();
    if(FAILED(hr)) {
        std::wcout << L"MFCreateSourceReaderFromMediaSource failed: " << hr << std::endl;
        return false;
    }

    std::wcout << L"MFCreateSourceReaderFromMediaSource failed: " << hr << std::endl;
    hr = pReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    hr = pReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);

    IMFMediaType* pMediaType = nullptr;
    hr = MFCreateMediaType(&pMediaType);
    if(SUCCEEDED(hr)) {
        hr = pMediaType->SetGUID(
            MF_MT_MAJOR_TYPE,
            MFMediaType_Video
        );
        hr = pMediaType->SetGUID(
            MF_MT_SUBTYPE,
            MFVideoFormat_RGB32
        );
        hr = pReader->SetCurrentMediaType(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            nullptr,
            pMediaType
        );
        pMediaType->Release();
        if(FAILED(hr)) {
            std::wcout << L"Failed to set media type: " << hr << std::endl;
        } else {
            std::wcout << L"Failed to set media type: " << hr << std::endl;
        }
    }

    isRunning = true;
    std::wcout << L"Source reader setup completed successfully" << std::endl;
    return true;
} 

HRESULT CaptureController::updateVideoWindow() {
    if(!hwndVideo || !pReader) return E_FAIL;

    IMFSample* pSample = nullptr;
    DWORD streamIndex;
    DWORD flags;
    LONGLONG timestamp;

    HRESULT hr = pReader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        &streamIndex,
        &flags,
        &timestamp,
        &pSample
    );
    if(SUCCEEDED(hr)) {
        if(flags & MF_SOURCE_READERF_ENDOFSTREAM) {
            isRunning = false;
        }

        if(pSample) {
            IMFMediaBuffer* pBuffer = nullptr;
            hr = pSample->ConvertToContiguousBuffer(&pBuffer);

            if(SUCCEEDED(hr)) {
                BYTE* pData = nullptr;
                DWORD length = 0;

                hr = pBuffer->Lock(&pData, nullptr, &length);
                if(SUCCEEDED(hr) && pData) {
                    pBuffer->Unlock();
                }
                pBuffer->Release();
            }
            pSample->Release();
        }
    }

    return hr;
}

void CaptureController::resize(int x, int y, int w, int h) {
    if(hwndVideo) {
        MoveWindow(hwndVideo, x, y, w, h, TRUE);
    }
}

IDevice* CaptureController::getCurrentDevice() const {
    return deviceController.findDevice(currentDeviceId);
}

bool CaptureController::refreshDevices() {
    return deviceController.setDevices();
}

void CaptureController::cleanup() {
    isRunning = false;

    if(pReader) {
        pReader->Release();
        pReader = nullptr;
    }
    if(pVideoSource) {
        pVideoSource->Release();
        pVideoSource = nullptr;
    }
    if(ppDevices) {
        for(UINT32 i = 0; i < deviceCount; i++) {
            if(ppDevices[i]) {
                ppDevices[i]->Release();
            }
        }
        CoTaskMemFree(ppDevices);
        ppDevices = nullptr;
        deviceCount = 0;
    }
    if(hwndVideo) {
        DestroyWindow(hwndVideo);
        hwndVideo = nullptr;
    }
}