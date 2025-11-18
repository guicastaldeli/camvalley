#include <iostream>
#include "device_list.h"

DeviceList::DeviceList() {
    MFStartup(MF_VERSION);
}
DeviceList::~DeviceList() {
    cleanup();
    MFShutdown();
}

/*
** Camera
*/
bool DeviceList::setCamera() {
    cleanup();

    IMFAttributes* pAttrs = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

    HRESULT hr = MFCreateAttributes(&pAttrs, 1);
    if(FAILED(hr)) return false;

    hr = pAttrs->SetGUID(
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
    );
    if(FAILED(hr)) {
        pAttrs->Release();
        return false;
    }

    hr = MFEnumDeviceSources(pAttrs, &ppDevices, &count);
    pAttrs->Release();
    if(FAILED(hr) || count == 0) return false;

    for(UINT32 i = 0; i < count; i++) {
        WCHAR* friendlyName = nullptr;
        UINT32 nameLength = 0;
        WCHAR* symbolicLink = nullptr;
        UINT32 linkLength = 0;

        hr = ppDevices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
            &friendlyName,
            &nameLength
        );
        hr = ppDevices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            &symbolicLink,
            &linkLength
        );
        if(SUCCEEDED(hr) && friendlyName) {
            std::wstring deviceId = L"camera_" + std::to_wstring(i);
            if(symbolicLink) deviceId = symbolicLink;

            devices.push_back(
                std::make_unique<Camera>(
                    std::wstring(friendlyName),
                    symbolicLink ? std::wstring(symbolicLink) : L"",
                    deviceId,
                    ppDevices[i]
                )
            );
            ppDevices[i]->AddRef();
        }

        if(friendlyName) CoTaskMemFree(friendlyName);
        if(symbolicLink) CoTaskMemFree(symbolicLink);
    }

    CoTaskMemFree(ppDevices);
    return !devices.empty();
}

void DeviceList::cleanup() {
    devices.clear();
}