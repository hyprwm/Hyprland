// TODO: here for debugging purposes till i know what to do w/ this PR
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <optional>
#include "../helpers/MiscFunctions.hpp"
#include "../helpers/varlist/VarList.hpp"
#include <hyprutils/string/String.hpp>

struct SNoScreenShareMaskOptions {
    std::optional<float> opacity            = 1.f;
    std::optional<bool>  includeDecorations = true;

    bool                 hasCustomOpacity() const {
        return opacity.has_value();
    }

    float resolvedOpacity(float fallback = 1.f) const {
        return std::clamp(opacity.value_or(fallback), 0.f, 1.f);
    }

    std::array<float, 4> maskColor(float fallbackOpacity = 1.f) const {
        const float alpha = resolvedOpacity(fallbackOpacity);
        return {0.f, 0.f, 0.f, alpha};
    }

    bool decorationsEnabled(bool fallback = true) const {
        return includeDecorations.value_or(fallback);
    }

    bool hasDecorationPreference() const {
        return includeDecorations.has_value();
    }

    void reset() {
        opacity            = 1.f;
        includeDecorations = true;
    }
};

inline bool operator==(const SNoScreenShareMaskOptions& lhs, const SNoScreenShareMaskOptions& rhs) {
    return lhs.opacity == rhs.opacity && lhs.includeDecorations == rhs.includeDecorations;
}

inline bool operator!=(const SNoScreenShareMaskOptions& lhs, const SNoScreenShareMaskOptions& rhs) {
    return !(lhs == rhs);
}

namespace Hyprland::NoScreenShare {

    inline void parseNoScreenShareTokens(const CVarList& tokens, bool& requestedUnset, bool& enableSpecified, bool& enable, SNoScreenShareMaskOptions& mask, bool& maskTouched) {
        // TODO: this parsing is intentionally lightweight; once the CLI/config flow is decided we can trim it down drastically.
        for (size_t i = 1; i < tokens.size(); ++i) {
            std::string token = Hyprutils::String::trim(tokens[i]);
            if (token.empty())
                continue;

            if (token.back() == ',')
                token.pop_back();

            if (token == "unset") {
                requestedUnset = true;
                break;
            }

            const size_t delimiter = token.find_first_of(":=");
            if (delimiter == std::string::npos) {
                if (!enableSpecified) {
                    if (const auto parsed = configStringToInt(token); parsed.has_value()) {
                        enable          = *parsed != 0;
                        enableSpecified = true;
                    }
                }
                continue;
            }

            std::string key   = Hyprutils::String::trim(token.substr(0, delimiter));
            std::string value = Hyprutils::String::trim(token.substr(delimiter + 1));
            if (!value.empty() && value.back() == ',')
                value.pop_back();

            if (key == "opacity") {
                if (value == "unset") {
                    mask.opacity.reset();
                } else {
                    char*       end    = nullptr;
                    const float parsed = std::clamp(strtof(value.c_str(), &end), 0.f, 1.f);
                    if (end != value.c_str())
                        mask.opacity = parsed;
                }
                maskTouched = true;
                continue;
            }

            if (key == "decorations") {
                if (value == "unset")
                    mask.includeDecorations.reset();
                else if (const auto parsed = configStringToInt(value); parsed.has_value())
                    mask.includeDecorations = *parsed != 0;
                maskTouched = true;
            }
        }
    }

}
