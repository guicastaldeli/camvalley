#pragma once
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#include <algorithm>
#include "device_controller.h"

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

void DeviceController::cleanup() {
    clear();
}