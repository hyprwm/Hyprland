#include "overview/Overview.hpp"
#include "hyprland/Overview.hpp"

bool Overview::IOverview::shouldRenderWorkspace(PHLWORKSPACE) const {
    return false;
}

PHLWORKSPACE Overview::IOverview::inputWorkspace() const {
    return nullptr;
}

UP<Overview::IOverview>& Overview::overview() {
    static UP<Overview::IOverview> p = makeUnique<Overview::Hyprland::COverview>();
    return p;
}
