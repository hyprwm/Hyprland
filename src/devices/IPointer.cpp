#include "IPointer.hpp"

uint32_t IPointer::getCapabilities() {
    return HID_INPUT_CAPABILITY_POINTER;
}
