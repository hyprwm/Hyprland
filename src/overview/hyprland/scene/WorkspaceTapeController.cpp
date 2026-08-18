#include "WorkspaceTapeController.hpp"

#include "../../../animation/AnimationManager.hpp"
#include "../../../config/shared/animation/AnimationTree.hpp"
#include "../../../desktop/Workspace.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../managers/eventLoop/EventLoopManager.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../output/MonitorResources.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../render/pass/RectPassElement.hpp"
#include "../../../render/pass/TexPassElement.hpp"
#include "../../../state/WorkspaceState.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>

using namespace Overview::Hyprland;

static constexpr float TILE_GAP         = 0.02F;
static constexpr float TILE_ROUNDING    = 20.F;
static constexpr float PLACEHOLDER_GRAY = 0.12F;

struct CWorkspaceTapeController::SWorkspaceTile {
    PHLWORKSPACEREF          workspace;
    PHLANIMVAR<Vector2D>     position;
    PHLANIMVAR<Vector2D>     size;
    PHLANIMVAR<float>        opacity;
    SP<Render::IFramebuffer> framebuffer;
    bool                     inLayout            = false;
    bool                     geometryInitialized = false;

    struct {
        CHyprSignalListener destroyed;
        CHyprSignalListener idChanged;
        CHyprSignalListener monitorChanged;
    } listeners;
};

CWorkspaceTapeController::CWorkspaceTapeController() : m_filter([](PHLWORKSPACE) { return true; }) {
    ;
}

CWorkspaceTapeController::~CWorkspaceTapeController() {
    reset();
}

void CWorkspaceTapeController::start(PHLMONITOR monitor, WP<Monitor::CMonitorResources> resources) {
    reset();

    if (!monitor || !resources)
        return;

    m_monitor   = monitor;
    m_resources = resources;
    m_started   = true;

    m_listeners.created = Event::bus()->m_events.workspace.created.listen([this](PHLWORKSPACEREF) {
        if (g_pEventLoopManager)
            g_pEventLoopManager->doLater([this] { reconcile(); });
        else
            reconcile();
    });
    m_listeners.removed = Event::bus()->m_events.workspace.removed.listen([this](PHLWORKSPACEREF) { reconcile(); });
    m_listeners.moved   = Event::bus()->m_events.workspace.moveToMonitor.listen([this](PHLWORKSPACE, PHLMONITOR) { reconcile(); });
    m_listeners.active  = Event::bus()->m_events.workspace.active.listen([this](PHLWORKSPACE workspace) {
        if (workspace && workspace->m_monitor == m_monitor)
            reconcile();
    });

    reconcile(true);
}

void CWorkspaceTapeController::reset() {
    m_started   = false;
    m_listeners = {};

    for (const auto& tile : m_tiles) {
        if (tile->position)
            tile->position->resetAllCallbacks();
        if (tile->size)
            tile->size->resetAllCallbacks();
        if (tile->opacity)
            tile->opacity->resetAllCallbacks();
    }

    m_tiles.clear();
    m_selectedWorkspace.reset();
    m_resources.reset();
    m_monitor.reset();
}

