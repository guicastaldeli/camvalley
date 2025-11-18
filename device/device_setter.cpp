#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#include <iostream>
#include "device_list.h"

DeviceList::DeviceList() {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
}
DeviceList::~DeviceList() {
    cleanup();
    CoUninitialize();
}

/*
** Set Camera
*/
bool DeviceList::setCamera() {
    cleanup();

    ICreateDevEnum* pDevEnum = nullptr;
    IEnumMoniker* pEnum = nullptr;

    HRESULT hr = CoCreateInstance(
        CLSID_SystemDeviceEnum,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_ICreateDevEnum,
        (void**)&pDevEnum
    );
    hr = pDevEnum->CreateClassEnumerator(
        CLSID_VideoInputDeviceCategory,
        &pEnum,
        0
    );
    if(hr != S_OK || !pEnum) {
        pDevEnum->Release();
        return false;
    }
    if(FAILED(hr)) {
        return false;
    }

    IMoniker* pMoniker = nullptr;
    int cameraCount = 0;
    while(pEnum->Next(1, &pMoniker, nullptr) == S_OK) {
        IPropertyBag* pPropBag = nullptr;
        hr = pMoniker->BindToStorage(
            0, 
            0, 
            IID_IPropertyBag, 
            (void**)&pPropBag
        );

        if(SUCCEEDED(hr)) {
            VARIANT name;
            VARIANT path;
            VariantInit(&name);
            VariantInit(&path);

            hr = pPropBag->Read(L"FriendlyName", &name, 0);
            if(SUCCEEDED(hr) && name.vt == VT_BSTR) {
                std::wstring friendlyName = name.bstrVal;
                std::wstring devicePath = L"";
                std::wstring deviceId = L"camera_" + std::to_wstring(cameraCount++);

                hr = pPropBag->Read(L"DevicePath", &path, 0);
                if(SUCCEEDED(hr) && path.vt == VT_BSTR) {
                    devicePath = path.bstrVal;
                    deviceId = L"camera_path_" + std::to_wstring(cameraCount); 
                }

                devices.push_back(
                    std::make_unique<Camera>(
                        friendlyName,
                        devicePath,
                        deviceId,
                        pMoniker
                    )
                );
            }

            VariantClear(&name);
            VariantClear(&path);
            pPropBag->Release();
        }

        pMoniker->Release();
    }

    pEnum->Release();
    pDevEnum->Release();
    return !devices.empty();
}

void DeviceList::cleanup() {
    devices.clear();
}