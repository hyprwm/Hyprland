#include "CMType.hpp"
#include <optional>
#include <string>
#include <unordered_map>

static std::unordered_map<std::string, NCMType::eCMType> const table = {{"auto", NCMType::CM_AUTO},   {"srgb", NCMType::CM_SRGB}, {"wide", NCMType::CM_WIDE},
                                                                        {"edid", NCMType::CM_EDID},   {"hdr", NCMType::CM_HDR},   {"hdredid", NCMType::CM_HDR_EDID},
                                                                        {"dcip3", NCMType::CM_DCIP3}, {"dp3", NCMType::CM_DP3},   {"adobe", NCMType::CM_ADOBE}};

std::optional<NCMType::eCMType>                                NCMType::fromString(const std::string cmType) {
    auto it = table.find(cmType);
    if (it == table.end())
        return std::nullopt;
    return it->second;
}

std::string NCMType::toString(eCMType cmType) {
    for (const auto& [key, value] : table) {
        if (value == cmType)
            return key;
    }
    return "";
}
