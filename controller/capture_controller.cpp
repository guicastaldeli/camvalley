#pragma comment(lib, "evr.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "dxva2.lib")
#include "capture_controller.h"
#include <iostream>
#include <evr.h>
#include <d3d9.h>
#include <cmath>

CaptureController::CaptureController() :
    pVideoSource(nullptr),
    pReader(nullptr),
    ppDevices(nullptr),
    deviceCount(0),
    hwndParent(nullptr),
    hwndVideo(nullptr),
    pSession(nullptr),
    pVideoDisplay(nullptr),
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
    std::wcout << L"Parent window: " << hwndParent << std::endl;
    
    if(!hwndVideo) {
        if(!createVideoWindow()) {
            std::wcout << L"Failed to create video window!" << std::endl;
            return false;
        }
        std::wcout << L"Video window created: " << hwndVideo << std::endl;
    }
    resize(x, y, w, h);

    if(!refreshDevices()) {
        std::wcout << L"No devices found!" << std::endl;
        return false;
    }
    
    std::wcout << L"Found " << deviceController.getDeviceCount() << L" devices" << std::endl;
    if(deviceController.getDeviceCount() > 0) {
        auto* fDevice = deviceController.getDevice(1);
        if(fDevice) {
            std::wcout << L"Using device: " << fDevice->getName() << std::endl;
            return initWithDevice(parent, fDevice->getId(), x, y, w, h);
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
    
    cleanup();
    if(!hwndVideo) {
        if(!createVideoWindow()) {
            return false;
        }
    }

    resize(x, y, w, h);
    return setupDevice(deviceId);
}

/*
** Create Video Window
*/
bool CaptureController::createVideoWindow() {
    if(!hwndParent || !IsWindow(hwndParent)) {
        std::wcout << L"Invalid parent window handle: " << hwndParent << std::endl;
        return false;
    }

    static bool classRegistered = false;
    static const wchar_t* VIDEO_WINDOW_CLASS = L"VideoWindow";
    if(!classRegistered) {
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = VIDEO_WINDOW_CLASS;
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        if(RegisterClass(&wc)) {
            classRegistered = true;
            std::wcout << L"Video window registered!" << std::endl;
        } else {
            std::wcout << L"Failed to register video window :(" << std::endl;
            VIDEO_WINDOW_CLASS = L"Static";
        }
    }

    hwndVideo = CreateWindowExW(
        WS_EX_NOPARENTNOTIFY,
        VIDEO_WINDOW_CLASS,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        10, 10, 100, 100,
        hwndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    if(!hwndVideo) {
        DWORD err = GetLastError();
        std::wcout << L"CreateWindowEx failed erroor: " << err << std::endl;
        return false;
    }

    std::wcout << L"Video window created!: " << hwndVideo << std::endl;
    return true;
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

    std::wcout << L"Device Source created! " << hr << std::endl;
    return createEVR();
}

/*
** Create EVR
*/
bool CaptureController::createEVR() {
    HRESULT hr = S_OK;
    IMFActivate* pSinkActivate = NULL;
    IMFTopology* pTopology = NULL;
    IMFTopologyNode* pSourceNode = NULL;
    IMFTopologyNode* pOutputNode = NULL;
    IMFStreamDescriptor* pSourceSD = NULL;
    IMFPresentationDescriptor* pPD = NULL;
    IMFMediaTypeHandler* pHandler = NULL;
    IMFMediaEvent* pEvent = NULL;
    DWORD streamCount = 0;
    BOOL fSelected = FALSE;
    DWORD videoStreamIndex = (DWORD)-1;
    MediaEventType eventType;
    std::wcout << L"Setting EVR pipeline" << std::endl;

    std::wcout << L"Setting EVR pipeline with window: " << hwndVideo << std::endl;
    hr = MFCreateVideoRendererActivate(hwndVideo, &pSinkActivate);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create video renderer activate: " << hr << std::endl;
        goto done;
    }

    hr = MFCreateTopology(&pTopology);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create topology: " << hr << std::endl;
        goto done;
    }

    hr = pVideoSource->CreatePresentationDescriptor(&pPD);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create presentatin descriptor: " << hr << std::endl;
        goto done;
    }

    hr = pPD->GetStreamDescriptorCount(&streamCount);
    if(FAILED(hr) || streamCount == 0) {
        std::wcout << L"No streams available: " << hr << std::endl;
        goto done;
    }
    for(DWORD i = 0; i < streamCount; i++) {
        hr = pPD->GetStreamDescriptorByIndex(i, &fSelected, &pSourceSD);
        if(SUCCEEDED(hr) && pSourceSD) {
            hr = pSourceSD->GetMediaTypeHandler(&pHandler);
            if(SUCCEEDED(hr)) {
                GUID majorType = GUID_NULL;
                hr = pHandler->GetMajorType(&majorType);
                if(SUCCEEDED(hr) && majorType == MFMediaType_Video) {
                    videoStreamIndex = i;
                    std::wcout << L"Found video stream at index: " << i << std::endl;
                    hr = configFormat(pHandler);
                    break;
                }
            }
            if(pHandler) {
                pHandler->Release();
                pHandler = NULL;
            }
            if(pSourceSD) {
                pSourceSD->Release();
                pSourceSD = NULL;
            }
        }
    }
    if(videoStreamIndex == (DWORD)-1) {
        std::wcout << L"No video stream found!" << std::endl;
        hr = E_FAIL;
        goto done;
    }
    if(!pSourceSD) {
        hr = pPD->GetStreamDescriptorByIndex(videoStreamIndex, &fSelected, &pSourceSD);
        if(FAILED(hr)) {
            std::wcout << L"Failed to get video stream descriptor: " << hr << std::endl;
            goto done;
        }
    }

    hr = MFCreateTopologyNode(
        MF_TOPOLOGY_SOURCESTREAM_NODE, 
        &pSourceNode
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to create source node: " << hr << std::endl;
        goto done;
    }
    hr = pSourceNode->SetUnknown(MF_TOPONODE_SOURCE, pVideoSource);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set source: " << hr << std::endl;
        goto done;
    }
    hr = pSourceNode->SetUnknown(MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set presentation descriptor: " << hr << std::endl;
        goto done;
    }
    hr = pSourceNode->SetUnknown(MF_TOPONODE_STREAM_DESCRIPTOR, pSourceSD);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set stream descriptor: " << hr << std::endl;
        goto done;
    }

    hr = MFCreateTopologyNode(
        MF_TOPOLOGY_OUTPUT_NODE,
        &pOutputNode
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to create output node: " << hr << std::endl;
        goto done;
    }
    hr = pOutputNode->SetObject(pSinkActivate);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set sink activate: " << hr << std::endl;
        goto done;
    }
    hr = pOutputNode->SetUINT32(MF_TOPONODE_STREAMID, 0);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set stream ID: " << hr << std::endl;
        goto done;
    }
    hr = pOutputNode->SetUINT32(MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE);
    if(FAILED(hr)) {
        std::wcout << L"Failed to set no shutdown: " << hr << std::endl;
        goto done;
    }

    hr = pTopology->AddNode(pSourceNode);
    if(FAILED(hr)) {
        std::wcout << L"Failed to add source node: " << hr << std::endl;
        goto done;
    }
    hr = pTopology->AddNode(pOutputNode);
    if(FAILED(hr)) {
        std::wcout << L"Failed to add output node: " << hr << std::endl;
        goto done;
    }
    hr = pSourceNode->ConnectOutput(0, pOutputNode, 0);
    if(FAILED(hr)) {
        std::wcout << L"Failed to connect nodes: " << hr << std::endl;
        goto done;
    }

    hr = MFCreateMediaSession(NULL, &pSession);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create media session: " << hr << std::endl;
        goto done;
    }
    hr = pSession->SetTopology(
        MFSESSION_SETTOPOLOGY_IMMEDIATE, 
        pTopology
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set topology: " << hr << std::endl;
        goto done;
    }

    hr = pSession->GetEvent(0, &pEvent);
    if(SUCCEEDED(hr)) {
        hr = pEvent->GetType(&eventType);
        if(SUCCEEDED(hr)) {
            std::wcout << L"First media event type: " << eventType << std::endl;
        }
        pEvent->Release();
    }

    PROPVARIANT varStart;
    PropVariantInit(&varStart);
    varStart.vt = VT_EMPTY;
    hr = pSession->Start(&GUID_NULL, &varStart);
    if(FAILED(hr)) {
        std::cout << L"Failed to start sesison: " << hr << std::endl;
        hr = pSession->Start(NULL, NULL);
        if(FAILED(hr)) {
            std::wcout << L"Alternative start failed: " << hr << std::endl;
            goto done;
        }
    }

    isRunning = true;
    std::wcout << L"EVR setup complete!" << std::endl;
    done:
        if(pSourceNode) pSourceNode->Release();
        if(pOutputNode) pOutputNode->Release();
        if(pTopology) pTopology->Release();
        if(pSinkActivate) pSinkActivate->Release();
        if(pSourceSD) pSourceSD->Release();
        if(pPD) pPD->Release();
        if(pHandler) pHandler->Release();
        if(pEvent) pEvent->Release();

        return SUCCEEDED(hr);
}

