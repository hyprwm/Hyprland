#include "overview/Overview.hpp"
#include "hyprland/Overview.hpp"

bool Overview::IOverview::shouldRenderWorkspace(PHLWORKSPACE) const {
    return false;
}

UP<Overview::IOverview>& Overview::overview() {
    static UP<Overview::IOverview> p = makeUnique<Overview::Hyprland::COverview>();
    return p;
}
