#pragma once
#include <windows.h>
#include <dshow.h>
#include <vector>
#include <string>

class IDevice {
    public:
        virtual ~IDevice() = default;

        virtual std::wstring getName() const = 0;
        virtual std::wstring getId() const = 0;
        virtual std::wstring getType() const = 0;
        virtual IMFActivate* getActivate() const = 0;
        virtual bool isAvailable() const = 0;
};