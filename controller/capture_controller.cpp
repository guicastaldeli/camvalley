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
    isRunning(false),
    faceDetectionEnabled(true),
    renderer(),
    frameReady(false)
{
    InitializeCriticalSection(&frameCriticalSection);
    HRESULT hr = MFStartup(MF_VERSION);
    if(FAILED(hr)) std::wcout << L"MFStartup failed: " << hr << std::endl;
}
CaptureController::~CaptureController() {
    cleanup();
    DeleteCriticalSection(&frameCriticalSection);
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
        wc.hbrBackground = NULL;
        wc.style = CS_HREDRAW | CS_VREDRAW;
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
    SetWindowLong(
        hwndVideo, 
        GWL_STYLE,
        GetWindowLong(
            hwndVideo,
            GWL_STYLE
        ) & ~WS_BORDER
    );

    std::wcout << L"Video window created!: " << hwndVideo << std::endl;
    return true;
}

/*
** Setup Device
*/
/*
** Setup Device with Separate Sources for EVR and Frame Capture
*/
bool CaptureController::setupDevice(const std::wstring& deviceId) {
    std::wcout << L"Setting up device: " << deviceId << std::endl;
    IDevice* device = deviceController.findDevice(deviceId);
    if(!device) {
        std::wcout << L"Device not found in controller" << std::endl;
        return false;
    }
    std::wcout << L"Found device: " << device->getName() << std::endl;

    // Create FIRST source for EVR display
    IMFAttributes* pAttrsEVR = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttrsEVR, 1);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes for EVR source" << std::endl;
        return false;
    }

    hr = pAttrsEVR->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set source type GUID for EVR" << std::endl;
        pAttrsEVR->Release();
        return false;
    }

    hr = pAttrsEVR->SetString(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
        deviceId.c_str()
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set symbolic link for EVR" << std::endl;
        pAttrsEVR->Release();
        return false;
    }

    std::wcout << L"Creating EVR device source..." << std::endl;
    hr = MFCreateDeviceSource(pAttrsEVR, &pVideoSource);
    pAttrsEVR->Release();
    if(FAILED(hr)) {
        std::wcout << L"MFCreateDeviceSource failed for EVR: " << hr << std::endl;
        return false;
    }

    std::wcout << L"EVR Device Source created! " << hr << std::endl;
    
    // Create EVR with first source
    if(!createEVR()) {
        std::wcout << L"Failed to create EVR pipeline" << std::endl;
        return false;
    }

    // Create SECOND source for frame capture
    IMFMediaSource* pCaptureSource = nullptr;
    IMFAttributes* pAttrsCapture = nullptr;
    hr = MFCreateAttributes(&pAttrsCapture, 1);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes for capture source" << std::endl;
        return false;
    }

    hr = pAttrsCapture->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set source type GUID for capture" << std::endl;
        pAttrsCapture->Release();
        return false;
    }

    hr = pAttrsCapture->SetString(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
        deviceId.c_str()
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set symbolic link for capture" << std::endl;
        pAttrsCapture->Release();
        return false;
    }

    std::wcout << L"Creating capture device source..." << std::endl;
    hr = MFCreateDeviceSource(pAttrsCapture, &pCaptureSource);
    pAttrsCapture->Release();
    if(FAILED(hr)) {
        std::wcout << L"MFCreateDeviceSource failed for capture: " << hr << std::endl;
        return false;
    }

    std::wcout << L"Capture Device Source created! " << hr << std::endl;

    // Create source reader with second source
    if(!createSourceReader(pCaptureSource)) {
        std::wcout << L"Failed to create source reader" << std::endl;
        pCaptureSource->Release();
        return false;
    }

    // We don't need to keep the capture source reference since source reader owns it
    pCaptureSource->Release();

    std::wcout << L"Both EVR and capture source setup complete!" << std::endl;
    return true;
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

bool CaptureController::createSourceReader(IMFMediaSource* pCaptureSource) {
    if(!pCaptureSource) {
        std::wcout << L"No capture source available" << std::endl;
        return false;
    }

    IMFAttributes* pAttrs = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttrs, 1);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes for source reader" << std::endl;
        return false;
    }

    hr = pAttrs->SetUINT32(
        MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,
        TRUE
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to enable video processing" << std::endl;
        pAttrs->Release();
        return false;
    }

    hr = MFCreateSourceReaderFromMediaSource(
        pCaptureSource,  // Use the provided capture source
        pAttrs,
        &pReader
    );
    pAttrs->Release();
    if(FAILED(hr)) {
        std::wcout << L"Failed to create source reader: " << hr << std::endl;
        return false;
    }

    IMFMediaType* pMediaType = nullptr;
    hr = MFCreateMediaType(&pMediaType);
    bool success = false;
    
    if(SUCCEEDED(hr)) {
        hr = pMediaType->SetGUID(
            MF_MT_MAJOR_TYPE,
            MFMediaType_Video
        );
        hr = pMediaType->SetGUID(
            MF_MT_SUBTYPE,
            MFVideoFormat_RGB32
        );
        if(SUCCEEDED(hr)) {
            hr = pReader->SetCurrentMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                nullptr,
                pMediaType
            );
            success = SUCCEEDED(hr);
        }
        pMediaType->Release();
    }

    if(success) {
        std::wcout << L"Source reader created successfully!" << std::endl;
    } else {
        std::wcout << L"Failed to set media type for source reader: " << hr << std::endl;
    }
    return success;
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

