#pragma comment(lib, "evr.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "dxva2.lib")
#include "capture_controller.h"
#include <iostream>
#include <evr.h>
#include <d3d9.h>
#include <cmath>

CaptureController::CaptureController(WindowManager& wm) :
    windowManager(wm),
    m_cRef(1),
    pVideoSource(nullptr),
    pReader(nullptr),
    ppDevices(nullptr),
    deviceCount(0),
    pSession(nullptr),
    pVideoDisplay(nullptr),
    isRunning(false),
    faceDetectionEnabled(true),
    renderer(),
    frameReady(false),
    pPresenter(nullptr)
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

bool CaptureController::init() {
    std::wcout << L"Parent window: " << windowManager.hwnd << std::endl;
    if(!windowManager.hwnd || !IsWindow(windowManager.hwnd)) {
        std::wcout << L"Main window not available" << std::endl;
        return false;
    }

    try {
        if(!windowManager.hwndVideo) {
            if(!windowManager.createVideoWindow()) {
                std::wcout << L"Failed to create video window!" << std::endl;
                return false;
            }
            std::wcout << L"Video window created: " << windowManager.hwndVideo << std::endl;
        }
    
        if(!refreshDevices()) {
            std::wcout << L"No devices found!" << std::endl;
            return false;
        }
        std::wcout << L"Found " << deviceController.getDeviceCount() << L" devices" << std::endl;
        if(deviceController.getDeviceCount() > 0) {
            auto* fDevice = deviceController.getDevice(1);
            if(fDevice) {
                std::wcout << L"Using device: " << fDevice->getName() << std::endl;
                return initWithDevice(windowManager.hwnd, fDevice->getId());
            }
        }
    } catch(...) {
        std::wcout << L"ERR capture controller!!" << std::endl;
        return false;
    }

    std::wcout << L"Capture controller init" << std::endl;
    return true;
}

bool CaptureController::initWithDevice(HWND parent, const std::wstring& deviceId) {
    currentDeviceId = deviceId; 
    cleanup();
    if(!windowManager.hwndVideo) {
        if(!windowManager.createVideoWindow()) {
            return false;
        }
    }
    return setupDevice(deviceId);
}

/*
** Setup Device
*/
bool CaptureController::setupDevice(const std::wstring& deviceId) {
    IDevice* device = deviceController.findDevice(deviceId);
    if(!device) {
        std::wcout << L"Device not found in controller" << std::endl;
        return false;
    }
    std::wcout << L"Found device: " << device->getName() << std::endl;

    IMFAttributes* pAttrsEVR = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttrsEVR, 1);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes" << std::endl;
        return false;
    }

    hr = pAttrsEVR->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set source type GUID" << std::endl;
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

    hr = MFCreateDeviceSource(pAttrsEVR, &pVideoSource);
    pAttrsEVR->Release();
    if(FAILED(hr)) {
        std::wcout << L"MFCreateDeviceSource failed for EVR: " << hr << std::endl;
        return false;
    }
    
    if(!pPresenter->setEVR()) {
        std::wcout << L"Failed to create EVR pipeline" << std::endl;
        return false;
    }

    IMFMediaSource* pCaptureSource = nullptr;
    IMFAttributes* pAttrsCapture = nullptr;
    hr = MFCreateAttributes(&pAttrsCapture, 1);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes" << std::endl;
        return false;
    }

    hr = pAttrsCapture->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set source type" << std::endl;
        pAttrsCapture->Release();
        return false;
    }

    hr = pAttrsCapture->SetString(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
        deviceId.c_str()
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set symbolic link" << std::endl;
        pAttrsCapture->Release();
        return false;
    }

    hr = MFCreateDeviceSource(pAttrsCapture, &pCaptureSource);
    pAttrsCapture->Release();
    if(FAILED(hr)) {
        std::wcout << L"MFCreateDeviceSource failed: " << hr << std::endl;
        return false;
    }

    if(!createSourceReader(pCaptureSource)) {
        std::wcout << L"Failed to create source reader" << std::endl;
        pCaptureSource->Release();
        return false;
    }
    pCaptureSource->Release();

    if(faceDetectionEnabled) startDetectionThread();
    return true;
}

bool CaptureController::createSourceReader(IMFMediaSource* pCaptureSource) {
    if(!pCaptureSource) {
        std::wcout << L"No capture source available" << std::endl;
        return false;
    }

    IMFAttributes* pAttrs = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttrs, 2);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes" << std::endl;
        return false;
    }

    hr = pAttrs->SetUINT32(
        MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,
        TRUE
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to enable video" << std::endl;
        pAttrs->Release();
        return false;
    }

    hr = pAttrs->SetUnknown(
        MF_SOURCE_READER_ASYNC_CALLBACK,
        this
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set callback" << std::endl;
        pAttrs->Release();
        return false;
    }

    hr = MFCreateSourceReaderFromMediaSource(
        pCaptureSource,
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
        hr = pReader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        );
    } else {
        std::wcout << L"Failed to set media type for source reader: " << hr << std::endl;
    }
    return success;
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
** Face Detection
*/
void CaptureController::enableFaceDetection(bool enable) {
    faceDetectionEnabled = enable;
    if(enable) {
        if(!renderer.isCascadeLoaded()) {
            std::wcout << L"Loading cascade..." << std::endl;
            loadCascade("../.data/haarcascade_frontalface_default.xml");
            renderer.forceEnable();
        }
        startDetectionThread();
    } else {
        std::wcout << "Enable face detection FATAL ERR." << std::endl;
    }
}

