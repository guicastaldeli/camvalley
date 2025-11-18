#include <iostream>
#include "device_list.h"

DeviceList::DeviceList() {
    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr)) {
        std::wcout << L"DeviceList: MFStartup failed: " << hr << std::endl;
    }
}
DeviceList::~DeviceList() {
    cleanup();
}

void DeviceList::cleanup() {
    devices.clear();
}

/*
** Camera
*/
bool DeviceList::setCamera() {
    cleanup();

    std::wcout << L"Enumerating cameras..." << std::endl;

    IMFAttributes* pAttrs = nullptr;
    IMFActivate** ppDevices = nullptr;
    UINT32 count = 0;

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
        std::wcout << L"Failed to set GUID: " << hr << std::endl;
        pAttrs->Release();
        return false;
    }

    hr = MFEnumDeviceSources(pAttrs, &ppDevices, &count);
    pAttrs->Release();
    if(FAILED(hr)) {
        std::wcout << L"MFEnumDeviceSources failed: " << hr << std::endl;
        return false;
    }
    if(count == 0) {
        std::wcout << L"No cameras found" << std::endl;
        return false;
    }

    std::wcout << L"Found " << count << L" camera(s)" << std::endl;

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
        std::wstring deviceName = L"Unkown Camera";
        std::wstring deviceId = L"camera_" + std::to_wstring(i);
        if(SUCCEEDED(hr) && friendlyName) {
            deviceName = friendlyName;
        }

        hr = ppDevices[i]->GetAllocatedString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            &symbolicLink,
            &linkLength
        );
        if(SUCCEEDED(hr) && symbolicLink) {
            deviceId = symbolicLink;
        }

        std::wcout << L"Camera" << i << L": " << deviceName << std::endl;
        std::wcout << L" ID: " << deviceId << std::endl;

        devices.push_back(
            std::make_unique<Camera>(
                deviceName,
                symbolicLink ? std::wstring(symbolicLink) : L"",
                deviceId,
                ppDevices[i]
            )
        );

        if(friendlyName) CoTaskMemFree(friendlyName);
        if(symbolicLink) CoTaskMemFree(symbolicLink);
    }
    CoTaskMemFree(ppDevices);
    
    std::wcout << L"Successfully added " << devices.size() << L" devices to list" << std::endl;
    return !devices.empty();
}