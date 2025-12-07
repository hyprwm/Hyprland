#include "ContentType.hpp"
#include <drm_mode.h>
#include <stdexcept>
#include <format>

namespace NContentType {
    static std::unordered_map<std::string, eContentType> const table = {
        {"none", CONTENT_TYPE_NONE}, {"photo", CONTENT_TYPE_PHOTO}, {"video", CONTENT_TYPE_VIDEO}, {"game", CONTENT_TYPE_GAME}};

    eContentType fromString(const std::string name) {
        auto it = table.find(name);
        if (it != table.end())
            return it->second;
        else
            return CONTENT_TYPE_NONE;
    }

    std::string toString(eContentType type) {
        for (const auto& [k, v] : table) {
            if (v == type)
                return k;
        }
        return "";
    }

    eContentType fromWP(wpContentTypeV1Type contentType) {
        switch (contentType) {
            case WP_CONTENT_TYPE_V1_TYPE_NONE: return CONTENT_TYPE_NONE;
            case WP_CONTENT_TYPE_V1_TYPE_PHOTO: return CONTENT_TYPE_PHOTO;
            case WP_CONTENT_TYPE_V1_TYPE_VIDEO: return CONTENT_TYPE_VIDEO;
            case WP_CONTENT_TYPE_V1_TYPE_GAME: return CONTENT_TYPE_GAME;
            default: return CONTENT_TYPE_NONE;
        }
    }

    uint16_t toDRM(eContentType contentType) {
        switch (contentType) {
            case CONTENT_TYPE_NONE: return DRM_MODE_CONTENT_TYPE_GRAPHICS;
            case CONTENT_TYPE_PHOTO: return DRM_MODE_CONTENT_TYPE_PHOTO;
            case CONTENT_TYPE_VIDEO: return DRM_MODE_CONTENT_TYPE_CINEMA;
            case CONTENT_TYPE_GAME: return DRM_MODE_CONTENT_TYPE_GAME;
            default: return DRM_MODE_CONTENT_TYPE_NO_DATA;
        }
    }
}