void CaptureController::setFacesForOverlay(const std::vector<Rect>& faces) {
    if(pPresenter) {
        pPresenter->setFaces(faces);
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

/*
** Query Interface
*/
STDMETHODIMP CaptureController::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(
            CaptureController,
            IMFSourceReaderCallback
        ),
        {0}
    };
    return QISearch(this, qit, riid, ppv);
}
STDMETHODIMP_(ULONG) CaptureController::AddRef() {
    return InterlockedIncrement(&m_cRef);
}
STDMETHODIMP_(ULONG) CaptureController::Release() {
    ULONG count = InterlockedDecrement(&m_cRef);
    if(count == 0) delete this;
    return count;
}

/*
** On Read Sample
*/
STDMETHODIMP CaptureController::OnReadSample(
    HRESULT hrStatus,
    DWORD dwStreamIndex,
    DWORD dwStreamFlags,
    LONGLONG llTimestamp,
    IMFSample* pSample
) {
    if(FAILED(hrStatus)) {
        std::wcout << L"On Read Sample failed!: " << hrStatus << std::endl;
        return hrStatus;
    }
    if(dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
        std::wcout << L"ENd of stream reached" << std::endl;
        return S_OK;
    }
    if(pSample == nullptr) {
        if(pReader) {
            pReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
        }
        return S_OK;
    }

    IMFMediaBuffer* pBuffer = nullptr;
    HRESULT hr = pSample->ConvertToContiguousBuffer(&pBuffer);
    if(SUCCEEDED(hr)) {
        BYTE* pData = nullptr;
        DWORD maxLength;
        DWORD currentLength;

        hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
        if(SUCCEEDED(hr) && pData && currentLength > 0) {
            auto frame = convertFrameToGrayscale(pData, currentLength);
            EnterCriticalSection(&frameCriticalSection);
            currentFrame = frame;
            frameReady = true;
            LeaveCriticalSection(&frameCriticalSection);

            if(faceDetectionEnabled && detectionRunning) {
                pushFrameToQueue(frame);
            }

            pBuffer->Unlock();
        }   
        pBuffer->Release();
    }

    if(pReader) {
        pReader->ReadSample(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            0,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        );
    }
    return S_OK;
}

STDMETHODIMP CaptureController::OnEvent(DWORD dwStreamIndex, IMFMediaEvent* pEvent) {
    return S_OK;
}

STDMETHODIMP CaptureController::OnFlush(DWORD dwStreamIndex) {
    return S_OK;
}

/*
** Detection Thread
*/
void CaptureController::startDetectionThread() {
    if(detectionRunning) return;
    detectionRunning = true;
    detectionThread = std::thread(&CaptureController::detectionWorker, this);
    std::wcout << L"Face detection thread started" << std::endl;
}

void CaptureController::stopDetectionThread() {
    detectionRunning = false;
    queueCondition.notify_all();
    if(detectionThread.joinable()) detectionThread.join();
    std::wcout << L"Face detection thread stopped" << std::endl;
}

/*
** Detection Worker
*/
void CaptureController::detectionWorker() {
    std::vector<Rect> prevFaces;

    while(detectionRunning) {
        std::vector<std::vector<unsigned char>> frame;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondition.wait_for(
                lock,
                std::chrono::milliseconds(100),
                [this]() {
                    return 
                        !frameQueue.empty() ||
                        !detectionRunning; 
                }
            );
            if(!detectionRunning) return;
            if(!frameQueue.empty()) {
                frame = frameQueue.front();
                frameQueue.pop();
            }
        }
        if(!frame.empty()) {
            renderer.processFrameForFaces(frame);
            std::vector<Rect> newFaces = renderer.getCurrentFaces();
            {
                std::lock_guard<std::mutex> lock(facesMutex);
                currentDetectedFaces = newFaces;
            }
            if(pPresenter) {
                pPresenter->setFaces(newFaces);
            }
            if(windowManager.hwnd && newFaces != prevFaces) {
                prevFaces = newFaces;
                windowManager.updateOverlayWindow();
                if(windowManager.hwnd) {
                    PostMessage(windowManager.hwnd, WM_UPDATE_FACES, 0, 0);
                }
            }
        }
    }
}

/*
** Push frame to Queue
*/
void CaptureController::pushFrameToQueue(const std::vector<std::vector<unsigned char>>& frame) {
    if(!detectionRunning || !faceDetectionEnabled) return;
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if(frameQueue.size() > 5) frameQueue.pop();
        frameQueue.push(frame);
    }
    queueCondition.notify_one();
}

/*
** Get Current Faces
*/
std::vector<Rect> CaptureController::getCurrentFaces() {
    std::lock_guard<std::mutex> lock(facesMutex);
    return currentDetectedFaces;
}

void CaptureController::cleanup() {
    isRunning = false;
    stopDetectionThread();

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
    if(windowManager.hwndVideo) {
        DestroyWindow(windowManager.hwndVideo);
        windowManager.hwndVideo = nullptr;
    }
}