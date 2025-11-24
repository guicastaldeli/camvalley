#pragma once
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <vector>
#include <string>
#include <memory>
#include "../device/device_list.h"
#include "../device/device_interface.h"

class DeviceController {
    private:
        DeviceList deviceList;
    
    public:
        DeviceController();
        ~DeviceController();

        bool setDevices();
        IDevice* getDevice(size_t i) const;
        std::vector<IDevice*> getDevices() const;
        size_t getDeviceCount() const;

        IDevice* findDevice(const std::wstring& id) const;
        std::vector<IDevice*> findDevices(const std::wstring& nameContains) const;
        std::vector<IDevice*> getDevicesByType(const std::wstring& type) const;

        void addDevice(std::unique_ptr<IDevice> device);
        void cleanup();
};