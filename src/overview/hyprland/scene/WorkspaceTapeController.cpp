#include "WorkspaceTapeController.hpp"
#include "OverviewLayout.hpp"
#include "WorkspaceTapeLayout.hpp"
#include "WorkspaceMiniStripLayout.hpp"
#include "WorkspaceTileShadow.hpp"

#include "../../../animation/AnimationManager.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../config/shared/animation/AnimationTree.hpp"
#include "../../../desktop/Workspace.hpp"
#include "../../../event/EventBus.hpp"
#include "../../../managers/eventLoop/EventLoopManager.hpp"
#include "../../../output/Monitor.hpp"
#include "../../../output/MonitorResources.hpp"
#include "../../../render/Renderer.hpp"
#include "../../../render/pass/BorderPassElement.hpp"
#include "../../../render/pass/BoxShadowPassElement.hpp"
#include "../../../render/pass/RectPassElement.hpp"
#include "../../../render/pass/TexPassElement.hpp"
#include "../../../state/WorkspaceState.hpp"

#include <algorithm>
#include <cmath>
#include <ranges>

#include <linux/input-event-codes.h>

using namespace Overview::Hyprland;

static constexpr float MAIN_TILE_GAP         = 0.02F;
static constexpr float MINI_TILE_GAP         = 0.15F;
static constexpr float TILE_ROUNDING         = 20.F;
static constexpr float MINI_TILE_ROUNDING    = 8.F;
static constexpr int   MINI_TILE_BORDER_SIZE = 3;
static constexpr float TILE_SHADOW_RANGE     = 14.F;
static constexpr float TILE_SHADOW_ALPHA     = 0.45F;
static constexpr float PLACEHOLDER_GRAY      = 0.12F;

struct CWorkspaceTapeController::SWorkspaceTile {
    PHLWORKSPACEREF          workspace;
    PHLANIMVAR<Vector2D>     position;
    PHLANIMVAR<Vector2D>     size;
    PHLANIMVAR<Vector2D>     miniPosition;
    PHLANIMVAR<Vector2D>     miniSize;
    PHLANIMVAR<CHyprColor>   miniBorderColor;
    PHLANIMVAR<float>        opacity;
    SP<Render::IFramebuffer> framebuffer;
    SP<Render::IFramebuffer> miniFramebuffer;
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

void CWorkspaceTapeController::start(PHLMONITOR monitor, WP<Monitor::CMonitorResources> resources, const OverviewLayout::SLayout& layout) {
    reset();

    if (!monitor || !resources)
        return;

    m_monitor       = monitor;
    m_resources     = resources;
    m_mainArea      = layout.pixelMain;
    m_miniStripArea = layout.pixelMiniStrip;
    m_started       = true;

    m_listeners.created              = Event::bus()->m_events.workspace.created.listen([this](PHLWORKSPACEREF) {
        if (g_pEventLoopManager)
            g_pEventLoopManager->doLater([this] { reconcile(); });
        else
            reconcile();
    });
    m_listeners.removed              = Event::bus()->m_events.workspace.removed.listen([this](PHLWORKSPACEREF) { reconcile(); });
    m_listeners.renamed              = Event::bus()->m_events.workspace.renamed.listen([this](PHLWORKSPACEREF) { reconcile(); });
    m_listeners.moved                = Event::bus()->m_events.workspace.moveToMonitor.listen([this](PHLWORKSPACE, PHLMONITOR) { reconcile(); });
    m_listeners.active               = Event::bus()->m_events.workspace.active.listen([this](PHLWORKSPACE workspace) {
        if (workspace && workspace->m_monitor == m_monitor)
            reconcile();
    });
    m_listeners.monitorAdded         = Event::bus()->m_events.monitor.added.listen([this](PHLMONITOR) { reconcile(); });
    m_listeners.monitorRemoved       = Event::bus()->m_events.monitor.removed.listen([this](PHLMONITOR) { reconcile(); });
    m_listeners.monitorLayoutChanged = Event::bus()->m_events.monitor.layoutChanged.listen([this] { reconcile(); });
    m_listeners.monitorPreRender     = Event::bus()->m_events.render.preChecks.listen([this](PHLMONITOR monitor) {
        const auto OVERVIEW_MONITOR = m_monitor.lock();
        if (!monitor || monitor == OVERVIEW_MONITOR || !monitor->m_damage.hasChanged())
            return;

        const auto TILES = layoutTiles();
        if (std::ranges::none_of(TILES, [&monitor](const auto* tile) {
                const auto WORKSPACE = tile->workspace.lock();
                return WORKSPACE && WORKSPACE->m_monitor == monitor;
            }))
            return;

        damageMonitor();
    });
    m_listeners.configRefreshed      = Event::bus()->m_events.config.props_refreshed.listen([this](bool) { updateMiniBorderColors(); });

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
        if (tile->miniPosition)
            tile->miniPosition->resetAllCallbacks();
        if (tile->miniSize)
            tile->miniSize->resetAllCallbacks();
        if (tile->miniBorderColor)
            tile->miniBorderColor->resetAllCallbacks();
        if (tile->opacity)
            tile->opacity->resetAllCallbacks();
    }