bool CaptureController::createSourceReader() {
    return true;
} 

HRESULT CaptureController::updateVideoWindow() {
    return S_OK;
}

void CaptureController::resize(int x, int y, int w, int h) {
    if(hwndVideo) {
        MoveWindow(hwndVideo, x, y, w, h, TRUE);
        if(pVideoDisplay) {
            RECT rc { 0, 0, w, h };
            pVideoDisplay->SetVideoPosition(NULL, &rc);
        }
    }
}

IDevice* CaptureController::getCurrentDevice() const {
    return deviceController.findDevice(currentDeviceId);
}

bool CaptureController::refreshDevices() {
    return deviceController.setDevices();
}

/*
** Config Format
*/
HRESULT CaptureController::configFormat(IMFMediaTypeHandler* pHandler) {
    HRESULT hr = S_OK;
    IMFMediaType* pMediaType = NULL;
    DWORD typeCount = 0;
    
    hr = pHandler->GetMediaTypeCount(&typeCount);
    if (FAILED(hr)) return hr;

    for(DWORD i = 0; i < typeCount; i++) {
        hr = pHandler->GetMediaTypeByIndex(i, &pMediaType);
        if(SUCCEEDED(hr)) {
            UINT32 width = 0;
            UINT32 height = 0;
            GUID subtype = GUID_NULL;

            hr = MFGetAttributeSize(
                pMediaType,
                MF_MT_FRAME_SIZE,
                &width,
                &height
            );
            if(SUCCEEDED(hr)) {
                float targetRatio = 16.0f / 9.0f;
                float aspectRatio = 
                    static_cast<float>(width) /
                    static_cast<float>(height);

                if(std::fabs(aspectRatio - targetRatio) < 0.1f) {
                    hr = pHandler->SetCurrentMediaType(pMediaType);
                    if(SUCCEEDED(hr)) {
                        std::wcout << L"Widescreen format success!" << std::endl;
                        pMediaType->Release();
                        return S_OK;
                    }
                }
                pMediaType->Release();
                pMediaType = NULL;
                    
            }
        }
    }

    UINT32 commonWs[][2] = {
        { 1920, 1080 },
        { 1280, 720 },
        { 1366, 768 },
        { 1600, 900 },
        { 1024, 576 },
        { 854, 480 }
    };
    for(int i = 0; i < sizeof(commonWs) / sizeof(commonWs[0]); i++) {
        hr = pHandler->GetMediaTypeByIndex(0, &pMediaType);
        if(SUCCEEDED(hr)) {
            hr = MFSetAttributeSize(
                pMediaType,
                MF_MT_FRAME_SIZE,
                commonWs[i][0],
                commonWs[i][1]
            );
            if(SUCCEEDED(hr)) {
                hr = pHandler->SetCurrentMediaType(pMediaType);
                if(SUCCEEDED(hr)) {
                    pMediaType->Release();
                    return S_OK;
                }
            }
            pMediaType->Release();
            pMediaType = NULL;
        }
    }

    return E_FAIL;
}

void CaptureController::cleanup() {
    isRunning = false;

    if(pSession) {
        pSession->Stop();
        pSession->Close();
        pSession->Release();
        pSession = nullptr;
    }
    if(pVideoDisplay) {
        pVideoDisplay->Release();
        pVideoDisplay = nullptr;
    }
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