/*
** Enable Face Detection
*/
void CaptureController::enableFaceDetection(bool enable) {
    faceDetectionEnabled = enable;
    if(enable && !renderer.isCascadeLoaded()) {
        std::wcout << L"Loading cascade..." << std::endl;
        loadCascade("../.data/haarcascade_frontalface_default.xml");
        renderer.forceEnable();
    }
}

/*
** Load Cascade
*/
void CaptureController::loadCascade(const std::string& file) {
    if(renderer.load(file)) {
        std::wcout << L"File loaded!: " << file.c_str() << std::endl;
    } else {
        std::wcout << L"Failed to load file :( " << file.c_str() << std::endl;  
    }
}

/*
** Process Frame
*/
void CaptureController::processFrame() {
    if(!pReader) {
        std::wcout << L"No source reader available" << std::endl;
        return;
    }
    
    IMFMediaBuffer* pBuffer = nullptr;
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
    if (FAILED(hr)) {
        std::wcout << L"Failed to read sample!: " << hr << std::endl;
        return;
    }
    if (!pSample) {
        std::wcout << L"No sample available!" << std::endl;
        return;
    }
    
    if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
        std::wcout << L"end of stream reached" << std::endl;
        pSample->Release();
        return;
    }
    hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if (FAILED(hr)) {
        std::wcout << L"failed to convert sample to buffer: " << hr << std::endl;
        pSample->Release();
        return;
    }

    BYTE* pData = nullptr;
    DWORD maxLength;
    DWORD currentLength;

    hr = pBuffer->Lock(
        &pData, 
        &maxLength, 
        &currentLength
    );
    if (FAILED(hr)) {
        std::wcout << L"failed to lock buffer: " << hr << std::endl;
        pBuffer->Release();
        pSample->Release();
        return;
    }

    std::wcout << L"Buffer data length: " << currentLength << std::endl;
    if (pData && currentLength > 0) {
        auto frame = convertFrameToGrayscale(pData, currentLength);

        EnterCriticalSection(&frameCriticalSection);
        currentFrame = frame;
        frameReady = true;
        LeaveCriticalSection(&frameCriticalSection);

        renderer.processFrameForFaces(frame);
    } else {
        std::wcout << L"no frame data available!!" << std::endl;
    }

    pBuffer->Unlock();
    pBuffer->Release();
    pSample->Release();
}

void CaptureController::getCurrentFrame(std::vector<std::vector<unsigned char>>& frame) {
    EnterCriticalSection(&frameCriticalSection);
    if(frameReady) frame = currentFrame;
    LeaveCriticalSection(&frameCriticalSection);
}

std::vector<std::vector<unsigned char>> CaptureController::convertFrameToGrayscale(
    BYTE* pData,
    DWORD length
) {
    std::wcout << L"TESTSTTSTSTSTTSTS" << std::endl;
    
    IMFMediaType* pType = nullptr;
    UINT32 width = 0;
    UINT32 height = 0;

    HRESULT hr = pReader->GetCurrentMediaType(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        &pType
    );
    if (SUCCEEDED(hr) && pType) {
        hr = MFGetAttributeSize(
            pType,
            MF_MT_FRAME_SIZE,
            &width,
            &height
        );
        pType->Release();
    } else {
        std::wcout << L"Failed to get current media type" << std::endl;
    }
    
    if (width == 0 || height == 0) {
        width = 640;
        height = 480;
        std::wcout << L"Using default" << width << L"x" << height << std::endl;
    }
    
    std::vector<std::vector<unsigned char>> grayscaleFrame(
        height,
        std::vector<unsigned char>(width, 0)
    );
    
    int convertedPixels = 0;
    for (UINT32 y = 0; y < height; y++) {
        for (UINT32 x = 0; x < width; x++) {
            DWORD pixelOffset = (y * width + x) * 4;
            if (pixelOffset + 3 < length) {
                BYTE b = pData[pixelOffset];
                BYTE g = pData[pixelOffset + 1];
                BYTE r = pData[pixelOffset + 2];

                unsigned char gray = static_cast<unsigned char>(
                    0.299f * r +
                    0.587f * g +
                    0.114f * b
                );
                grayscaleFrame[y][x] = gray;
                convertedPixels++;
            }
        }
    }
    return grayscaleFrame;
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