void CWorkspaceTapeController::draw(Time::steady_tp tp, float overviewProgress) {
    const auto MONITOR   = m_monitor.lock();
    const auto RESOURCES = m_resources;
    if (!m_started || !MONITOR || !RESOURCES || !g_pHyprRenderer)
        return;

    pruneRetiredTiles();

    const float    PROGRESS   = std::clamp(overviewProgress, 0.F, 1.F);
    const CBox     MONITORBOX = {{}, MONITOR->m_transformedSize};
    const auto     ACTIVE     = MONITOR->m_activeWorkspace;
    const auto     SELECTED   = selectedWorkspace();
    const Vector2D FULLSIZE   = MONITOR->m_transformedSize;

    struct SDrawTile {
        SWorkspaceTile* tile = nullptr;
        CBox            box;
        float           opacity  = 0.F;
        bool            selected = false;
    };

    std::vector<SDrawTile> drawTiles;
    drawTiles.reserve(m_tiles.size());

    for (const auto& tile : m_tiles) {
        const auto     WORKSPACE = tile->workspace.lock();
        const bool     IS_ACTIVE = WORKSPACE && WORKSPACE == ACTIVE;
        const auto     OPENPOS   = tile->position->value();
        const auto     OPENSIZE  = tile->size->value();

        const Vector2D POS   = IS_ACTIVE ? OPENPOS * PROGRESS : OPENPOS;
        const Vector2D SIZE  = IS_ACTIVE ? FULLSIZE + (OPENSIZE - FULLSIZE) * PROGRESS : OPENSIZE;
        const float    ALPHA = IS_ACTIVE ? (1.F - PROGRESS) + tile->opacity->value() * PROGRESS : tile->opacity->value() * PROGRESS;
        const CBox     BOX{POS, SIZE};

        if (ALPHA <= 0.F || BOX.intersection(MONITORBOX).empty()) {
            tile->framebuffer.reset();
            continue;
        }

        drawTiles.emplace_back(SDrawTile{
            .tile     = tile.get(),
            .box      = BOX,
            .opacity  = ALPHA,
            .selected = WORKSPACE && WORKSPACE == SELECTED,
        });
    }

    std::ranges::stable_sort(drawTiles, [](const auto& lhs, const auto& rhs) { return lhs.selected < rhs.selected; });

    const int ROUNDING = std::lround(TILE_ROUNDING * MONITOR->m_scale * PROGRESS);
    for (auto& drawTile : drawTiles) {
        auto&      tile      = *drawTile.tile;
        const auto WORKSPACE = tile.workspace.lock();

        const bool CAN_RENDER = WORKSPACE && WORKSPACE->m_monitor == MONITOR;
        if (!tile.framebuffer && CAN_RENDER)
            tile.framebuffer = RESOURCES->getUnusedWorkBuffer();

        bool rendered = false;
        if (CAN_RENDER && tile.framebuffer)
            rendered = g_pHyprRenderer->renderWorkspaceSceneToBuffer(WORKSPACE, tile.framebuffer, tp, drawTile.selected);

        if (!rendered && tile.inLayout)
            tile.framebuffer.reset();

        if (!tile.framebuffer) {
            CRectPassElement::SRectData data;
            data.box   = drawTile.box;
            data.color = CHyprColor(PLACEHOLDER_GRAY, PLACEHOLDER_GRAY, PLACEHOLDER_GRAY, drawTile.opacity);
            data.round = ROUNDING;
            g_pHyprRenderer->addPassElement(makeUnique<CRectPassElement>(data));
            continue;
        }

        CTexPassElement::SRenderData data;
        data.tex     = tile.framebuffer->getTexture();
        data.box     = drawTile.box;
        data.a       = drawTile.opacity;
        data.round   = ROUNDING;
        data.clipBox = MONITORBOX;
        g_pHyprRenderer->addPassElement(makeUnique<CTexPassElement>(std::move(data)));
    }
}

bool CWorkspaceTapeController::navigateLeft() {
    return navigate(-1);
}

bool CWorkspaceTapeController::navigateRight() {
    return navigate(1);
}

PHLWORKSPACE CWorkspaceTapeController::selectedWorkspace() const {
    return m_selectedWorkspace.lock();
}

void CWorkspaceTapeController::setFilter(FWorkspaceFilter filter) {
    m_filter = filter ? std::move(filter) : FWorkspaceFilter{[](PHLWORKSPACE) { return true; }};
    reconcile();
}

void CWorkspaceTapeController::refresh() {
    reconcile();
}

bool CWorkspaceTapeController::navigate(int direction) {
    const auto TILES = layoutTiles();
    if (!m_started || TILES.empty() || direction == 0)
        return false;

    const auto SELECTED = selectedWorkspace();
    const auto IT       = std::ranges::find_if(TILES, [&SELECTED](const auto* tile) { return tile->workspace == SELECTED; });
    const auto INDEX    = IT == TILES.end() ? 0L : std::ranges::distance(TILES.begin(), IT);
    const auto TARGET   = std::clamp(INDEX + direction, 0L, sc<long>(TILES.size()) - 1L);
    if (TARGET == INDEX)
        return false;

    m_selectedWorkspace = TILES.at(TARGET)->workspace;
    updateLayout();
    damageMonitor();
    return true;
}

