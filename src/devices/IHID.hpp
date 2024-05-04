#pragma once

#include <cstdint>
#include <string>
#include "../helpers/signal/Signal.hpp"

enum eHIDCapabilityType : uint32_t {
    HID_INPUT_CAPABILITY_KEYBOARD = (1 << 0),
    HID_INPUT_CAPABILITY_POINTER  = (1 << 1),
    HID_INPUT_CAPABILITY_TOUCH    = (1 << 2),
};

/*
    Base class for a HID device.
    This could be a keyboard, a mouse, or a touchscreen.
*/
class IHID {
  public:
    virtual ~IHID() = default;
    virtual uint32_t getCapabilities() = 0;

    struct {
        CSignal destroy;
    } events;

    std::string deviceName;
};