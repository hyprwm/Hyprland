#include "overview/Overview.hpp"
#include "hyprland/Overview.hpp"

UP<Overview::IOverview>& Overview::overview() {
    static UP<Overview::IOverview> p = makeUnique<Overview::Hyprland::COverview>();
    return p;
}
