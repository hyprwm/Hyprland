#include "ContentType.hpp"

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
}