#pragma once
#include <Windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <vector>
#include <string>
#include <memory>
#include "device_interface.h"

class DeviceList {    
    private:
        struct Camera : public IDevice {
            std::wstring friendlyName;
            std::wstring symbolicLink;
            std::wstring id;
            IMFActivate* activate;

            Camera() : activate(nullptr) {}
            Camera(
                const std::wstring& name,
                const std::wstring& link,
                const std::wstring& deviceId,
                IMFActivate* pActivate
            ) :
            friendlyName(name),
            symbolicLink(link),
            id(deviceId),
            activate(pActivate) {
                if(activate) activate->AddRef();
            }

            ~Camera() {
                if(activate) {
                    activate->Release();
                    activate = nullptr;
                }
            }

            std::wstring getName() const override {
                return friendlyName;
            }
            std::wstring getId() const override {
                return id;
            }
            std::wstring getType() const override {
                return L"Camera";
            }
            IMFActivate* getActivate() const override {
                return activate;
            }
            bool isAvailable() const override {
                return activate != nullptr;
            }
        };
    
    public:
        std::vector<std::unique_ptr<IDevice>> devices;
        DeviceList();
        ~DeviceList();
        void cleanup();

        bool setCamera();
};