void CWorkspaceTapeController::reconcile(bool initial) {
    if (!m_started)
        return;

    const auto OLD_LAYOUT   = layoutTiles();
    const auto OLD_SELECTED = selectedWorkspace();
    const auto OLD_IT       = std::ranges::find_if(OLD_LAYOUT, [&OLD_SELECTED](const auto* tile) { return tile->workspace == OLD_SELECTED; });
    const auto OLD_INDEX    = OLD_IT == OLD_LAYOUT.end() ? 0L : std::ranges::distance(OLD_LAYOUT.begin(), OLD_IT);
    const auto WORKSPACES   = filteredWorkspaces();

    for (const auto& tile : m_tiles)
        tile->inLayout = false;

    for (const auto& workspace : WORKSPACES) {
        auto* tile = tileFor(workspace);
        if (!tile) {
            auto newTile       = makeUnique<SWorkspaceTile>();
            newTile->workspace = workspace;
            ensureAnimations(*newTile);
            installWorkspaceListeners(*newTile);
            tile = m_tiles.emplace_back(std::move(newTile)).get();
        }

        tile->inLayout = true;
        tile->opacity->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewFade"));
        if (initial)
            tile->opacity->setValueAndWarp(1.F);
        else
            *tile->opacity = 1.F;
    }

    for (const auto& tile : m_tiles) {
        if (!tile->inLayout)
            retireTile(*tile);
    }

    if (WORKSPACES.empty())
        m_selectedWorkspace.reset();
    else if (std::ranges::contains(WORKSPACES, OLD_SELECTED))
        m_selectedWorkspace = OLD_SELECTED;
    else if (const auto MONITOR = m_monitor.lock(); initial && MONITOR && std::ranges::contains(WORKSPACES, MONITOR->m_activeWorkspace))
        m_selectedWorkspace = MONITOR->m_activeWorkspace;
    else
        m_selectedWorkspace = WORKSPACES.at(std::min(OLD_INDEX, sc<long>(WORKSPACES.size()) - 1L));

    if (const auto MONITOR = m_monitor.lock(); MONITOR && MONITOR->m_activeWorkspace && !tileFor(MONITOR->m_activeWorkspace)) {
        auto tile       = makeUnique<SWorkspaceTile>();
        tile->workspace = MONITOR->m_activeWorkspace;
        ensureAnimations(*tile);
        installWorkspaceListeners(*tile);
        tile->opacity->setValueAndWarp(0.F);
        m_tiles.emplace_back(std::move(tile));
    }

    updateLayout(initial);
    damageMonitor();
}

void CWorkspaceTapeController::updateLayout(bool warp) {
    const auto MONITOR = m_monitor.lock();
    const auto TILES   = layoutTiles();
    if (!MONITOR)
        return;

    const auto SELECTED = selectedWorkspace();
    const auto IT       = std::ranges::find_if(TILES, [&SELECTED](const auto* tile) { return tile->workspace == SELECTED; });
    const auto INDEX    = IT == TILES.end() ? 0L : std::ranges::distance(TILES.begin(), IT);
    const auto SIZE     = MONITOR->m_transformedSize * TILE_SCALE;
    const auto BASEPOS  = (MONITOR->m_transformedSize - SIZE) / 2.F;
    const auto STRIDE   = SIZE.x + MONITOR->m_transformedSize.x * TILE_GAP;

    for (size_t i = 0; i < TILES.size(); ++i) {
        auto& tile = *TILES.at(i);
        tile.position->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewMove"));
        tile.size->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewMove"));

        const Vector2D POSITION = BASEPOS + Vector2D{(sc<long>(i) - INDEX) * STRIDE, 0.F};
        if (warp || !tile.geometryInitialized) {
            tile.position->setValueAndWarp(POSITION);
            tile.size->setValueAndWarp(SIZE);
            tile.geometryInitialized = true;
        } else {
            *tile.position = POSITION;
            *tile.size     = SIZE;
        }
    }

    for (const auto& tile : m_tiles) {
        if (tile->inLayout)
            continue;

        const auto WORKSPACE = tile->workspace.lock();
        if (!WORKSPACE || WORKSPACE != MONITOR->m_activeWorkspace)
            continue;

        const Vector2D POSITION = BASEPOS;
        if (warp || !tile->geometryInitialized) {
            tile->position->setValueAndWarp(POSITION);
            tile->size->setValueAndWarp(SIZE);
            tile->geometryInitialized = true;
        } else {
            *tile->position = POSITION;
            *tile->size     = SIZE;
        }
    }
}

void CWorkspaceTapeController::retireTile(SWorkspaceTile& tile) {
    if (tile.opacity->goal() == 0.F)
        return;

    tile.position->setValueAndWarp(tile.position->value());
    tile.size->setValueAndWarp(tile.size->value());
    tile.opacity->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewFade"));
    *tile.opacity = 0.F;
}

