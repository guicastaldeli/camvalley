#include "device_controller.h"
#include <iostream>

DeviceController::DeviceController() {}
DeviceController::~DeviceController() {
    cleanup();
}

bool DeviceController::setDevices() {
    return deviceList.setCamera();
}

size_t DeviceController::getDeviceCount() const {
    return deviceList.devices.size();
}

/*
** Get Device
*/
IDevice* DeviceController::getDevice(size_t i) const {
    if(i < deviceList.devices.size()) {
        return deviceList.devices[i].get();
    }
    return nullptr;
}

std::vector<IDevice*> DeviceController::getDevices() const {
    std::vector<IDevice*> result;
    for(auto& device : deviceList.devices) {
        result.push_back(device.get());
    }
    return result;
}

std::vector<IDevice*> DeviceController::getDevicesByType(const std::wstring& type) const {
    std::vector<IDevice*> result;
    for(auto& device : deviceList.devices) {
        if(device->getType() == type) {
            result.push_back(device.get());
        }
    }
    return result;
}

/*
** Find Device
*/
IDevice* DeviceController::findDevice(const std::wstring& id) const {
    for(auto& device : deviceList.devices) {
        if(device->getId() == id) {
            return device.get();
        }
    }
    return nullptr;
}

std::vector<IDevice*> DeviceController::findDevices(const std::wstring& nameContains) const {
    std::vector<IDevice*> result;
    for(auto& device : deviceList.devices) {
        if(device->getName().find(nameContains) != std::wstring::npos) {
            result.push_back(device.get());
        }
    }
    return result;
}

void DeviceController::addDevice(std::unique_ptr<IDevice> device) {
    deviceList.devices.push_back(std::move(device));
}

bool DeviceController::resetCamera(const std::wstring& deviceId) {
    IDevice* device = findDevice(deviceId);
    if(!device) {
        std::wcout << L"Device not found for reset" << deviceId << std::endl;
        return false;
    }

    IMFAttributes* pAttrs = nullptr;
    IMFMediaSource* pTempSource = nullptr;
    
    HRESULT hr = MFCreateAttributes(&pAttrs, 1);
    if(SUCCEEDED(hr)) {
        hr = pAttrs->SetGUID(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
        );
        hr = pAttrs->SetString(
            MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            deviceId.c_str()
        );
        
        if (SUCCEEDED(hr)) {
            hr = MFCreateDeviceSource(pAttrs, &pTempSource);
            if (SUCCEEDED(hr) && pTempSource) {
                pTempSource->Shutdown();
                pTempSource->Release();
                std::wcout << L"Camera reset successfully: " << deviceId << std::endl;
            }
        }
        
        pAttrs->Release();
    }

    Sleep(100);
    return SUCCEEDED(hr);
}

void DeviceController::cleanup() {
    deviceList.cleanup();
}