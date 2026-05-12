#pragma once

#include <expected>
#include <string>

namespace Hyprpaper {
    std::expected<void, std::string> makeHyprpaperRequest(const std::string_view& rq);
};