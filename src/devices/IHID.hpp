#pragma once

#include <cstdint>
#include <string>
#include "../helpers/signal/Signal.hpp"

enum eHIDCapabilityType : uint32_t {
    HID_INPUT_CAPABILITY_KEYBOARD = (1 << 0),
    HID_INPUT_CAPABILITY_POINTER  = (1 << 1),
    HID_INPUT_CAPABILITY_TOUCH    = (1 << 2),
    HID_INPUT_CAPABILITY_TABLET   = (1 << 3),
};

enum eHIDType {
    HID_TYPE_UNKNOWN = 0,
    HID_TYPE_POINTER,
    HID_TYPE_KEYBOARD,
    HID_TYPE_TOUCH,
    HID_TYPE_TABLET,
    HID_TYPE_TABLET_TOOL,
    HID_TYPE_TABLET_PAD,
};

/*
    Base class for a HID device.
    This could be a keyboard, a mouse, or a touchscreen.
*/
class IHID {
  public:
    virtual ~IHID() {}

    virtual uint32_t getCapabilities() = 0;
    virtual eHIDType getType();

    struct {
        CSignal destroy;
    } events;

    std::string deviceName;
};