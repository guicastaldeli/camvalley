#pragma comment(lib, "evr.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "dxva2.lib")
#include "capture_controller.h"
#include "../renderer/d2d_renderer.h"
#include <iostream>
#include <evr.h>
#include <d3d9.h>
#include <cmath>

CaptureController::CaptureController(WindowManager& wm) :
    windowManager(wm),
    m_cRef(1),
    pVideoSource(nullptr),
    sourceReader(nullptr),
    frameConverter(nullptr),
    ppDevices(nullptr),
    deviceCount(0),
    pSession(nullptr),
    pVideoDisplay(nullptr),
    isRunning(false),
    faceDetectionEnabled(true),
    classifierRenderer(),
    d2dRenderer(nullptr),
    useD2D(false),
    frameReady(false)
{
    sourceReader = new SourceReader();
    frameConverter = new FrameConverter();

    InitializeCriticalSection(&frameCriticalSection);
    HRESULT hr = MFStartup(MF_VERSION);
    if(FAILED(hr)) std::wcout << L"MFStartup failed: " << hr << std::endl;
}
CaptureController::~CaptureController() {
    cleanup();
    stopDetectionThread();
    DeleteCriticalSection(&frameCriticalSection);
    MFShutdown();
    if(d2dRenderer) {
        delete d2dRenderer;
        d2dRenderer = nullptr;
    }
}

bool CaptureController::init(int x, int y, int w, int h) {
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
        windowManager.resize(x, y, w, h);
    
        if(!refreshDevices()) {
            std::wcout << L"No devices found!" << std::endl;
            return false;
        }
        
        std::wcout << L"Found " << deviceController.getDeviceCount() << L" devices" << std::endl;
        if(deviceController.getDeviceCount() > 0) {
            auto* fDevice = deviceController.getDevice(1);
            if(fDevice) {
                std::wcout << L"Using device: " << fDevice->getName() << std::endl;
                return initWithDevice(windowManager.hwnd, fDevice->getId(), x, y, w, h);
            }
        }
    } catch(...) {
        std::wcout << L"ERR capture controller!!" << std::endl;
        return false;
    }

    std::wcout << L"Capture controller init" << std::endl;
    return true;
}

bool CaptureController::initWithDevice(
    HWND parent,
    const std::wstring& deviceId,
    int x,
    int y,
    int w,
    int h
) {
    currentDeviceId = deviceId; 
    cleanup();
    if(!windowManager.hwndVideo) {
        if(!windowManager.createVideoWindow()) {
            return false;
        }
    }
    windowManager.resize(x, y, w, h);
    return setupDevice(deviceId);
}

/*
** Set Direct2D
*/
bool CaptureController::setD2D() {
    if(!windowManager.hwndVideo) {
        std::wcout << "No video available for direct2d" << std::endl;
        return false;
    }
    if(!d2dRenderer) {
        d2dRenderer = new D2DRenderer();
    }
    if(!d2dRenderer->init(windowManager.hwndVideo)) {
        std::wcout << L"failed to init direct2d" << std::endl;
        return false;
    }

    useD2D = true;
    std::wcout << L"d2d render setup complete!!!" << std::endl;
    return true;
}

/*
** Setup Device
*/
bool CaptureController::setupDevice(const std::wstring& deviceId) {
    if(!deviceController.resetCamera(deviceId)) {
        std::wcout << L"Warning: Could not reset camera before setup" << std::endl;
    }

    IDevice* device = deviceController.findDevice(deviceId);
    if(!device) {
        std::wcout << L"Device not found in controller" << std::endl;
        return false;
    }
    std::wcout << L"Found device: " << device->getName() << std::endl;
    
    if(!windowManager.createOverlayWindow()) {
        std::wcout << L"Failed to create overlay window" << std::endl;
        return false;
    }

    IMFMediaSource* pCaptureSource = nullptr;
    IMFAttributes* pAttrsCapture = nullptr;
    HRESULT hr = MFCreateAttributes(&pAttrsCapture, 1);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create attributes" << std::endl;
        return false;
    }

    hr = pAttrsCapture->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );
    if(FAILED(hr)) {
        std::wcout << L"Failed to set GUID" << std::endl;
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

    if(!setD2D()) {
        std::wcout << L"Failed to set D2D" << std::endl;
        pAttrsCapture->Release();
        return false;
    }

    hr = MFCreateDeviceSource(pAttrsCapture, &pCaptureSource);
    pAttrsCapture->Release();
    if(FAILED(hr)) {
        std::wcout << L"Failed to create device source: " << hr << std::endl;
        return false;
    }

    if(!sourceReader->createSourceReaderWithRes(this, pCaptureSource, 1280, 720)) {
        std::wcout << L"Failed to create source reader with 1280x720" << std::endl;
        pCaptureSource->Release();
        return false;
    }
    pCaptureSource->Release();
    
    return true;
}

