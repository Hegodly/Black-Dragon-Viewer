#include "kami/world/device.hpp"

int main() {
    kami::world::Device device;
    if (device.status() != kami::world::DeviceStatus::NotInitialized) return 1;
    if (device.native_device() != nullptr) return 2;
    if (device.graphics_queue() != nullptr) return 3;

    // GitHub CI may lack a hardware GPU. This tests safe initialization
    // without claiming that a rendered Second Life world exists.
    const auto status = device.initialize(true);
    if (device.is_ready()) {
        if (status != kami::world::DeviceStatus::ReadyHardware &&
            status != kami::world::DeviceStatus::ReadySoftware) return 4;
        if (!device.native_device() || !device.graphics_queue()) return 5;
        if (device.adapter_name().empty()) return 6;
    } else {
        if (status != kami::world::DeviceStatus::Unavailable) return 7;
        if (device.native_device() || device.graphics_queue()) return 8;
    }
    return 0;
}
