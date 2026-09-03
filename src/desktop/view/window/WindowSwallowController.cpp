#include "WindowSwallowController.hpp"

#include "Window.hpp"
#include "WindowGroupMembership.hpp"
#include "WindowMetadata.hpp"
#include "../Group.hpp"
#include "../../history/WindowHistoryTracker.hpp"
#include "../../state/FocusState.hpp"
#include "../../state/WindowState.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../helpers/MiscFunctions.hpp"
#include "../../../layout/LayoutManager.hpp"
#include "../../../layout/space/Space.hpp"
#include "../../../layout/target/WindowTarget.hpp"
#include "../../../managers/fullscreen/FullscreenTypes.hpp"

#include <algorithm>
#include <ranges>
#include <re2/re2.h>

using namespace Desktop;
using namespace Desktop::View;

struct CWindowSwallowController::SRelation {
    enum eState : uint8_t {
        STATE_PENDING,
        STATE_ACTIVE,
        STATE_REVEALED,
    };

    enum ePlacement : uint8_t {
        PLACEMENT_STANDALONE,
        PLACEMENT_GROUP_SLOT,
    };

    PHLWINDOWREF swallower;
    PHLWINDOWREF swallowee;
    WP<CGroup>   group;
    eState       state     = STATE_PENDING;
    ePlacement   placement = PLACEMENT_STANDALONE;
};

CWindowSwallowController::CWindowSwallowController(CWindow& window) : m_window(window) {
    ;
}

