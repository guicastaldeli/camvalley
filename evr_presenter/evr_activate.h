#pragma once
#include <evr.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <vector>
#include <shlwapi.h>
#include <mfapi.h>
#include <mfidl.h>
#include "evr_presenter.h"

class EVRActivate : public IMFActivate {
    public:
        EVRActivate(WindowManager& windowManager) :
            m_windowManager(windowManager),
            m_cRef(1) {}
        
        STDMETHOD(QueryInterface)(REFIID riid, void** ppv) {
            static const QITAB qit[] = {
                QITABENT(EVRActivate, IMFActivate),
                {0}
            };
            return QISearch(this, qit, riid, ppv);
        }
    
        STDMETHOD_(ULONG, AddRef)() { return InterlockedIncrement(&m_cRef); }
        STDMETHOD_(ULONG, Release)() {
            ULONG count = InterlockedDecrement(&m_cRef);
            if (count == 0) delete this;
            return count;
        }
        
        STDMETHOD(ActivateObject)(REFIID riid, void** ppv) {
            EVRPresenter* pPresenter = new EVRPresenter(m_windowManager);
            HRESULT hr = pPresenter->QueryInterface(riid, ppv);
            if (SUCCEEDED(hr)) {
                pPresenter->Init();
            } else {
                delete pPresenter;
            }
            return hr;
        }
        
        STDMETHOD(ShutdownObject)() { return S_OK; }
        STDMETHOD(DetachObject)() { return S_OK; }
        STDMETHOD(GetItem)(REFGUID guidKey, PROPVARIANT* pValue) { return E_NOTIMPL; }
        STDMETHOD(GetItemType)(REFGUID guidKey, MF_ATTRIBUTE_TYPE* pType) { return E_NOTIMPL; }
        STDMETHOD(CompareItem)(REFGUID guidKey, REFPROPVARIANT Value, BOOL* pbResult) { return E_NOTIMPL; }
        STDMETHOD(Compare)(IMFAttributes* pTheirs, MF_ATTRIBUTES_MATCH_TYPE MatchType, BOOL* pbResult) { return E_NOTIMPL; }
        STDMETHOD(GetUINT32)(REFGUID guidKey, UINT32* punValue) { return E_NOTIMPL; }
        STDMETHOD(GetUINT64)(REFGUID guidKey, UINT64* punValue) { return E_NOTIMPL; }
        STDMETHOD(GetDouble)(REFGUID guidKey, double* pfValue) { return E_NOTIMPL; }
        STDMETHOD(GetGUID)(REFGUID guidKey, GUID* pguidValue) { return E_NOTIMPL; }
        STDMETHOD(GetStringLength)(REFGUID guidKey, UINT32* pcchLength) { return E_NOTIMPL; }
        STDMETHOD(GetString)(REFGUID guidKey, LPWSTR pwszValue, UINT32 cchBufSize, UINT32* pcchLength) { return E_NOTIMPL; }
        STDMETHOD(GetAllocatedString)(REFGUID guidKey, LPWSTR* ppszValue, UINT32* pcchLength) { return E_NOTIMPL; }
        STDMETHOD(GetBlobSize)(REFGUID guidKey, UINT32* pcbSize) { return E_NOTIMPL; }
        STDMETHOD(GetBlob)(REFGUID guidKey, UINT8* pBuf, UINT32 cbBufSize, UINT32* pcbBlobSize) { return E_NOTIMPL; }
        STDMETHOD(GetAllocatedBlob)(REFGUID guidKey, UINT8** ppBuf, UINT32* pcbSize) { return E_NOTIMPL; }
        STDMETHOD(GetUnknown)(REFGUID guidKey, REFIID riid, LPVOID* ppv) { return E_NOTIMPL; }
        STDMETHOD(SetItem)(REFGUID guidKey, REFPROPVARIANT Value) { return S_OK; }
        STDMETHOD(DeleteItem)(REFGUID guidKey) { return S_OK; }
        STDMETHOD(DeleteAllItems)() { return S_OK; }
        STDMETHOD(SetUINT32)(REFGUID guidKey, UINT32 unValue) { return S_OK; }
        STDMETHOD(SetUINT64)(REFGUID guidKey, UINT64 unValue) { return S_OK; }
        STDMETHOD(SetDouble)(REFGUID guidKey, double fValue) { return S_OK; }
        STDMETHOD(SetGUID)(REFGUID guidKey, REFGUID guidValue) { return S_OK; }
        STDMETHOD(SetString)(REFGUID guidKey, LPCWSTR wszValue) { return S_OK; }
        STDMETHOD(SetBlob)(REFGUID guidKey, const UINT8* pBuf, UINT32 cbSize) { return S_OK; }
        STDMETHOD(SetUnknown)(REFGUID guidKey, IUnknown* pUnknown) { return S_OK; }
        STDMETHOD(LockStore)() { return S_OK; }
        STDMETHOD(UnlockStore)() { return S_OK; }
        STDMETHOD(GetCount)(UINT32* pcItems) { return E_NOTIMPL; }
        STDMETHOD(GetItemByIndex)(UINT32 unIndex, GUID* pguidKey, PROPVARIANT* pValue) { return E_NOTIMPL; }
        STDMETHOD(CopyAllItems)(IMFAttributes* pDest) { return E_NOTIMPL; }

    private:
        WindowManager& m_windowManager;
        ULONG m_cRef;
};