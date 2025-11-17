#include <windows.h>
#include <dshow.h>
#include <vector>
#include <string>
#include <device_list.h>
#include <device_interface.h>

class DeviceController {
    private:
        DeviceList deviceList;

    public:
        DeviceController();
        ~DeviceController();

        bool enumDevices();
        size_t getDeviceCount() const {
            return deviceList.devices.size();
        }
        IDevice* getDevice(size_t i) const;
        std::vector<IDevice*> getDevices() const;

        IDevice* findDevice(const std::wstring& id) const;
        std::vector<IDevice*> findDevices(const std::wstring& nameContains) const;
        std::vector<IDevice*> getDevicesByType(const std::wstring& type) const;

        void addDevice(std::unique_ptr<IDevice> device);
        void clear();
        void cleanup();
};