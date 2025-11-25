#pragma once
#include <evr.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include <mfapi.h>
#include <mfidl.h>
#include "../classifier/rect.h"
#include "../controller/capture_controller.h"
#include "../classifier/overlay.h"

class WindowManager;
class CaptureController;
class EVRPresenter : public IMFVideoPresenter {
    public:
        EVRPresenter(
            WindowManager& wm,
            CaptureController& captureController
        );
        virtual ~EVRPresenter();
        bool setEVR();

        STDMETHOD(ProcessMessage)(MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam);
        STDMETHOD(GetCurrentMediaType)(IMFVideoMediaType** ppMediaType);

        STDMETHOD(QueryInterface)(REFIID riid, void** ppv);
        STDMETHOD_(ULONG, AddRef)();
        STDMETHOD_(ULONG, Release)();

        STDMETHOD(GetService)(REFGUID guidService, REFIID riid, LPVOID* ppvObject);
        STDMETHOD(OnClockStart)(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
        STDMETHOD(OnClockStop)(MFTIME hnsSystemTime);
        STDMETHOD(OnClockPause)(MFTIME hnsSystemTime);
        STDMETHOD(OnClockRestart)(MFTIME hnsSystemTime);
        STDMETHOD(OnClockSetRate)(MFTIME hnsSystemTime, float flRate);
        STDMETHOD(PresentSample)(IMFSample* pSample, LONGLONG llTargetTime);

        void setFaces(const std::vector<Rect>& faces);
        HRESULT Init();

        WindowManager& m_windowManager;
        CaptureController& m_captureController;
        std::vector<Rect> m_currentFaces;
        ULONG m_cRef;
        
        IDirect3D9* m_pD3D;
        IDirect3DDevice9* m_pDevice;
        ID3DXLine* m_pLine;
        
        HRESULT InitD3D();
        Overlay* pOverlay;
};

HRESULT CreateEVRPresenter(WindowManager& windowManager, IMFActivate** ppActivate);