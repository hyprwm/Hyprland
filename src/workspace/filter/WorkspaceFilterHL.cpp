#include "WorkspaceFilter.hpp"

#include "../HLWorkspace.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../output/Monitor.hpp"
#include "../../state/MonitorState.hpp"

#include <algorithm>

using namespace Workspace;
using namespace Workspace::Filter;

namespace {
    class CHLWorkspaceFilterDataSource final : public IDataSource {
      public:
        bool monitorMatches(const IAbstractWorkspace& workspace, std::string_view selector) const override {
            const auto MONITOR = State::monitorState()->query().relativeTo(Desktop::focusState()->monitor()).configString(selector).run();
            return MONITOR && workspace.monitor() == dynamicPointerCast<Monitor::IMonitorAddressable>(MONITOR);
        }

        int windowCount(const IAbstractWorkspace& workspace, const SWindowCountOptions& options) const override {
            const auto HL_WORKSPACE = dc<const CHLWorkspace*>(&workspace);
            if (!HL_WORKSPACE)
                return 0;

            const auto PINNED  = options.pinned ? std::optional{true} : std::nullopt;
            const auto VISIBLE = options.visible ? std::optional{true} : std::nullopt;
            return options.groups ? HL_WORKSPACE->getGroups(options.tiled, PINNED, VISIBLE) : HL_WORKSPACE->getWindowCount(options.tiled, PINNED, VISIBLE);
        }

        int fullscreenState(const IAbstractWorkspace& workspace) const override {
            const auto HL_WORKSPACE = dc<const CHLWorkspace*>(&workspace);
            if (!HL_WORKSPACE)
                return -1;

            const auto SELF = HL_WORKSPACE->m_self.lock();
            if (!SELF || !Fullscreen::controller()->hasFullscreen(SELF))
                return -1;

            const auto MODES = Fullscreen::controller()->getFullscreenModes(SELF);
            if (MODES.internal == Fullscreen::FSMODE_MAXIMIZED)
                return 1;
            if (MODES.internal == Fullscreen::FSMODE_FULLSCREEN && MODES.client != Fullscreen::FSMODE_FULLSCREEN)
                return 2;
            if (MODES.internal == Fullscreen::FSMODE_FULLSCREEN)
                return 0;

            return -1;
        }
    };
}

const IDataSource& Workspace::Filter::hlDataSource() {
    static const CHLWorkspaceFilterDataSource DATA_SOURCE;
    return DATA_SOURCE;
}

void CWorkspaceFilter::transform(std::vector<SP<CHLWorkspace>>& workspaces) const {
    std::erase_if(workspaces, [this](const auto& workspace) { return !workspace || !matches(*workspace); });
}
