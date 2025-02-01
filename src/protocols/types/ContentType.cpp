#include "ContentType.hpp"
#include <drm_mode.h>
#include <stdexcept>
#include <format>

namespace NContentType {
    static std::unordered_map<std::string, eContentType> const table = {{"none", NONE}, {"photo", PHOTO}, {"video", VIDEO}, {"game", GAME}};

    eContentType                                               fromString(const std::string name) {
        auto it = table.find(name);
        if (it != table.end())
            return it->second;
        else
            throw std::invalid_argument(std::format("Unknown content type {}", name));
    }

    eContentType fromWP(wpContentTypeV1Type contentType) {
        switch (contentType) {
            case WP_CONTENT_TYPE_V1_TYPE_NONE: return NONE;
            case WP_CONTENT_TYPE_V1_TYPE_PHOTO: return PHOTO;
            case WP_CONTENT_TYPE_V1_TYPE_VIDEO: return VIDEO;
            case WP_CONTENT_TYPE_V1_TYPE_GAME: return GAME;
            default: return NONE;
        }
    }

    uint16_t toDRM(eContentType contentType) {
        switch (contentType) {
            case NONE: return DRM_MODE_CONTENT_TYPE_GRAPHICS;
            case PHOTO: return DRM_MODE_CONTENT_TYPE_PHOTO;
            case VIDEO: return DRM_MODE_CONTENT_TYPE_CINEMA;
            case GAME: return DRM_MODE_CONTENT_TYPE_GAME;
            default: return DRM_MODE_CONTENT_TYPE_NO_DATA;
        }
    }
}