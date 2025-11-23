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
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable> 
#include "device_controller.h"
#include "classifier/renderer.h"

class CaptureController : public IMFSourceReaderCallback {
    private:
        ULONG m_cRef;
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

        CRITICAL_SECTION frameCriticalSection;
        std::vector<std::vector<unsigned char>> currentFrame;
        bool frameReady;
        
        std::thread detectionThread;
        std::atomic<bool> detectionRunning{false};
        std::queue<std::vector<std::vector<unsigned char>>> frameQueue;
        std::mutex queueMutex;
        std::condition_variable queueCondition;
        std::vector<Rect> currentDetectedFaces;
        std::mutex facesMutex;

        bool createVideoWindow();
        bool setupDevice(const std::wstring& deviceId);
        bool createSourceReader(IMFMediaSource* pCaptureSource);
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
        std::vector<std::vector<unsigned char>> convertFrameToGrayscale(
            BYTE* pData, 
            DWORD length
        );
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
        void getCurrentFrame(std::vector<std::vector<unsigned char>>& frame);
        void cleanup();

        STDMETHODIMP QueryInterface(REFIID riid, void** ppv);
        STDMETHODIMP_(ULONG) AddRef();
        STDMETHODIMP_(ULONG) Release();

        STDMETHODIMP OnReadSample(
            HRESULT hrStatus,
            DWORD dwStreamIndex,
            DWORD dwStreamFlags,
            LONGLONG llTimestamp,
            IMFSample* pSample
        );
        STDMETHODIMP OnEvent(
            DWORD dwStreamIndex,
            IMFMediaEvent* pEvent
        );
        STDMETHODIMP OnFlush(DWORD dwStreamIndex);

        void startDetectionThread();
        void stopDetectionThread();
        void detectionWorker();
        void pushFrameToQueue(const std::vector<std::vector<unsigned char>>& frame);
        std::vector<Rect> getCurrentFaces();
};