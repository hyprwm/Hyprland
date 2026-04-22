#pragma once

#include <string>
#include <expected>
#include <cstdint>

namespace Config::Supplementary::Jeremy {
    enum eConfigType : uint8_t {
        CONFIG_TYPE_REGULAR = 0,
        CONFIG_TYPE_EXPLICIT,
        CONFIG_TYPE_SPECIAL,
        CONFIG_TYPE_ERR,
    };

    struct SConfigStateReply {
        std::string path;
        eConfigType type = CONFIG_TYPE_REGULAR;
    };

    std::expected<SConfigStateReply, std::string> getMainConfigPath();
};