    m_tiles.clear();
    m_selectedWorkspace.reset();
    m_preferredWorkspace.reset();
    m_pressedMiniWorkspace.reset();
    m_mainArea      = {};
    m_miniStripArea = {};
    m_resources.reset();
    m_monitor.reset();
}

void CWorkspaceTapeController::draw(Render::CRenderingContext& context, Time::steady_tp tp, float overviewProgress, size_t reservedWorkBuffers) {
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
        CBox            mainBox;
        CBox            miniBox;
        float           mainOpacity = 0.F;
        float           miniOpacity = 0.F;
        bool            mainVisible = false;
        bool            miniVisible = false;
        bool            selected    = false;
    };

    std::vector<SDrawTile> drawTiles;
    drawTiles.reserve(m_tiles.size());

    for (const auto& tile : m_tiles) {
        const auto     WORKSPACE = tile->workspace.lock();
        const bool     IS_ACTIVE = WORKSPACE && WORKSPACE == ACTIVE;
        const auto     OPENPOS   = tile->position->value();
        const auto     OPENSIZE  = tile->size->value();

        const Vector2D POS          = IS_ACTIVE ? OPENPOS * PROGRESS : OPENPOS;
        const Vector2D SIZE         = IS_ACTIVE ? FULLSIZE + (OPENSIZE - FULLSIZE) * PROGRESS : OPENSIZE;
        const float    ALPHA        = IS_ACTIVE ? (1.F - PROGRESS) + tile->opacity->value() * PROGRESS : tile->opacity->value() * PROGRESS;
        const CBox     MAIN_BOX     = CBox{POS, SIZE}.round();
        const CBox     MINI_BOX     = CBox{tile->miniPosition->value(), tile->miniSize->value()}.round();
        const float    MINI_ALPHA   = tile->opacity->value() * PROGRESS;
        const bool     MAIN_VISIBLE = ALPHA > 0.F && !MAIN_BOX.intersection(MONITORBOX).empty();
        const bool     MINI_VISIBLE = MINI_ALPHA > 0.F && !MINI_BOX.intersection(m_miniStripArea).empty();

        if (!MAIN_VISIBLE && !MINI_VISIBLE) {
            tile->framebuffer.reset();
            tile->miniFramebuffer.reset();
            continue;
        }

        drawTiles.emplace_back(SDrawTile{
            .tile        = tile.get(),
            .mainBox     = MAIN_BOX,
            .miniBox     = MINI_BOX,
            .mainOpacity = ALPHA,
            .miniOpacity = MINI_ALPHA,
            .mainVisible = MAIN_VISIBLE,
            .miniVisible = MINI_VISIBLE,
            .selected    = WORKSPACE && WORKSPACE == SELECTED,
        });
    }

    std::ranges::stable_sort(drawTiles, [&MONITORBOX](const auto& lhs, const auto& rhs) {
        if (lhs.selected != rhs.selected)
            return lhs.selected;
        if (lhs.mainVisible != rhs.mainVisible)
            return lhs.mainVisible;

        const double MONITOR_CENTER = MONITORBOX.x + MONITORBOX.w / 2.0;
        const auto&  LHS_BOX        = lhs.mainVisible ? lhs.mainBox : lhs.miniBox;
        const auto&  RHS_BOX        = rhs.mainVisible ? rhs.mainBox : rhs.miniBox;
        const double LHS_DISTANCE   = std::abs(LHS_BOX.x + LHS_BOX.w / 2.0 - MONITOR_CENTER);
        const double RHS_DISTANCE   = std::abs(RHS_BOX.x + RHS_BOX.w / 2.0 - MONITOR_CENTER);
        return LHS_DISTANCE < RHS_DISTANCE;
    });
    for (size_t i = 0; i < drawTiles.size(); ++i) {
        auto&      drawTile  = drawTiles.at(i);
        auto&      tile      = *drawTile.tile;
        const auto WORKSPACE = tile.workspace.lock();

        const auto SOURCE_MONITOR   = WORKSPACE ? WORKSPACE->m_monitor.lock() : nullptr;
        const auto BUFFER_SIZE      = SOURCE_MONITOR ? SOURCE_MONITOR->m_transformedSize : Vector2D{};
        const auto MINI_BUFFER_SIZE = drawTile.miniBox.size();
        const bool CAN_RENDER = SOURCE_MONITOR && SOURCE_MONITOR->m_enabled && !SOURCE_MONITOR->isMirror() && SOURCE_MONITOR->resources() && BUFFER_SIZE.x > 0 && BUFFER_SIZE.y > 0;
        if (tile.framebuffer && (!CAN_RENDER || tile.framebuffer->m_size != BUFFER_SIZE))
            tile.framebuffer.reset();
        if (tile.miniFramebuffer && (!CAN_RENDER || tile.miniFramebuffer->m_size != MINI_BUFFER_SIZE))
            tile.miniFramebuffer.reset();
        if (!drawTile.mainVisible)
            tile.framebuffer.reset();

        const auto ACQUIRE_BUFFER = [&] {
            if (SOURCE_MONITOR == MONITOR) {
                if (RESOURCES->availableWorkBufferCount() <= reservedWorkBuffers)
                    return;

                tile.framebuffer = RESOURCES->getUnusedWorkBuffer();
            } else
                tile.framebuffer = RESOURCES->getUnusedWorkBuffer(BUFFER_SIZE);
        };

        if (drawTile.mainVisible && !tile.framebuffer && CAN_RENDER) {
            ACQUIRE_BUFFER();

            for (size_t candidateIndex = drawTiles.size(); !tile.framebuffer && candidateIndex > i + 1; --candidateIndex) {
                auto&      candidate           = *drawTiles.at(candidateIndex - 1).tile;
                const auto CANDIDATE_WORKSPACE = candidate.workspace.lock();
                const auto CANDIDATE_MONITOR   = CANDIDATE_WORKSPACE ? CANDIDATE_WORKSPACE->m_monitor.lock() : nullptr;
                if (!candidate.framebuffer || (CANDIDATE_MONITOR == MONITOR) != (SOURCE_MONITOR == MONITOR))
                    continue;

                candidate.framebuffer.reset();
                ACQUIRE_BUFFER();
            }
        }

        bool rendered = false;
        if (CAN_RENDER && tile.framebuffer)
            rendered = g_pHyprRenderer->renderWorkspaceSceneToBuffer(context, WORKSPACE, tile.framebuffer, tp, WORKSPACE->m_visible && SOURCE_MONITOR == MONITOR);

        if (!rendered)
            tile.framebuffer.reset();
        else
            tile.miniFramebuffer.reset();

        if (!rendered && CAN_RENDER && MINI_BUFFER_SIZE.x > 0 && MINI_BUFFER_SIZE.y > 0) {
            if (!tile.miniFramebuffer)
                tile.miniFramebuffer = g_pHyprRenderer->createFB("overview-mini-workspace");

            if (!tile.miniFramebuffer || !tile.miniFramebuffer->alloc(std::lround(MINI_BUFFER_SIZE.x), std::lround(MINI_BUFFER_SIZE.y)) ||
                !g_pHyprRenderer->renderWorkspaceSceneToBufferScaled(context, WORKSPACE, tile.miniFramebuffer, tp,
                                                                     drawTile.mainVisible && WORKSPACE->m_visible && SOURCE_MONITOR == MONITOR))
                tile.miniFramebuffer.reset();
        }
    }

    std::ranges::stable_sort(drawTiles, [](const auto& lhs, const auto& rhs) { return lhs.selected < rhs.selected; });

    const int ROUNDING = std::lround(TILE_ROUNDING * MONITOR->m_scale * PROGRESS);
    for (const auto& drawTile : drawTiles) {
        if (!drawTile.mainVisible)
            continue;

        const auto& tile = *drawTile.tile;

        const auto  SHADOW = WorkspaceTileShadow::calculate(drawTile.mainBox, MONITOR->m_scale, drawTile.mainOpacity, PROGRESS, TILE_SHADOW_RANGE, TILE_ROUNDING);
        if (SHADOW) {
            CBoxShadowPassElement::SBoxShadowData shadow;
            shadow.box           = SHADOW->outerBox;
            shadow.cutoutBox     = drawTile.mainBox;
            shadow.clipBox       = MONITORBOX;
            shadow.color         = CHyprColor(0.F, 0.F, 0.F, TILE_SHADOW_ALPHA);
            shadow.a             = SHADOW->opacity;
            shadow.round         = SHADOW->rounding;
            shadow.roundingPower = 2.F;
            shadow.range         = SHADOW->range;
            g_pHyprRenderer->addPassElement(context, makeUnique<CBoxShadowPassElement>(shadow));
        }

        const auto FRAMEBUFFER = tile.framebuffer ? tile.framebuffer : tile.miniFramebuffer;
        if (!FRAMEBUFFER) {
            CRectPassElement::SRectData data;
            data.box   = drawTile.mainBox;
            data.color = CHyprColor(PLACEHOLDER_GRAY, PLACEHOLDER_GRAY, PLACEHOLDER_GRAY, drawTile.mainOpacity);
            data.round = ROUNDING;
            g_pHyprRenderer->addPassElement(context, makeUnique<CRectPassElement>(data));
            continue;
        }

        CTexPassElement::SRenderData data;
        data.tex     = FRAMEBUFFER->getTexture();
        data.box     = drawTile.mainBox;
        data.a       = drawTile.mainOpacity;
        data.round   = ROUNDING;
        data.clipBox = MONITORBOX;
        g_pHyprRenderer->addPassElement(context, makeUnique<CTexPassElement>(std::move(data)));
    }

    const int MINI_ROUNDING = std::lround(MINI_TILE_ROUNDING * MONITOR->m_scale);
    for (const auto& drawTile : drawTiles) {
        if (!drawTile.miniVisible)
            continue;

        const auto& tile        = *drawTile.tile;
        const auto  FRAMEBUFFER = tile.framebuffer ? tile.framebuffer : tile.miniFramebuffer;
        if (!FRAMEBUFFER) {
            CRectPassElement::SRectData data;
            data.box     = drawTile.miniBox;
            data.color   = CHyprColor(PLACEHOLDER_GRAY, PLACEHOLDER_GRAY, PLACEHOLDER_GRAY, drawTile.miniOpacity);
            data.round   = MINI_ROUNDING;
            data.clipBox = m_miniStripArea;
            g_pHyprRenderer->addPassElement(context, makeUnique<CRectPassElement>(data));
        } else {
            CTexPassElement::SRenderData data;
            data.tex     = FRAMEBUFFER->getTexture();
            data.box     = drawTile.miniBox;
            data.a       = drawTile.miniOpacity;
            data.round   = MINI_ROUNDING;
            data.clipBox = m_miniStripArea;
            g_pHyprRenderer->addPassElement(context, makeUnique<CTexPassElement>(std::move(data)));
        }

        CBorderPassElement::SBorderData border;
        border.box           = drawTile.miniBox;
        border.grad1         = Config::CGradientValueData{tile.miniBorderColor->value()};
        border.a             = drawTile.miniOpacity;
        border.round         = MINI_ROUNDING;
        border.outerRound    = MINI_ROUNDING;
        border.borderSize    = MINI_TILE_BORDER_SIZE;
        border.roundingPower = 2.F;
        g_pHyprRenderer->addPassElement(context, makeUnique<CBorderPassElement>(border));
    }
}

