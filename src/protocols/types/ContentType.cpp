#include "ContentType.hpp"
#include <drm_mode.h>
#include <stdexcept>
#include <format>

namespace NContentType {
    static std::unordered_map<std::string, wpContentTypeV1Type> const table = {
        {"none", WP_CONTENT_TYPE_V1_TYPE_NONE}, {"photo", WP_CONTENT_TYPE_V1_TYPE_PHOTO}, {"video", WP_CONTENT_TYPE_V1_TYPE_VIDEO}, {"game", WP_CONTENT_TYPE_V1_TYPE_GAME}};

    wpContentTypeV1Type fromString(const std::string name) {
        auto it = table.find(name);
        if (it != table.end())
            return it->second;
        else
            throw std::invalid_argument(std::format("Unknown content type {}", name));
    }

    uint16_t toDRM(wpContentTypeV1Type contentType) {
        switch (contentType) {
            case WP_CONTENT_TYPE_V1_TYPE_NONE: return DRM_MODE_CONTENT_TYPE_GRAPHICS;
            case WP_CONTENT_TYPE_V1_TYPE_PHOTO: return DRM_MODE_CONTENT_TYPE_PHOTO;
            case WP_CONTENT_TYPE_V1_TYPE_VIDEO: return DRM_MODE_CONTENT_TYPE_CINEMA;
            case WP_CONTENT_TYPE_V1_TYPE_GAME: return DRM_MODE_CONTENT_TYPE_GAME;
            default: return DRM_MODE_CONTENT_TYPE_NO_DATA;
        }
    }
}