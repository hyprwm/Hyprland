#pragma once

#include "content-type-v1.hpp"
#include <cstdint>

namespace NContentType {

    enum eContentType : uint8_t {
        CONTENT_TYPE_NONE  = 0,
        CONTENT_TYPE_PHOTO = 1,
        CONTENT_TYPE_VIDEO = 2,
        CONTENT_TYPE_GAME  = 3,
    };

    eContentType fromString(const std::string NAME);
    eContentType fromWP(wpContentTypeV1Type contentType);
    uint16_t     toDRM(eContentType contentType);
}