void CWorkspaceTapeController::ensureAnimations(SWorkspaceTile& tile) {
    const auto MOVE = Config::animationTree()->getAnimationPropertyConfig("overviewMove");
    const auto FADE = Config::animationTree()->getAnimationPropertyConfig("overviewFade");

    Animation::mgr()->createAnimation(Vector2D{}, tile.position, MOVE, AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(Vector2D{}, tile.size, MOVE, AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(0.F, tile.opacity, FADE, AVARDAMAGE_NONE);

    const auto DAMAGE = [this](auto) { damageMonitor(); };
    tile.position->setUpdateCallback(DAMAGE);
    tile.size->setUpdateCallback(DAMAGE);
    tile.opacity->setUpdateCallback(DAMAGE);
}

void CWorkspaceTapeController::installWorkspaceListeners(SWorkspaceTile& tile) {
    const auto WORKSPACE = tile.workspace.lock();
    if (!WORKSPACE)
        return;

    tile.listeners.destroyed      = WORKSPACE->m_events.destroy.listen([this] { reconcile(); });
    tile.listeners.idChanged      = WORKSPACE->m_events.idChanged.listen([this] { reconcile(); });
    tile.listeners.monitorChanged = WORKSPACE->m_events.monitorChanged.listen([this] { reconcile(); });
}

void CWorkspaceTapeController::damageMonitor() const {
    if (const auto MONITOR = m_monitor.lock(); MONITOR && g_pHyprRenderer)
        g_pHyprRenderer->damageMonitor(MONITOR);
}

CWorkspaceTapeController::SWorkspaceTile* CWorkspaceTapeController::tileFor(PHLWORKSPACE workspace) const {
    if (!workspace)
        return nullptr;

    return tileFor(PHLWORKSPACEREF{workspace});
}

CWorkspaceTapeController::SWorkspaceTile* CWorkspaceTapeController::tileFor(PHLWORKSPACEREF workspace) const {
    const auto IT = std::ranges::find_if(m_tiles, [&workspace](const auto& tile) { return tile->workspace == workspace; });
    return IT == m_tiles.end() ? nullptr : IT->get();
}

std::vector<PHLWORKSPACE> CWorkspaceTapeController::filteredWorkspaces() const {
    std::vector<PHLWORKSPACE> workspaces;
    const auto                MONITOR = m_monitor.lock();
    if (!MONITOR)
        return workspaces;

    for (const auto& workspaceRef : State::workspaceState()->workspaces()) {
        const auto WORKSPACE = workspaceRef.lock();
        if (!valid(WORKSPACE) || WORKSPACE->m_monitor != MONITOR || WORKSPACE->m_isSpecialWorkspace || !m_filter(WORKSPACE))
            continue;

        workspaces.emplace_back(WORKSPACE);
    }

    std::ranges::stable_sort(workspaces, [](const auto& lhs, const auto& rhs) {
        const bool LHS_NUMERIC = lhs->m_id > 0;
        const bool RHS_NUMERIC = rhs->m_id > 0;
        if (LHS_NUMERIC != RHS_NUMERIC)
            return LHS_NUMERIC;
        if (LHS_NUMERIC)
            return lhs->m_id < rhs->m_id;
        return false;
    });

    return workspaces;
}

std::vector<CWorkspaceTapeController::SWorkspaceTile*> CWorkspaceTapeController::layoutTiles() const {
    std::vector<SWorkspaceTile*> tiles;
    tiles.reserve(m_tiles.size());

    for (const auto& tile : m_tiles) {
        if (tile->inLayout)
            tiles.emplace_back(tile.get());
    }

    std::ranges::stable_sort(tiles, [](const auto* lhs, const auto* rhs) {
        const auto LHS = lhs->workspace.lock();
        const auto RHS = rhs->workspace.lock();
        if (!LHS || !RHS)
            return !!LHS;

        const bool LHS_NUMERIC = LHS->m_id > 0;
        const bool RHS_NUMERIC = RHS->m_id > 0;
        if (LHS_NUMERIC != RHS_NUMERIC)
            return LHS_NUMERIC;
        if (LHS_NUMERIC)
            return LHS->m_id < RHS->m_id;
        return false;
    });

    return tiles;
}

void CWorkspaceTapeController::pruneRetiredTiles() {
    const auto MONITOR = m_monitor.lock();
    std::erase_if(m_tiles, [&MONITOR](const auto& tile) {
        if (tile->inLayout || tile->opacity->isBeingAnimated() || tile->opacity->value() > 0.F)
            return false;

        const auto WORKSPACE = tile->workspace.lock();
        return !MONITOR || !WORKSPACE || WORKSPACE != MONITOR->m_activeWorkspace;
    });
}
