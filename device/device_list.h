#pragma once
#include <windows.h>
#include <dshow.h>
#include <vector>
#include <string>
#include <memory>
#include "device_interface.h"

class DeviceList {    
    private:
        struct Camera : IDevice {
            std::wstring name;
            std::wstring path;
            std::wstring id;
            IMoniker* moniker;
            
            Camera() : moniker(nullptr) {}
            Camera(
                const std::wstring& name,
                const std::wstring& path,
                const std::wstring& id,
                IMoniker* moniker
            ) :
            name(name),
            path(path),
            id(id),
            moniker(moniker) {
                if(moniker) moniker->AddRef();
            }
            
            ~Camera() {
                if(moniker) {
                    moniker->Release();
                    moniker = nullptr;
                }
            }

            std::wstring getName() const override {
                return name;
            }
            std::wstring getId() const override {
                return id;
            }
            std::wstring getType() const override {
                return L"Camera";
            }

            IBaseFilter* createFilter() const override {
                if(!moniker) return nullptr;
                IBaseFilter* pFilter = nullptr;
                HRESULT hr = moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pFilter);
                return SUCCEEDED(hr) ? pFilter : nullptr;
            }

            bool isAvailable() const override {
                IBaseFilter* filter = createFilter();
                if(filter) {
                    filter->Release();
                    return true;
                }
                return false;
            }
        };

    public:
        std::vector<std::unique_ptr<IDevice>> devices;
        DeviceList();
        ~DeviceList();
};