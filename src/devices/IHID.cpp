#include "IHID.hpp"

#include <libinput.h>

IHID::IHID(libinput_device* device) {
    if (!device)
        return;

    const auto SYSNAME = libinput_device_get_sysname(device);
    if (SYSNAME)
        m_sysname = SYSNAME;
}

eHIDType IHID::getType() {
    return HID_TYPE_UNKNOWN;
}
