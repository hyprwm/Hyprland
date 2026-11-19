#include "Rounding.hpp"
#include "../complex/CornersData.hpp"

#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/VarList2.hpp>
#include <array>
#include <algorithm>
#include <format>

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

std::expected<CCornersData, std::string> Rounding::parse(std::string_view raw, std::optional<int64_t> max) {
    const CVarList2 tokens(raw, 0, 's', true);
    if (tokens.size() < 1 || tokens.size() > 4)
        return std::unexpected("rounding requires one to four integers");

    std::array<int64_t, 4> values = {};
    for (size_t i = 0; i < tokens.size(); ++i) {
        const auto value = parseComponent(tokens[i]);
        if (!value)
            return std::unexpected(value.error());

        if (*value < 0)
            return std::unexpected("rounding must be non-negative");

        if (max && *value > *max)
            return std::unexpected(std::format("rounding must be at most {}", *max));

        values[i] = *value;
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
        return parse(raw, MAX_RADIUS);

    if (tokens.size() != 2)
        return std::unexpected("relative rounding requires one integer delta");

    const auto delta = parseComponent(tokens[1]);
    if (!delta)
        return std::unexpected(delta.error());

    // clamping both sides first keeps the sum from overflowing
    const auto step   = std::clamp<int64_t>(*delta, -MAX_RADIUS, MAX_RADIUS);
    std::array values = {current.m_topLeft, current.m_topRight, current.m_bottomRight, current.m_bottomLeft};
    for (auto& component : values)
        component = std::clamp<int64_t>(std::min(component, MAX_RADIUS) + step, 0, MAX_RADIUS);

    return CCornersData(values[0], values[1], values[2], values[3]);
}
