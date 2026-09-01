#include "OverviewLayout.hpp"

using namespace Overview::Hyprland;
using Hyprutils::Math::CBox;
using Hyprutils::Math::Vector2D;

static constexpr float  SEARCH_TOP_SCALE       = 0.01F;
static constexpr float  SEARCH_WIDTH_SCALE     = 0.2F;
static constexpr float  SEARCH_HEIGHT_SCALE    = 0.03F;
static constexpr float  SEARCH_MINI_GAP_SCALE  = 0.015F;
static constexpr float  MINI_HEIGHT_SCALE      = 0.11F;
static constexpr float  MINI_SIDE_MARGIN_SCALE = 0.025F;
static constexpr float  MINI_MAIN_GAP_SCALE    = 0.025F;
static constexpr float  MAIN_SIDE_MARGIN_SCALE = 0.05F;
static constexpr float  MAIN_BOTTOM_SCALE      = 0.025F;

OverviewLayout::SLayout OverviewLayout::calculate(const Vector2D& logicalMonitorSize, float scale) {
    if (logicalMonitorSize.x <= 0.F || logicalMonitorSize.y <= 0.F || scale <= 0.F)
        return {};

    const float SEARCH_WIDTH  = logicalMonitorSize.x * SEARCH_WIDTH_SCALE;
    const float SEARCH_HEIGHT = logicalMonitorSize.y * SEARCH_HEIGHT_SCALE;
    const float SEARCH_Y      = logicalMonitorSize.y * SEARCH_TOP_SCALE;
    const CBox  SEARCH        = {{(logicalMonitorSize.x - SEARCH_WIDTH) / 2.F, SEARCH_Y}, {SEARCH_WIDTH, SEARCH_HEIGHT}};

    const float MINI_Y      = SEARCH_Y + SEARCH_HEIGHT + logicalMonitorSize.y * SEARCH_MINI_GAP_SCALE;
    const float MINI_MARGIN = logicalMonitorSize.x * MINI_SIDE_MARGIN_SCALE;
    const CBox  MINI_STRIP  = {{MINI_MARGIN, MINI_Y}, {logicalMonitorSize.x - 2.F * MINI_MARGIN, logicalMonitorSize.y * MINI_HEIGHT_SCALE}};

    const float MAIN_Y      = MINI_STRIP.y + MINI_STRIP.h + logicalMonitorSize.y * MINI_MAIN_GAP_SCALE;
    const float MAIN_MARGIN = logicalMonitorSize.x * MAIN_SIDE_MARGIN_SCALE;
    const CBox  MAIN        = {{MAIN_MARGIN, MAIN_Y}, {logicalMonitorSize.x - 2.F * MAIN_MARGIN, logicalMonitorSize.y - MAIN_Y - logicalMonitorSize.y * MAIN_BOTTOM_SCALE}};

    if (SEARCH.empty() || MINI_STRIP.empty() || MAIN.empty())
        return {};

    return {
        .logicalSearch    = SEARCH,
        .logicalMiniStrip = MINI_STRIP,
        .logicalMain      = MAIN,
        .pixelSearch      = SEARCH.copy().scale(scale).round(),
        .pixelMiniStrip   = MINI_STRIP.copy().scale(scale).round(),
        .pixelMain        = MAIN.copy().scale(scale).round(),
    };
}
