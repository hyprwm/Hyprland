#pragma once

#include "content-type-v1.hpp"
#include <cstdint>

namespace NContentType {

    enum eContentType : uint8_t {
        NONE  = 0,
        PHOTO = 1,
        VIDEO = 2,
        GAME  = 3,
    };

    eContentType fromString(const std::string name);
    eContentType fromWP(wpContentTypeV1Type contentType);
    uint16_t     toDRM(eContentType contentType);
}