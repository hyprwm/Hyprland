#include "overview/Overview.hpp"
#include "hyprland/Overview.hpp"

void Overview::openWithQuery(IOverview* overview, PHLMONITOR monitor, const std::string& query) {
    if (!overview)
        return;

    if (const auto QUERY_OPENABLE = dynamic_cast<IOverviewQueryOpenable*>(overview)) {
        QUERY_OPENABLE->open(monitor, query);
        return;
    }

    if (typeid(*overview) == typeid(Hyprland::COverview)) {
        sc<Hyprland::COverview*>(overview)->open(monitor, query);
        return;
    }

    overview->open(monitor);
}

Overview::SOverviewState Overview::state(const IOverview* overview) {
    if (!overview)
        return {};

    if (const auto STATE_PROVIDER = dynamic_cast<const IOverviewStateProvider*>(overview))
        return STATE_PROVIDER->state();

    if (typeid(*overview) == typeid(Hyprland::COverview))
        return sc<const Hyprland::COverview*>(overview)->state();

    return {.open = overview->isOpen()};
}

static std::optional<bool> moveOverview(Overview::IOverview* overview, bool left) {
    if (!overview)
        return std::nullopt;

    if (const auto NAVIGABLE = dynamic_cast<Overview::IOverviewNavigable*>(overview))
        return left ? NAVIGABLE->moveLeft() : NAVIGABLE->moveRight();

    if (typeid(*overview) == typeid(Overview::Hyprland::COverview)) {
        const auto BUILTIN = sc<Overview::Hyprland::COverview*>(overview);
        return left ? BUILTIN->moveLeft() : BUILTIN->moveRight();
    }

    return std::nullopt;
}

std::optional<bool> Overview::moveLeft(IOverview* overview) {
    return moveOverview(overview, true);
}

std::optional<bool> Overview::moveRight(IOverview* overview) {
    return moveOverview(overview, false);
}

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
