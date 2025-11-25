#include "../window_manager.h"
#include "evr_presenter.h"
#include "evr_activate.h"
#include <d3dx9.h>
#include <iostream>
#include <shlwapi.h>
#include <mfapi.h>
#include <mfidl.h>

EVRPresenter::EVRPresenter(WindowManager& wm, CaptureController& captureController) :
    m_windowManager(wm),
    m_captureController(captureController),
    m_cRef(1),
    m_pD3D(NULL),
    m_pDevice(NULL),
    m_pLine(NULL),
    pOverlay(nullptr)
{
    pOverlay = new Overlay(this);
}

EVRPresenter::~EVRPresenter() {
    if(m_pLine) m_pLine->Release();
    if(m_pDevice) m_pDevice->Release();
    if(m_pD3D) m_pD3D->Release();
}

/*
** Query Interface
*/
STDMETHODIMP EVRPresenter::QueryInterface(REFIID riid, void** ppv) {
    static const QITAB qit[] = {
        QITABENT(EVRPresenter, IMFVideoPresenter),
        QITABENT(EVRPresenter, IMFClockStateSink),
        {0}
    };
    return QISearch(this, qit, riid, ppv);
}

STDMETHODIMP_(ULONG) EVRPresenter::AddRef() {
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) EVRPresenter::Release() {
    ULONG count = InterlockedDecrement(&m_cRef);
    if(count == 0) delete this;
    return count;
}

/*
** Process Message
*/
STDMETHODIMP EVRPresenter::ProcessMessage(
    MFVP_MESSAGE_TYPE eMessage,
    ULONG_PTR ulParam 
) {
    switch(eMessage) {
        case MFVP_MESSAGE_PROCESSINPUTNOTIFY:
            pOverlay->Draw();
            break;
        case MFVP_MESSAGE_BEGINSTREAMING:
            InitD3D();
            break;
        case MFVP_MESSAGE_ENDSTREAMING:
            if(m_pLine) {
                m_pLine->Release();
                m_pLine = NULL;
            }
            break;
    }
    return S_OK;
}

STDMETHODIMP EVRPresenter::GetCurrentMediaType(IMFVideoMediaType** ppMediaType) {
    return E_NOTIMPL;
}

STDMETHODIMP EVRPresenter::GetService(REFGUID guidService, REFIID riid, LPVOID* ppvObject) {
    if (riid == __uuidof(IDirect3DDevice9)) {
        if (m_pDevice) {
            *ppvObject = m_pDevice;
            m_pDevice->AddRef();
            return S_OK;
        }
    }
    return E_NOINTERFACE;
}

STDMETHODIMP EVRPresenter::OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) {
    return S_OK;
}

STDMETHODIMP EVRPresenter::OnClockStop(MFTIME hnsSystemTime) {
    return S_OK;
}

STDMETHODIMP EVRPresenter::OnClockPause(MFTIME hnsSystemTime) {
    return S_OK;
}

STDMETHODIMP EVRPresenter::OnClockRestart(MFTIME hnsSystemTime) {
    return S_OK;
}

STDMETHODIMP EVRPresenter::OnClockSetRate(MFTIME hnsSystemTime, float flRate) {
    return S_OK;
}

STDMETHODIMP EVRPresenter::PresentSample(IMFSample* pSample, LONGLONG llTargetTime) {
    pOverlay->Draw();
    return S_OK;
}

/*
** Init
*/
HRESULT EVRPresenter::Init() {
    return InitD3D();
}

HRESULT EVRPresenter::InitD3D() {
    HRESULT hr = S_OK;
    m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if(!m_pD3D) {
        std::wcout << L"Failed to create D3D!" << std::endl;
        return E_FAIL;
    }

    RECT rect;
    GetClientRect(m_windowManager.hwndVideo, &rect);

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = m_windowManager.hwndVideo;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dpp.BackBufferWidth = rect.right - rect.left;
    d3dpp.BackBufferHeight = rect.bottom - rect.top;

    hr = m_pD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        m_windowManager.hwndVideo,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp,
        &m_pDevice
    );
    if(FAILED(hr)) {
        std::wcout << L"FAiled to create D3D device!: " << hr << std::endl;
        return hr;
    }

    hr = D3DXCreateLine(m_pDevice, &m_pLine);
    if(FAILED(hr)) {
        std::wcout << L"Failed to create line: " << hr << std::endl;
    }
    return hr;
}

void EVRPresenter::setFaces(const std::vector<Rect>& faces) {
    m_currentFaces = faces;
}

/*
** Create
*/
HRESULT CreateEVRPresenter(WindowManager& windowManager, IMFActivate** ppActivate) {
    *ppActivate = new EVRActivate(windowManager);
    return (*ppActivate) ? S_OK : E_OUTOFMEMORY;
}