bool CWorkspaceTapeController::navigateLeft() {
    return navigate(-1);
}

bool CWorkspaceTapeController::navigateRight() {
    return navigate(1);
}

bool CWorkspaceTapeController::selectWorkspace(PHLWORKSPACE workspace) {
    if (!m_started || !workspace || workspace == selectedWorkspace())
        return false;

    const auto TILES = layoutTiles();
    if (std::ranges::none_of(TILES, [&workspace](const auto* tile) { return tile->workspace == workspace; }))
        return false;

    m_selectedWorkspace  = workspace;
    m_preferredWorkspace = workspace;
    updateLayout();
    damageMonitor();
    return true;
}

PHLWORKSPACE CWorkspaceTapeController::selectedWorkspace() const {
    return m_selectedWorkspace.lock();
}

PHLWORKSPACE CWorkspaceTapeController::miniWorkspaceAt(const Vector2D& monitorLocal) const {
    const auto MONITOR = m_monitor.lock();
    if (!m_started || !MONITOR)
        return nullptr;

    const auto PIXEL = monitorLocal * MONITOR->m_scale;
    if (!m_miniStripArea.containsPoint(PIXEL))
        return nullptr;

    for (const auto& tile : m_tiles) {
        if (!tile->inLayout)
            continue;

        if (!CBox{tile->miniPosition->value(), tile->miniSize->value()}.containsPoint(PIXEL))
            continue;

        return tile->workspace.lock();
    }

    return nullptr;
}

