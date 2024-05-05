#include "ITouch.hpp"

uint32_t ITouch::getCapabilities() {
    return HID_INPUT_CAPABILITY_TOUCH;
}

eHIDType ITouch::getType() {
    return HID_TYPE_TOUCH;
}
