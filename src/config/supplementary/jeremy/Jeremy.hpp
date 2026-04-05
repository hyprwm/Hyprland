#pragma once

#include <string>
#include <expected>

namespace Config::Supplementary::Jeremy {
    std::expected<std::string, std::string> getMainConfigPath();
};