IDevice* CaptureController::getCurrentDevice() const {
    return deviceController.findDevice(currentDeviceId);
}

bool CaptureController::refreshDevices() {
    return deviceController.setDevices();
}

/*
** Enable Face Detection
*/
void CaptureController::enableFaceDetection(bool enable) {
    faceDetectionEnabled = enable;
    if(enable) {
        if(!classifierRenderer.isCascadeLoaded()) {
            std::wcout << L"Loading cascade..." << std::endl;
            loadCascade("../.data/haarcascade_frontalface_default.xml");
            classifierRenderer.forceEnable();
        }
        startDetectionThread();
    } else {
        std::wcout << "Enable face detection FATAL ERR." << std::endl;
    }
}

/*
** Load Cascade
*/
void CaptureController::loadCascade(const std::string& file) {
    if(classifierRenderer.load(file)) {
        std::wcout << L"File loaded!: " << file.c_str() << std::endl;
    } else {
        std::wcout << L"Failed to load file :( " << file.c_str() << std::endl;  
    }
}

void CaptureController::getCurrentFrame(std::vector<std::vector<unsigned char>>& frame) {
    EnterCriticalSection(&frameCriticalSection);
    if(frameReady) frame = currentFrame;
    LeaveCriticalSection(&frameCriticalSection);
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
    std::wcout << L"OnReadSample entered" << std::endl;
    if(FAILED(hrStatus)) {
        std::wcout << L"On Read Sample failed!: " << hrStatus << std::endl;
        return hrStatus;
    }
    try {
        if(dwStreamFlags & MF_SOURCE_READERF_ENDOFSTREAM) {
            std::wcout << L"End of stream reached" << std::endl;
            return S_OK;
        }
        if(pSample == nullptr) {
            std::wcout << L"No sample available, requesting next frame" << std::endl;
            if(sourceReader->pReader) {
                sourceReader->pReader->ReadSample(
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

        UINT32 frameWidth = 0;
        UINT32 frameHeight = 0;
        GUID pixelFormat = GUID_NULL;
        IMFMediaType* pMediaType = nullptr;
        HRESULT hr = sourceReader->pReader->GetCurrentMediaType(
            MF_SOURCE_READER_FIRST_VIDEO_STREAM,
            &pMediaType
        );
        if(SUCCEEDED(hr) && pMediaType) {
            hr = MFGetAttributeSize(
                pMediaType,
                MF_MT_FRAME_SIZE,
                &frameWidth,
                &frameHeight
            );
            pMediaType->GetGUID(MF_MT_SUBTYPE, &pixelFormat);
            pMediaType->Release();
            
            if(SUCCEEDED(hr)) {
                std::wcout << L"Actual frame resolution: " << frameWidth 
                        << L"x" << frameHeight << std::endl;
            }
        }
        
        if(useD2D) {
            std::wcout << L"Rendering with D2D" << std::endl;
            if(d2dRenderer->renderFrame(pSample, frameWidth, frameHeight, pixelFormat)) {
                windowManager.updateOverlayWindow();
            }
        }

        IMFMediaBuffer* pBuffer = nullptr;
        hr = pSample->ConvertToContiguousBuffer(&pBuffer);
        if(SUCCEEDED(hr)) {
            BYTE* pData = nullptr;
            DWORD maxLength;
            DWORD currentLength;

            hr = pBuffer->Lock(&pData, &maxLength, &currentLength);
            if(SUCCEEDED(hr) && pData && currentLength > 0) {
                auto frame = frameConverter->convertToGrayscale(
                    pData, 
                    currentLength,
                    sourceReader->pReader
                );
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
        
        if(sourceReader->pReader) {
            sourceReader->pReader->ReadSample(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                0,
                nullptr,
                nullptr,
                nullptr,
                nullptr
            );
        }
    } catch(const std::exception& e) {
        std::wcout << L"Exception in OnReadSample: " << e.what() << std::endl;
    } catch (...) {
        std::wcout << L"Unknown exception in OnReadSample" << std::endl;
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
            classifierRenderer.processFrameForFaces(frame);
            std::vector<Rect> newFaces = classifierRenderer.getCurrentFaces();
            {
                std::lock_guard<std::mutex> lock(facesMutex);
                currentDetectedFaces = newFaces;
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
        if(frameQueue.size() >= 5) return;
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

    if(!currentDeviceId.empty()) {
        deviceController.resetCamera(currentDeviceId);
    }
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
    if(sourceReader->pReader) {
        sourceReader->pReader->Release();
        sourceReader->pReader = nullptr;
    }
    if(pVideoSource) {
        pVideoSource->Release();
        pVideoSource = nullptr;
    }
    if(d2dRenderer) {
        delete d2dRenderer;
        d2dRenderer = nullptr;
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
    if(windowManager.hwndOverlay) {
        DestroyWindow(windowManager.hwndOverlay);
        windowManager.hwndOverlay = nullptr;
    }
}