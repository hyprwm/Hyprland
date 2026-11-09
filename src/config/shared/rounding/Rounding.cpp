#include "Rounding.hpp"
#include "../complex/CornersData.hpp"

#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/VarList2.hpp>
#include <array>
#include <algorithm>
#include <format>
#include <limits>

using namespace Config;
using namespace Hyprutils::String;

static std::expected<int64_t, std::string> parseComponent(std::string_view raw) {
    if (raw.starts_with('+') && !raw.starts_with("+-"))
        raw.remove_prefix(1);

    const auto value = strToNumber<int64_t>(raw);
    if (!value)
        return std::unexpected(std::format("invalid rounding integer \"{}\"", raw));
    return *value;
}

std::expected<CCornersData, std::string> Rounding::parse(std::string_view raw, bool clampNegative) {
    const CVarList2 tokens(raw, 0, 's', true);
    if (tokens.size() < 1 || tokens.size() > 4)
        return std::unexpected("rounding requires one to four integers");

    std::array<int64_t, 4> values = {};
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto value = parseComponent(tokens[i]);
        if (!value)
            return std::unexpected(value.error());
        if (*value < 0 && !clampNegative)
            return std::unexpected("rounding must be non-negative");
        values[i] = std::max<int64_t>(*value, 0);
    }

    switch (tokens.size()) {
        case 1: return CCornersData(values[0]);
        case 2: return CCornersData(values[0], values[1], values[0], values[1]);
        case 3: return CCornersData(values[0], values[1], values[2], values[1]);
        default: return CCornersData(values[0], values[1], values[2], values[3]);
    }
}

std::expected<CCornersData, std::string> Rounding::parseProp(std::string_view raw, const CCornersData& current) {
    const CVarList2 tokens(raw, 0, 's', true);
    if (tokens.size() == 0 || tokens[0] != "relative")
        return parse(raw, true);
    if (tokens.size() != 2)
        return std::unexpected("relative rounding requires one integer delta");

    const auto delta = parseComponent(tokens[1]);
    if (!delta)
        return std::unexpected(delta.error());

    std::array values = {current.m_topLeft, current.m_topRight, current.m_bottomRight, current.m_bottomLeft};
    for (auto& component : values) {
        if ((*delta > 0 && component > std::numeric_limits<int64_t>::max() - *delta) || (*delta < 0 && component < std::numeric_limits<int64_t>::min() - *delta))
            return std::unexpected("relative rounding overflows an integer");
        component = std::max<int64_t>(component + *delta, 0);
    }
    return CCornersData(values[0], values[1], values[2], values[3]);
}