bool CWorkspaceTapeController::pointerButton(uint32_t button, bool pressed, const Vector2D& monitorLocal) {
    if (button != BTN_LEFT)
        return false;

    const auto WORKSPACE = miniWorkspaceAt(monitorLocal);
    if (pressed) {
        if (!WORKSPACE)
            return false;

        m_pressedMiniWorkspace = WORKSPACE;
        return true;
    }

    const auto PRESSED = m_pressedMiniWorkspace.lock();
    m_pressedMiniWorkspace.reset();
    if (!PRESSED)
        return false;

    if (WORKSPACE == PRESSED)
        selectWorkspace(PRESSED);
    return true;
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

    return selectWorkspace(TILES.at(TARGET)->workspace.lock());
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

    if (initial) {
        if (const auto MONITOR = m_monitor.lock(); MONITOR)
            m_preferredWorkspace = MONITOR->m_activeWorkspace;
    }

    if (WORKSPACES.empty())
        m_selectedWorkspace.reset();
    else if (std::ranges::contains(WORKSPACES, OLD_SELECTED))
        m_selectedWorkspace = OLD_SELECTED;
    else if (const auto PREFERRED = m_preferredWorkspace.lock(); PREFERRED && std::ranges::contains(WORKSPACES, PREFERRED))
        m_selectedWorkspace = PREFERRED;
    else
        m_selectedWorkspace = WORKSPACES.at(std::min(OLD_INDEX, sc<long>(WORKSPACES.size()) - 1L));

    if (const auto SELECTED = selectedWorkspace(); SELECTED)
        m_preferredWorkspace = SELECTED;

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

    const auto            SELECTED = selectedWorkspace();
    const auto            IT       = std::ranges::find_if(TILES, [&SELECTED](const auto* tile) { return tile->workspace == SELECTED; });
    const auto            INDEX    = IT == TILES.end() ? 0L : std::ranges::distance(TILES.begin(), IT);
    std::vector<Vector2D> sourceSizes;
    sourceSizes.reserve(TILES.size());

    for (const auto* tile : TILES) {
        const auto WORKSPACE = tile->workspace.lock();
        const auto SOURCE    = WORKSPACE ? WORKSPACE->m_monitor.lock() : nullptr;
        sourceSizes.emplace_back(SOURCE ? SOURCE->m_transformedSize : MONITOR->m_transformedSize);
    }

    const auto BOXES      = WorkspaceTapeLayout::calculate(m_mainArea, sourceSizes, INDEX, MAIN_TILE_GAP);
    const auto MINI_BOXES = WorkspaceMiniStripLayout::calculate(m_miniStripArea, sourceSizes, INDEX, MINI_TILE_GAP);

    for (size_t i = 0; i < TILES.size(); ++i) {
        auto& tile = *TILES.at(i);
        tile.position->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewMove"));
        tile.size->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewMove"));
        tile.miniPosition->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewMove"));
        tile.miniSize->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewMove"));

        const auto POSITION      = BOXES.at(i).pos();
        const auto SIZE          = BOXES.at(i).size();
        const auto MINI_POSITION = MINI_BOXES.at(i).pos();
        const auto MINI_SIZE     = MINI_BOXES.at(i).size();
        if (warp || !tile.geometryInitialized) {
            tile.position->setValueAndWarp(POSITION);
            tile.size->setValueAndWarp(SIZE);
            tile.miniPosition->setValueAndWarp(MINI_POSITION);
            tile.miniSize->setValueAndWarp(MINI_SIZE);
            tile.geometryInitialized = true;
        } else {
            *tile.position     = POSITION;
            *tile.size         = SIZE;
            *tile.miniPosition = MINI_POSITION;
            *tile.miniSize     = MINI_SIZE;
        }
    }

    for (const auto& tile : m_tiles) {
        if (tile->inLayout)
            continue;

        const auto WORKSPACE = tile->workspace.lock();
        if (!WORKSPACE || WORKSPACE != MONITOR->m_activeWorkspace)
            continue;

        const auto SOURCE       = WORKSPACE->m_monitor.lock();
        const auto SOURCE_SIZE  = SOURCE ? SOURCE->m_transformedSize : MONITOR->m_transformedSize;
        const auto ACTIVE_BOXES = WorkspaceTapeLayout::calculate(m_mainArea, std::span<const Vector2D>{&SOURCE_SIZE, 1}, 0, MAIN_TILE_GAP);
        if (ACTIVE_BOXES.empty())
            continue;

        const auto POSITION = ACTIVE_BOXES.front().pos();
        const auto SIZE     = ACTIVE_BOXES.front().size();
        if (warp || !tile->geometryInitialized) {
            tile->position->setValueAndWarp(POSITION);
            tile->size->setValueAndWarp(SIZE);
            tile->geometryInitialized = true;
        } else {
            *tile->position = POSITION;
            *tile->size     = SIZE;
        }
    }

    updateMiniBorderColors(warp);
}