PHLWINDOW CWindowSwallowController::findCandidate() const {
    static auto PSWALLOWREGEX   = CConfigValue<std::string>("misc:swallow_regex");
    static auto PSWALLOWEXREGEX = CConfigValue<std::string>("misc:swallow_exception_regex");
    static auto PSWALLOW        = CConfigValue<Config::INTEGER>("misc:enable_swallow");

    if (!*PSWALLOW || std::string{*PSWALLOWREGEX} == STRVAL_EMPTY || (*PSWALLOWREGEX).empty())
        return nullptr;

    std::vector<PHLWINDOW> candidates;
    pid_t                  currentPid = m_window.backend().pid();

    for (size_t i = 0; i < 25; ++i) {
        currentPid = getPPIDof(currentPid);

        if (!currentPid)
            break;

        for (const auto& w : Desktop::windowState()->windows()) {
            if (!w->mapped() || !w->acceptsInput())
                continue;

            if (w->backend().pid() == currentPid)
                candidates.push_back(w);
        }
    }

    if (!(*PSWALLOWREGEX).empty())
        std::erase_if(candidates, [&](const auto& other) { return !RE2::FullMatch(other->metadata().appID(), *PSWALLOWREGEX); });

    if (candidates.empty())
        return nullptr;

    if (!(*PSWALLOWEXREGEX).empty())
        std::erase_if(candidates, [&](const auto& other) { return RE2::FullMatch(other->metadata().title(), *PSWALLOWEXREGEX); });

    if (candidates.empty())
        return nullptr;

    if (candidates.size() == 1)
        return candidates.front();

    for (const auto& w : Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {
        if (w && std::ranges::find(candidates, w.lock()) != candidates.end())
            return w.lock();
    }

    return candidates.front();
}

void CWindowSwallowController::reserveCandidate() {
    if (m_outgoingRelation)
        return;

    const auto SWALLOWEE = findCandidate();
    if (!SWALLOWEE || SWALLOWEE->swallowing().m_incomingRelation)
        return;

    const auto GROUP = SWALLOWEE->grouping().group();
    if (GROUP && (GROUP->denied() || !m_window.grouping().canBeGroupedInto(GROUP)))
        return;

    const auto RELATION                        = makeShared<SRelation>();
    RELATION->swallower                        = m_window.m_self;
    RELATION->swallowee                        = SWALLOWEE;
    RELATION->group                            = GROUP;
    RELATION->placement                        = GROUP ? SRelation::PLACEMENT_GROUP_SLOT : SRelation::PLACEMENT_STANDALONE;
    m_outgoingRelation                         = RELATION;
    SWALLOWEE->swallowing().m_incomingRelation = RELATION;
}

bool CWindowSwallowController::activateStandalone(const SP<SRelation>& relation) {
    const auto SWALLOWER = relation->swallower.lock();
    const auto SWALLOWEE = relation->swallowee.lock();
    if (!SWALLOWER || !SWALLOWEE || SWALLOWEE->grouping().group())
        return false;

    if (!SWALLOWER->windowTarget()->space())
        g_layoutManager->newTarget(SWALLOWER->windowTarget(), SWALLOWER->m_workspace->space());

    if (SWALLOWEE->windowTarget()->space())
        g_layoutManager->removeTarget(SWALLOWEE->windowTarget());

    SWALLOWEE->setHidden(true);
    relation->state = SRelation::STATE_ACTIVE;
    return true;
}

bool CWindowSwallowController::activateGroupSlot(const SP<SRelation>& relation) {
    const auto SWALLOWER = relation->swallower.lock();
    const auto SWALLOWEE = relation->swallowee.lock();
    const auto GROUP     = relation->group.lock();
    if (!SWALLOWER || !SWALLOWEE || !GROUP || SWALLOWEE->grouping().group() != GROUP || !GROUP->has(SWALLOWEE) || SWALLOWER->grouping().group() || GROUP->denied() ||
        !SWALLOWER->grouping().canBeGroupedInto(GROUP))
        return false;

    GROUP->replaceMember(SWALLOWEE, SWALLOWER);
    if (SWALLOWER->grouping().group() != GROUP || !GROUP->has(SWALLOWER))
        return false;

    SWALLOWER->setHidden(false);
    SWALLOWEE->setHidden(true);
    relation->state = SRelation::STATE_ACTIVE;
    return true;
}

bool CWindowSwallowController::activate() {
    const auto RELATION = m_outgoingRelation;
    if (!RELATION || RELATION->swallower != m_window.m_self || RELATION->state != SRelation::STATE_PENDING)
        return false;

    const bool ACTIVATED = RELATION->placement == SRelation::PLACEMENT_GROUP_SLOT ? activateGroupSlot(RELATION) : activateStandalone(RELATION);
    if (!ACTIVATED)
        clear(RELATION);

    return ACTIVATED;
}

bool CWindowSwallowController::restoreStandalone(const SP<SRelation>& relation) {
    const auto SWALLOWER = relation->swallower.lock();
    const auto SWALLOWEE = relation->swallowee.lock();
    if (!SWALLOWER || !SWALLOWEE)
        return false;

    relation->placement = SRelation::PLACEMENT_STANDALONE;
    relation->group.reset();
    SWALLOWER->setHidden(false);
    if (!SWALLOWER->grouping().group() && !SWALLOWER->windowTarget()->space() && SWALLOWER->mapped() && SWALLOWER->m_workspace)
        g_layoutManager->newTarget(SWALLOWER->windowTarget(), SWALLOWER->m_workspace->space());

    if (relation->state == SRelation::STATE_ACTIVE) {
        if (SWALLOWEE->grouping().group()) {
            SWALLOWEE->setHidden(false);
            clear(relation);
            return false;
        }

        SWALLOWEE->setHidden(true);
        return true;
    }

    SWALLOWEE->setHidden(false);
    if (!SWALLOWEE->grouping().group() && !SWALLOWEE->windowTarget()->space() && SWALLOWEE->mapped())
        g_layoutManager->newTarget(SWALLOWEE->windowTarget(), SWALLOWER->m_workspace->space());

    return true;
}

bool CWindowSwallowController::toggleStandalone(const SP<SRelation>& relation) {
    const auto SWALLOWER = relation->swallower.lock();
    const auto SWALLOWEE = relation->swallowee.lock();
    if (!SWALLOWER || !SWALLOWEE)
        return false;

    if (relation->state == SRelation::STATE_ACTIVE) {
        if (SWALLOWEE->grouping().group())
            return false;

        SWALLOWEE->moveToWorkspace(SWALLOWER->m_workspace);
        SWALLOWEE->m_monitor = SWALLOWER->m_monitor;
        SWALLOWEE->setHidden(false);
        if (!SWALLOWEE->windowTarget()->space())
            g_layoutManager->newTarget(SWALLOWEE->windowTarget(), SWALLOWER->m_workspace->space());
        relation->state = SRelation::STATE_REVEALED;
        return true;
    }

    if (SWALLOWEE->grouping().group())
        return false;

    const bool WAS_FOCUSED = Desktop::focusState()->window() == SWALLOWEE;

    if (SWALLOWEE->windowTarget()->space())
        g_layoutManager->removeTarget(SWALLOWEE->windowTarget());
    SWALLOWEE->setHidden(true);
    relation->state = SRelation::STATE_ACTIVE;
    if (WAS_FOCUSED)
        Desktop::focusState()->fullWindowFocus(SWALLOWER, FOCUS_REASON_KEYBIND);
    return true;
}

bool CWindowSwallowController::toggleGroupSlot(const SP<SRelation>& relation) {
    const auto SWALLOWER = relation->swallower.lock();
    const auto SWALLOWEE = relation->swallowee.lock();
    const auto GROUP     = relation->group.lock();
    const auto CURRENT   = relation->state == SRelation::STATE_ACTIVE ? SWALLOWER : SWALLOWEE;
    const auto NEXT      = relation->state == SRelation::STATE_ACTIVE ? SWALLOWEE : SWALLOWER;

    if (!SWALLOWER || !SWALLOWEE || !GROUP || !CURRENT || !NEXT || CURRENT->grouping().group() != GROUP || !GROUP->has(CURRENT) || NEXT->grouping().group())
        return false;

    const bool WAS_FOCUSED = Desktop::focusState()->window() == CURRENT;

    GROUP->replaceMember(CURRENT, NEXT);
    if (NEXT->grouping().group() != GROUP || !GROUP->has(NEXT))
        return false;

    CURRENT->setHidden(true);
    NEXT->setHidden(false);
    relation->state = relation->state == SRelation::STATE_ACTIVE ? SRelation::STATE_REVEALED : SRelation::STATE_ACTIVE;
    if (WAS_FOCUSED)
        Desktop::focusState()->fullWindowFocus(NEXT, FOCUS_REASON_KEYBIND);
    return true;
}

void CWindowSwallowController::toggle() {
    const auto RELATION = m_outgoingRelation ? m_outgoingRelation : m_incomingRelation;
    if (!RELATION || RELATION->state == SRelation::STATE_PENDING)
        return;

    if (RELATION->placement == SRelation::PLACEMENT_GROUP_SLOT && !toggleGroupSlot(RELATION)) {
        if (!restoreStandalone(RELATION))
            return;
        if (!toggleStandalone(RELATION))
            clear(RELATION);
        return;
    }

    if (RELATION->placement == SRelation::PLACEMENT_STANDALONE && !toggleStandalone(RELATION))
        clear(RELATION);
}

void CWindowSwallowController::moveToWorkspace(PHLWORKSPACE workspace) {
    const auto RELATION = m_outgoingRelation;
    if (!RELATION || RELATION->swallower != m_window.m_self || RELATION->state != SRelation::STATE_ACTIVE)
        return;

    const auto SWALLOWEE = RELATION->swallowee.lock();
    if (!SWALLOWEE || SWALLOWEE->m_workspace == workspace)
        return;

    SWALLOWEE->moveToWorkspace(workspace);
    SWALLOWEE->m_monitor = m_window.m_monitor;
}

CWindowSwallowController::SUnmapResult CWindowSwallowController::restoreAfterSwallower(const SP<SRelation>& relation, std::optional<Fullscreen::eFullscreenMode> internalMode,
                                                                                       bool layoutManaged) {
    const auto SWALLOWER = relation->swallower.lock();
    const auto SWALLOWEE = relation->swallowee.lock();
    if (!SWALLOWEE || !SWALLOWEE->mapped())
        return {};

    if (relation->placement == SRelation::PLACEMENT_GROUP_SLOT) {
        const auto GROUP = relation->group.lock();
        if (relation->state == SRelation::STATE_REVEALED && GROUP && SWALLOWEE->grouping().group() == GROUP && GROUP->has(SWALLOWEE)) {
            SWALLOWEE->setHidden(false);
            return {.restoredWindow = SWALLOWEE};
        }

        if (relation->state == SRelation::STATE_ACTIVE && SWALLOWER && GROUP && SWALLOWER->grouping().group() == GROUP && GROUP->has(SWALLOWER) && !SWALLOWEE->grouping().group()) {
            GROUP->replaceMember(SWALLOWER, SWALLOWEE, internalMode, layoutManaged);
            if (SWALLOWEE->grouping().group() == GROUP && GROUP->has(SWALLOWEE)) {
                SWALLOWEE->setHidden(false);
                return {
                    .restoredWindow                = SWALLOWEE,
                    .transferredInternalFullscreen = internalMode.value_or(Fullscreen::FSMODE_NONE) != Fullscreen::FSMODE_NONE,
                };
            }
        }

        restoreStandalone(relation);
    }

    SWALLOWEE->setHidden(false);
    if (!SWALLOWEE->grouping().group() && !SWALLOWEE->windowTarget()->space()) {
        const auto WORKSPACE = SWALLOWER && SWALLOWER->m_workspace ? SWALLOWER->m_workspace : SWALLOWEE->m_workspace;
        if (WORKSPACE)
            g_layoutManager->newTarget(SWALLOWEE->windowTarget(), WORKSPACE->space());
    }

    return {.restoredWindow = SWALLOWEE};
}

CWindowSwallowController::SUnmapResult CWindowSwallowController::restoreAfterSwallowee(const SP<SRelation>& relation, std::optional<Fullscreen::eFullscreenMode> internalMode,
                                                                                       bool layoutManaged) {
    if (relation->placement != SRelation::PLACEMENT_GROUP_SLOT || relation->state != SRelation::STATE_REVEALED)
        return {};

    const auto SWALLOWER = relation->swallower.lock();
    const auto SWALLOWEE = relation->swallowee.lock();
    const auto GROUP     = relation->group.lock();
    if (SWALLOWER && SWALLOWEE && GROUP && SWALLOWEE->grouping().group() == GROUP && GROUP->has(SWALLOWEE) && !SWALLOWER->grouping().group()) {
        GROUP->replaceMember(SWALLOWEE, SWALLOWER, internalMode, layoutManaged);
        if (SWALLOWER->grouping().group() == GROUP && GROUP->has(SWALLOWER)) {
            SWALLOWER->setHidden(false);
            return {
                .restoredWindow                = SWALLOWER,
                .transferredInternalFullscreen = internalMode.value_or(Fullscreen::FSMODE_NONE) != Fullscreen::FSMODE_NONE,
            };
        }
    }

    restoreStandalone(relation);
    return {.restoredWindow = SWALLOWER};
}

void CWindowSwallowController::clear(const SP<SRelation>& relation) {
    if (!relation)
        return;

    const auto SWALLOWER = relation->swallower.lock();
    const auto SWALLOWEE = relation->swallowee.lock();
    if (SWALLOWER && SWALLOWER->swallowing().m_outgoingRelation == relation)
        SWALLOWER->swallowing().m_outgoingRelation.reset();
    if (SWALLOWEE && SWALLOWEE->swallowing().m_incomingRelation == relation)
        SWALLOWEE->swallowing().m_incomingRelation.reset();
    if (m_outgoingRelation == relation)
        m_outgoingRelation.reset();
    if (m_incomingRelation == relation)
        m_incomingRelation.reset();
}

CWindowSwallowController::SUnmapResult CWindowSwallowController::onUnmap(std::optional<Fullscreen::eFullscreenMode> internalMode, bool layoutManaged) {
    const auto OUTGOING = m_outgoingRelation;
    const auto INCOMING = m_incomingRelation;
    if (!OUTGOING && !INCOMING)
        return {};

    SUnmapResult result;

    if (INCOMING) {
        if (INCOMING->state != SRelation::STATE_PENDING)
            result = restoreAfterSwallowee(INCOMING, internalMode, layoutManaged);
        clear(INCOMING);
    }

    if (OUTGOING && OUTGOING != INCOMING) {
        const bool INCOMING_RESTORED = !!result.restoredWindow;
        if (OUTGOING->state != SRelation::STATE_PENDING) {
            const auto OUTGOING_RESULT = restoreAfterSwallower(OUTGOING, INCOMING_RESTORED ? std::nullopt : internalMode, INCOMING_RESTORED ? false : layoutManaged);
            if (!result.restoredWindow)
                result = OUTGOING_RESULT;
        }
        clear(OUTGOING);
    }

    return result;
}

void CWindowSwallowController::onDestroy() {
    onUnmap();
}

PHLWINDOW CWindowSwallowController::swallowee() const {
    if (!m_outgoingRelation || m_outgoingRelation->swallower != m_window.m_self)
        return nullptr;

    return m_outgoingRelation->swallowee.lock();
}
