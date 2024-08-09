#pragma once

#include "ConfigManager.hpp"

inline static const std::vector<SConfigOptionDescription> CONFIG_OPTIONS = {
    SConfigOptionDescription{
        .value       = "general:border_size",
        .description = "size of the border around windows",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{1, 0, 20},
    },
    SConfigOptionDescription{
        .value       = "general:no_border_on_floating",
        .description = "disable borders for floating windows",
        .type        = CONFIG_OPTION_BOOL,
        .data        = SConfigOptionDescription::SBoolData{false},
    },
    SConfigOptionDescription{
        .value       = "general:gaps_in",
        .description = "gaps between windows\n\nsupports css style gaps (top, right, bottom, left -> 5 10 15 20)",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"5"},
    },
    SConfigOptionDescription{
        .value       = "general:gaps_out",
        .description = "gaps between windows and monitor edges\n\nsupports css style gaps (top, right, bottom, left -> 5 10 15 20)",
        .type        = CONFIG_OPTION_STRING_SHORT,
        .data        = SConfigOptionDescription::SStringData{"20"},
    },
    SConfigOptionDescription{
        .value       = "general:gaps_workspaces",
        .description = "gaps between workspaces. Stacks with gaps_out.",
        .type        = CONFIG_OPTION_INT,
        .data        = SConfigOptionDescription::SRangeData{0, 0, 100},
    },
};