void CWorkspaceTapeController::retireTile(SWorkspaceTile& tile) {
    if (tile.opacity->goal() == 0.F)
        return;

    tile.position->setValueAndWarp(tile.position->value());
    tile.size->setValueAndWarp(tile.size->value());
    tile.miniPosition->setValueAndWarp(tile.miniPosition->value());
    tile.miniSize->setValueAndWarp(tile.miniSize->value());
    tile.opacity->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewFade"));
    *tile.opacity = 0.F;
}

void CWorkspaceTapeController::ensureAnimations(SWorkspaceTile& tile) {
    const auto  MOVE                 = Config::animationTree()->getAnimationPropertyConfig("overviewMove");
    const auto  FADE                 = Config::animationTree()->getAnimationPropertyConfig("overviewFade");
    static auto PINACTIVEBORDERCOLOR = CConfigValue<Config::INTEGER>("overview:col.inactive_border");

    Animation::mgr()->createAnimation(Vector2D{}, tile.position, MOVE, AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(Vector2D{}, tile.size, MOVE, AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(Vector2D{}, tile.miniPosition, MOVE, AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(Vector2D{}, tile.miniSize, MOVE, AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(CHyprColor(*PINACTIVEBORDERCOLOR), tile.miniBorderColor, FADE, AVARDAMAGE_NONE);
    Animation::mgr()->createAnimation(0.F, tile.opacity, FADE, AVARDAMAGE_NONE);

    const auto DAMAGE = [this](auto) { damageMonitor(); };
    tile.position->setUpdateCallback(DAMAGE);
    tile.size->setUpdateCallback(DAMAGE);
    tile.miniPosition->setUpdateCallback(DAMAGE);
    tile.miniSize->setUpdateCallback(DAMAGE);
    tile.miniBorderColor->setUpdateCallback([this](auto) { damageMiniStrip(); });
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

void CWorkspaceTapeController::damageMiniStrip() const {
    if (const auto MONITOR = m_monitor.lock(); MONITOR)
        MONITOR->addDamage(m_miniStripArea.copy().expand(MINI_TILE_BORDER_SIZE * MONITOR->m_scale));
}

void CWorkspaceTapeController::updateMiniBorderColors(bool warp) {
    static auto PACTIVEBORDERCOLOR   = CConfigValue<Config::INTEGER>("overview:col.active_border");
    static auto PINACTIVEBORDERCOLOR = CConfigValue<Config::INTEGER>("overview:col.inactive_border");

    const auto  SELECTED = selectedWorkspace();
    for (const auto& tile : m_tiles) {
        const auto WORKSPACE = tile->workspace.lock();
        const auto COLOR     = CHyprColor(tile->inLayout && WORKSPACE && WORKSPACE == SELECTED ? *PACTIVEBORDERCOLOR : *PINACTIVEBORDERCOLOR);

        tile->miniBorderColor->setConfig(Config::animationTree()->getAnimationPropertyConfig("overviewFade"));
        if (warp)
            tile->miniBorderColor->setValueAndWarp(COLOR);
        else
            *tile->miniBorderColor = COLOR;
    }

    damageMiniStrip();
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
        const auto WORKSPACE      = workspaceRef.lock();
        const auto SOURCE_MONITOR = WORKSPACE ? WORKSPACE->m_monitor.lock() : nullptr;
        if (!valid(WORKSPACE) || !SOURCE_MONITOR || !SOURCE_MONITOR->m_enabled || SOURCE_MONITOR->isMirror() || !SOURCE_MONITOR->resources() || WORKSPACE->m_isSpecialWorkspace ||
            !m_filter(WORKSPACE))
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
