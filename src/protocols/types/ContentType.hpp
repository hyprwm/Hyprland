#pragma once

#include "content-type-v1.hpp"
#include <cstdint>

namespace NContentType {
    wpContentTypeV1Type fromString(const std::string name);
    uint16_t            toDRM(wpContentTypeV1Type contentType);
}