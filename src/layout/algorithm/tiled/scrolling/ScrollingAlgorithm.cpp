#include "ScrollingAlgorithm.hpp"
#include "ScrollTapeController.hpp"

#include "../../Algorithm.hpp"
#include "../../../space/Space.hpp"
#include "../../../LayoutManager.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../config/ConfigValue.hpp"
#include "../../../../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../../../../render/Renderer.hpp"
#include "../../../../managers/input/InputManager.hpp"
#include "../../../../managers/animation/DesktopAnimationManager.hpp"
#include "../../../../event/EventBus.hpp"

#include <hyprutils/string/VarList2.hpp>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/ConstVarList.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

using namespace Hyprutils::String;
using namespace Hyprutils::Utils;
using namespace Layout;
using namespace Layout::Tiled;

constexpr float MIN_COLUMN_WIDTH = 0.05F;
constexpr float MAX_COLUMN_WIDTH = 1.F;
constexpr float MIN_ROW_HEIGHT   = 0.1F;
constexpr float MAX_ROW_HEIGHT   = 1.F;

//
float SColumnData::getColumnWidth() const {
    if (!scrollingData || !scrollingData->controller)
        return 1.F;

    auto sd = scrollingData.lock();
    if (!sd)
        return 1.F;

    int64_t idx = sd->idx(self.lock());
    if (idx < 0 || (size_t)idx >= sd->controller->stripCount())
        return 1.F;

    return sd->controller->getStrip(idx).size;
}

void SColumnData::setColumnWidth(float width) {
    if (!scrollingData || !scrollingData->controller)
        return;

    auto sd = scrollingData.lock();
    if (!sd)
        return;

    int64_t idx = sd->idx(self.lock());
    if (idx < 0 || (size_t)idx >= sd->controller->stripCount())
        return;

    sd->controller->getStrip(idx).size = width;
}

float SColumnData::getTargetSize(size_t idx) const {
    if (!scrollingData || !scrollingData->controller)
        return 1.F;

    auto sd = scrollingData.lock();
    if (!sd)
        return 1.F;

    int64_t colIdx = sd->idx(self.lock());
    if (colIdx < 0 || (size_t)colIdx >= sd->controller->stripCount())
        return 1.F;

    const auto& strip = sd->controller->getStrip(colIdx);
    if (idx >= strip.targetSizes.size())
        return 1.F;

    return strip.targetSizes[idx];
}

void SColumnData::setTargetSize(size_t idx, float size) {
    if (!scrollingData || !scrollingData->controller)
        return;

    auto sd = scrollingData.lock();
    if (!sd)
        return;

    int64_t colIdx = sd->idx(self.lock());
    if (colIdx < 0 || (size_t)colIdx >= sd->controller->stripCount())
        return;

    auto& strip = sd->controller->getStrip(colIdx);
    if (idx >= strip.targetSizes.size())
        strip.targetSizes.resize(idx + 1, 1.F);

    strip.targetSizes[idx] = size;
}

float SColumnData::getTargetSize(SP<SScrollingTargetData> target) const {
    for (size_t i = 0; i < targetDatas.size(); ++i) {
        if (targetDatas[i] == target)
            return getTargetSize(i);
    }
    return 1.F;
}

void SColumnData::setTargetSize(SP<SScrollingTargetData> target, float size) {
    for (size_t i = 0; i < targetDatas.size(); ++i) {
        if (targetDatas[i] == target) {
            setTargetSize(i, size);
            return;
        }
    }
}

void SColumnData::add(SP<ITarget> t) {
    const float newSize = 1.F / (float)(targetDatas.size() + 1);

    for (size_t i = 0; i < targetDatas.size(); ++i) {
        setTargetSize(i, getTargetSize(i) * (float)targetDatas.size() / (float)(targetDatas.size() + 1));
    }

    targetDatas.emplace_back(makeShared<SScrollingTargetData>(t, self.lock()));
    setTargetSize(targetDatas.size() - 1, newSize);
}

void SColumnData::add(SP<ITarget> t, int after) {
    const float newSize = 1.F / (float)(targetDatas.size() + 1);

    for (size_t i = 0; i < targetDatas.size(); ++i) {
        setTargetSize(i, getTargetSize(i) * (float)targetDatas.size() / (float)(targetDatas.size() + 1));
    }

    targetDatas.insert(targetDatas.begin() + after + 1, makeShared<SScrollingTargetData>(t, self.lock()));

    // Sync sizes - need to insert at the right position
    if (scrollingData) {
        auto sd = scrollingData.lock();
        if (sd && sd->controller) {
            int64_t colIdx = sd->idx(self.lock());
            if (colIdx >= 0 && (size_t)colIdx < sd->controller->stripCount()) {
                auto& strip = sd->controller->getStrip(colIdx);
                strip.targetSizes.insert(strip.targetSizes.begin() + after + 1, newSize);
            }
        }
    }
}

void SColumnData::add(SP<SScrollingTargetData> w) {
    const float newSize = 1.F / (float)(targetDatas.size() + 1);

    for (size_t i = 0; i < targetDatas.size(); ++i) {
        setTargetSize(i, getTargetSize(i) * (float)targetDatas.size() / (float)(targetDatas.size() + 1));
    }

    targetDatas.emplace_back(w);
    w->column = self;
    setTargetSize(targetDatas.size() - 1, newSize);
}

void SColumnData::add(SP<SScrollingTargetData> w, int after) {
    const float newSize = 1.F / (float)(targetDatas.size() + 1);

    for (size_t i = 0; i < targetDatas.size(); ++i) {
        setTargetSize(i, getTargetSize(i) * (float)targetDatas.size() / (float)(targetDatas.size() + 1));
    }

    targetDatas.insert(targetDatas.begin() + after + 1, w);
    w->column = self;

    // Sync sizes
    if (scrollingData) {
        auto sd = scrollingData.lock();
        if (sd && sd->controller) {
            int64_t colIdx = sd->idx(self.lock());
            if (colIdx >= 0 && (size_t)colIdx < sd->controller->stripCount()) {
                auto& strip = sd->controller->getStrip(colIdx);
                strip.targetSizes.insert(strip.targetSizes.begin() + after + 1, newSize);
            }
        }
    }
}

size_t SColumnData::idx(SP<ITarget> t) {
    for (size_t i = 0; i < targetDatas.size(); ++i) {
        if (targetDatas[i]->target == t)
            return i;
    }
    return 0;
}

size_t SColumnData::idxForHeight(float y) {
    if (targetDatas.empty())
        return 0;
    for (size_t i = 0; i < targetDatas.size(); ++i) {
        if (targetDatas[i]->target->position().y < y)
            continue;
        return i == 0 ? 0 : i - 1;
    }
    return targetDatas.size() - 1;
}

void SColumnData::remove(SP<ITarget> t) {
    const auto SIZE_BEFORE = targetDatas.size();
    size_t     removedIdx  = 0;
    bool       found       = false;

    for (size_t i = 0; i < targetDatas.size(); ++i) {
        if (targetDatas[i]->target == t) {
            removedIdx = i;
            found      = true;
            break;
        }
    }

    std::erase_if(targetDatas, [&t](const auto& e) { return e->target == t; });

    if (SIZE_BEFORE == targetDatas.size() && SIZE_BEFORE > 0)
        return;

    if (found && scrollingData) {
        auto sd = scrollingData.lock();
        if (sd && sd->controller) {
            int64_t colIdx = sd->idx(self.lock());
            if (colIdx >= 0 && (size_t)colIdx < sd->controller->stripCount()) {
                auto& strip = sd->controller->getStrip(colIdx);
                if (removedIdx < strip.targetSizes.size()) {
                    strip.targetSizes.erase(strip.targetSizes.begin() + removedIdx);
                }
            }
        }
    }

    // Renormalize sizes
    float newMaxSize = 0.F;
    for (size_t i = 0; i < targetDatas.size(); ++i) {
        newMaxSize += getTargetSize(i);
    }

    if (newMaxSize > 0.F) {
        for (size_t i = 0; i < targetDatas.size(); ++i) {
            setTargetSize(i, getTargetSize(i) / newMaxSize);
        }
    }

    if (targetDatas.empty() && scrollingData)
        scrollingData->remove(self.lock());
}

bool SColumnData::up(SP<SScrollingTargetData> w) {
    for (size_t i = 1; i < targetDatas.size(); ++i) {
        if (targetDatas[i] != w)
            continue;

        std::swap(targetDatas[i], targetDatas[i - 1]);
        return true;
    }

    return false;
}

bool SColumnData::down(SP<SScrollingTargetData> w) {
    if (targetDatas.empty())
        return false;

    for (size_t i = 0; i < targetDatas.size() - 1; ++i) {
        if (targetDatas[i] != w)
            continue;

        std::swap(targetDatas[i], targetDatas[i + 1]);
        return true;
    }

    return false;
}

SP<SScrollingTargetData> SColumnData::next(SP<SScrollingTargetData> w) {
    for (size_t i = 0; i < targetDatas.size() - 1; ++i) {
        if (targetDatas[i] != w)
            continue;

        return targetDatas[i + 1];
    }

    return nullptr;
}

SP<SScrollingTargetData> SColumnData::prev(SP<SScrollingTargetData> w) {
    for (size_t i = 1; i < targetDatas.size(); ++i) {
        if (targetDatas[i] != w)
            continue;

        return targetDatas[i - 1];
    }

    return nullptr;
}

bool SColumnData::has(SP<ITarget> t) {
    return std::ranges::find_if(targetDatas, [t](const auto& e) { return e->target == t; }) != targetDatas.end();
}

SScrollingData::SScrollingData(CScrollingAlgorithm* algo) : algorithm(algo) {
    controller = makeUnique<CScrollTapeController>(SCROLL_DIR_RIGHT);
}

SP<SColumnData> SScrollingData::add(std::optional<float> width) {
    auto col  = columns.emplace_back(makeShared<SColumnData>(self.lock()));
    col->self = col;

    size_t stripIdx                         = controller->addStrip(width.value_or(algorithm->defaultColumnWidth()));
    controller->getStrip(stripIdx).userData = col;

    return col;
}

SP<SColumnData> SScrollingData::add(int after, std::optional<float> width) {
    auto col  = makeShared<SColumnData>(self.lock());
    col->self = col;
    columns.insert(columns.begin() + after + 1, col);

    controller->insertStrip(after, width.value_or(algorithm->defaultColumnWidth()));
    controller->getStrip(after + 1).userData = col;

    return col;
}

int64_t SScrollingData::idx(SP<SColumnData> c) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] == c)
            return i;
    }

    return -1;
}

void SScrollingData::remove(SP<SColumnData> c) {
    // find index before removing
    int64_t index = idx(c);

    std::erase(columns, c);

    // sync with controller
    if (index >= 0)
        controller->removeStrip(index);
}

SP<SColumnData> SScrollingData::next(SP<SColumnData> c) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] != c)
            continue;

        if (i == columns.size() - 1)
            return nullptr;

        return columns[i + 1];
    }

    return nullptr;
}

SP<SColumnData> SScrollingData::prev(SP<SColumnData> c) {
    for (size_t i = 0; i < columns.size(); ++i) {
        if (columns[i] != c)
            continue;

        if (i == 0)
            return nullptr;

        return columns[i - 1];
    }

    return nullptr;
}

void SScrollingData::centerCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();
    int64_t           colIdx   = idx(c);

    if (colIdx >= 0)
        controller->centerStrip(colIdx, USABLE, *PFSONONE);
}

void SScrollingData::fitCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();
    int64_t           colIdx   = idx(c);

    if (colIdx >= 0)
        controller->fitStrip(colIdx, USABLE, *PFSONONE);
}

void SScrollingData::centerOrFitCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFITMETHOD = CConfigValue<Config::INTEGER>("scrolling:focus_fit_method");

    if (*PFITMETHOD == 1)
        fitCol(c);
    else
        centerCol(c);
}

SP<SColumnData> SScrollingData::atCenter() {
    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();

    size_t            centerIdx = controller->getStripAtCenter(USABLE, *PFSONONE);

    if (centerIdx < columns.size())
        return columns[centerIdx];

    return nullptr;
}

void SScrollingData::recalculate(bool forceInstant) {
    if (!algorithm->m_parent || !algorithm->m_parent->space() || !algorithm->m_parent->space()->workspace() || !algorithm->m_parent->space()->workspace()->m_monitor ||
        algorithm->m_parent->space()->workspace()->m_hasFullscreenWindow)
        return;

    algorithm->syncFullscreenTargets();

    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");

    const CBox        USABLE   = algorithm->usableArea();
    const auto        WORKAREA = algorithm->m_parent->space()->workArea();
    const CBox        MONBOX   = algorithm->m_parent->space()->workspace()->m_monitor->logicalBox();

    const auto        WORKSPACERULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(algorithm->m_parent->space()->workspace());
    static auto       PGAPSINDATA   = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
    auto* const       PGAPSIN       = sc<Config::CCssGapData*>((PGAPSINDATA.ptr()));
    const auto        GAPSIN        = (WORKSPACERULE && WORKSPACERULE->m_gapsIn.has_value()) ? WORKSPACERULE->m_gapsIn.value() : *PGAPSIN;

    bool              anyFullscreenCovers = false;
    for (const auto& COL : columns) {
        if (algorithm->fullscreenTargetDataForColumn(COL) && algorithm->fullscreenColumnCoversMonitor(COL)) {
            anyFullscreenCovers = true;
            break;
        }
    }

    controller->setDirection(algorithm->getDynamicDirection());
    algorithm->updateFullscreenFade(anyFullscreenCovers);

    const auto targetBoxWithGaps = [&](const CBox& logical, size_t colIdx, size_t targetIdx, bool fullscreenOrHidden) -> STargetBox {
        if (fullscreenOrHidden)
            return {.logicalBox = logical, .visualBox = logical};

        CBox       visual        = logical;
        const bool PRIMARY_HORIZ = controller->isPrimaryHorizontal();

        const bool COL_NOT_LEFT      = controller->isReversed() ? colIdx + 1 < columns.size() : colIdx > 0;
        const bool COL_NOT_RIGHT     = controller->isReversed() ? colIdx > 0 : colIdx + 1 < columns.size();
        const bool TARGET_NOT_TOP    = controller->isReversed() ? targetIdx + 1 < columns[colIdx]->targetDatas.size() : targetIdx > 0;
        const bool TARGET_NOT_BOTTOM = controller->isReversed() ? targetIdx > 0 : targetIdx + 1 < columns[colIdx]->targetDatas.size();

        const bool GAP_LEFT   = PRIMARY_HORIZ ? COL_NOT_LEFT : TARGET_NOT_TOP;
        const bool GAP_RIGHT  = PRIMARY_HORIZ ? COL_NOT_RIGHT : TARGET_NOT_BOTTOM;
        const bool GAP_TOP    = PRIMARY_HORIZ ? TARGET_NOT_TOP : COL_NOT_LEFT;
        const bool GAP_BOTTOM = PRIMARY_HORIZ ? TARGET_NOT_BOTTOM : COL_NOT_RIGHT;

        const auto GAPOFFSETTOPLEFT     = Vector2D(sc<double>(GAP_LEFT ? GAPSIN.m_left : 0), sc<double>(GAP_TOP ? GAPSIN.m_top : 0));
        const auto GAPOFFSETBOTTOMRIGHT = Vector2D(sc<double>(GAP_RIGHT ? GAPSIN.m_right : 0), sc<double>(GAP_BOTTOM ? GAPSIN.m_bottom : 0));

        visual.x += GAPOFFSETTOPLEFT.x;
        visual.y += GAPOFFSETTOPLEFT.y;
        visual.w = std::max(1.0, visual.w - GAPOFFSETTOPLEFT.x - GAPOFFSETBOTTOMRIGHT.x);
        visual.h = std::max(1.0, visual.h - GAPOFFSETTOPLEFT.y - GAPOFFSETBOTTOMRIGHT.y);

        return {.logicalBox = logical, .visualBox = visual};
    };

    for (size_t i = 0; i < columns.size(); ++i) {
        const auto& COL = columns[i];
        const auto  FS  = algorithm->fullscreenTargetDataForColumn(COL);

        for (size_t j = 0; j < COL->targetDatas.size(); ++j) {
            const auto& TARGET = COL->targetDatas[j];

            if (FS) {
                if (TARGET == FS) {
                    if (algorithm->fullscreenColumnCoversMonitor(COL))
                        TARGET->layoutBox = MONBOX;
                    else {
                        TARGET->layoutBox = controller->calculateStripBox(i, USABLE, WORKAREA.pos(), *PFSONONE);

                        if (controller->isPrimaryHorizontal()) {
                            TARGET->layoutBox.y = MONBOX.y;
                            TARGET->layoutBox.h = MONBOX.h;
                        } else {
                            TARGET->layoutBox.x = MONBOX.x;
                            TARGET->layoutBox.w = MONBOX.w;
                        }
                    }
                } else
                    TARGET->layoutBox = CBox{WORKAREA.pos() - Vector2D{100000.0, 100000.0}, Vector2D{1.0, 1.0}};
            } else
                TARGET->layoutBox = controller->calculateTargetBox(i, j, USABLE, WORKAREA.pos(), *PFSONONE);

            if (TARGET->target)
                TARGET->target->setPositionGlobal(targetBoxWithGaps(TARGET->layoutBox, i, j, FS));

            if (forceInstant && TARGET->target)
                TARGET->target->warpPositionSize();
        }
    }
}

double SScrollingData::maxWidth() {
    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();

    return controller->calculateMaxExtent(USABLE, *PFSONONE);
}

bool SScrollingData::visible(SP<SColumnData> c, bool full) {
    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();
    int64_t           colIdx   = idx(c);

    if (colIdx >= 0)
        return controller->isStripVisible(colIdx, USABLE, *PFSONONE, full);

    return false;
}

CScrollingAlgorithm::CScrollingAlgorithm() {
    static const auto PCONFWIDTHS    = CConfigValue<Config::STRING>("scrolling:explicit_column_widths");
    static const auto PCONFDIRECTION = CConfigValue<Config::STRING>("scrolling:direction");

    m_scrollingData       = makeShared<SScrollingData>(this);
    m_scrollingData->self = m_scrollingData;

    // Helper to parse explicit_column_widths string
    auto parseColumnWidths = [](const std::string& dir) -> std::vector<float> {
        auto          widthVec = std::vector<float>();

        CConstVarList widths(dir, 0, ',');
        for (auto& w : widths) {
            try {
                widthVec.emplace_back(std::clamp(std::stof(std::string{w}), MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH));
            } catch (...) { Log::logger->log(Log::ERR, "scrolling: Failed to parse width {} as float", w); }
        }
        if (widthVec.empty())
            widthVec = {0.333, 0.5, 0.667, 1.0}; // default
        return widthVec;
    };

    // Helper to parse direction string
    auto parseDirection = [](const std::string& dir) -> eScrollDirection {
        if (dir == "left")
            return SCROLL_DIR_LEFT;
        else if (dir == "down")
            return SCROLL_DIR_DOWN;
        else if (dir == "up")
            return SCROLL_DIR_UP;
        else
            return SCROLL_DIR_RIGHT; // default
    };

    m_configCallback = Event::bus()->m_events.config.reloaded.listen([this, parseColumnWidths, parseDirection] {
        static const auto PCONFDIRECTION = CConfigValue<Config::STRING>("scrolling:direction");

        m_config.configuredWidths.clear();
        m_config.configuredWidths = parseColumnWidths(*PCONFWIDTHS);

        // Update scroll direction
        m_scrollingData->controller->setDirection(parseDirection(*PCONFDIRECTION));
    });

    m_mouseButtonCallback = Event::bus()->m_events.input.mouse.button.listen([this](IPointer::SButtonEvent e, Event::SCallbackInfo&) {
        static const auto PFOLLOW_FOCUS = CConfigValue<Config::INTEGER>("scrolling:follow_focus");

        if (*PFOLLOW_FOCUS && e.state == WL_POINTER_BUTTON_STATE_RELEASED && Desktop::focusState()->window())
            focusOnInput(Desktop::focusState()->window()->layoutTarget(), INPUT_MODE_CLICK);
    });

    m_focusCallback = Event::bus()->m_events.window.active.listen([this](PHLWINDOW pWindow, Desktop::eFocusReason reason) {
        if (!pWindow)
            return;

        static const auto PFOLLOW_FOCUS = CConfigValue<Config::INTEGER>("scrolling:follow_focus");

        if (!*PFOLLOW_FOCUS && !Desktop::isHardInputFocusReason(reason))
            return;

        if (pWindow->m_workspace != m_parent->space()->workspace())
            return;

        const auto TARGET = pWindow->layoutTarget();
        if (!TARGET || TARGET->floating())
            return;

        // if follow_focus != 0, focuswindow always moves scrolling view
        // if follow_focus != 0, change in a group's current window state always moves scrolling view
        // if follow_focus != 0, moving a window into group via the corresponding dispatches `moveintogroup`, `movewindoworgroup` always moves scrolling view
        // if follow_focus != 0, moving focus via dispatches that cause switching to a specific window via calling switchToWindow(), such as movefocus, cyclenext, focuscurrentor(last/urgent); always moves scrolling view
        if (*PFOLLOW_FOCUS &&
            (reason == Desktop::FOCUS_REASON_DISPATCH_FOCUSWINDOW || reason == Desktop::FOCUS_REASON_GROUP_CURRENT_WINDOW_CHANGE ||
             reason == Desktop::FOCUS_REASON_DISPATCH_MOVEWINDOWINTOGROUP || reason == Desktop::FOCUS_REASON_SWITCH_TO_WINDOW_SOFT))
            focusOnInput(TARGET, INPUT_MODE_HARD);
        else
            focusOnInput(TARGET, reason == Desktop::FOCUS_REASON_CLICK ? INPUT_MODE_CLICK : (Desktop::isHardInputFocusReason(reason) ? INPUT_MODE_HARD : INPUT_MODE_SOFT));
    });

    // Initialize default widths and direction
    m_config.configuredWidths = parseColumnWidths(*PCONFWIDTHS);
    m_scrollingData->controller->setDirection(parseDirection(*PCONFDIRECTION));
}

CScrollingAlgorithm::~CScrollingAlgorithm() {
    clearFullscreenTarget();
    updateFullscreenFade(false);

    m_configCallback.reset();
    m_focusCallback.reset();
}

void CScrollingAlgorithm::focusOnInput(SP<ITarget> target, eInputMode input) {
    static const auto PFOLLOW_FOCUS_MIN_PERC = CConfigValue<Config::FLOAT>("scrolling:follow_min_visible");

    if (!target || target->space() != m_parent->space())
        return;

    const auto TARGETDATA = dataFor(target);
    if (!TARGETDATA)
        return;

    if (*PFOLLOW_FOCUS_MIN_PERC > 0.F && input == INPUT_MODE_SOFT) {
        // check how much of the window is visible, unless hard input focus

        const auto   IS_HORIZ = m_scrollingData->controller->isPrimaryHorizontal();

        const auto   MON_BOX     = m_parent->space()->workspace()->m_monitor->logicalBox();
        const auto   TARGET_POS  = target->position();
        const double VISIBLE_LEN = IS_HORIZ ?                                                                            //
            std::abs(std::min(MON_BOX.x + MON_BOX.w, TARGET_POS.x + TARGET_POS.w) - (std::max(MON_BOX.x, TARGET_POS.x))) //
            :
            std::abs(std::min(MON_BOX.y + MON_BOX.h, TARGET_POS.y + TARGET_POS.h) - (std::max(MON_BOX.y, TARGET_POS.y)));

        // if the amount of visible X is below minimum, reject
        if (VISIBLE_LEN < (IS_HORIZ ? MON_BOX.w : MON_BOX.h) * std::clamp(*PFOLLOW_FOCUS_MIN_PERC, 0.F, 1.F))
            return;
    }

    // if we moved via non-kb, and it's fully visible, ignore
    if (m_scrollingData->visible(TARGETDATA->column.lock(), true) && input != INPUT_MODE_HARD)
        return;

    static const auto PFITMETHOD = CConfigValue<Config::INTEGER>("scrolling:focus_fit_method");
    if (*PFITMETHOD == 1)
        m_scrollingData->fitCol(TARGETDATA->column.lock());
    else
        m_scrollingData->centerCol(TARGETDATA->column.lock());
    m_scrollingData->recalculate();
}

void CScrollingAlgorithm::newTarget(SP<ITarget> target) {
    auto droppingOn = Desktop::focusState()->window();

    if (droppingOn && droppingOn->layoutTarget() == target)
        droppingOn = g_pCompositor->vectorToWindowUnified(g_pInputManager->getMouseCoordsInternal(), Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS);

    SP<SScrollingTargetData> droppingData   = droppingOn ? dataFor(droppingOn->layoutTarget()) : nullptr;
    SP<SColumnData>          droppingColumn = droppingData ? droppingData->column.lock() : nullptr;
    const auto               width          = target->window()->m_ruleApplicator->static_.scrollingWidth;

    if (!droppingColumn) {
        auto col = m_scrollingData->add(width);
        col->add(target);
        m_scrollingData->fitCol(col);
    } else {
        if (g_layoutManager->dragController()->wasDraggingWindow() && g_layoutManager->dragController()->draggingTiled()) {
            if (droppingOn) {
                const auto IDX = droppingColumn->idx(droppingOn->layoutTarget());
                const auto TOP = droppingOn->getWindowIdealBoundingBoxIgnoreReserved().middle().y > g_pInputManager->getMouseCoordsInternal().y;
                droppingColumn->add(target, TOP ? (IDX == 0 ? -1 : IDX - 1) : (IDX));
            } else
                droppingColumn->add(target);
            m_scrollingData->fitCol(droppingColumn);
        } else {
            auto idx = m_scrollingData->idx(droppingColumn);
            auto col = idx == -1 ? m_scrollingData->add(width) : m_scrollingData->add(idx, width);
            col->add(target);
            m_scrollingData->fitCol(col);
        }
    }

    m_scrollingData->recalculate();
}

void CScrollingAlgorithm::movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) {
    newTarget(target);
}

void CScrollingAlgorithm::removeTarget(SP<ITarget> target) {
    const auto DATA = dataFor(target);

    if (!DATA)
        return;

    clearFullscreenTarget(target);

    if (!m_scrollingData->next(DATA->column.lock()) && DATA->column->targetDatas.size() <= 1) {
        // move the view if this is the last column
        const auto   USABLE         = usableArea();
        const bool   isPrimaryHoriz = m_scrollingData->controller->isPrimaryHorizontal();
        const double usablePrimary  = isPrimaryHoriz ? USABLE.w : USABLE.h;
        m_scrollingData->controller->adjustOffset(-(usablePrimary * DATA->column->getColumnWidth()));
    }

    DATA->column->remove(target);

    if (!DATA->column) {
        // column got removed, let's ensure we don't leave any cringe extra space
        const auto   USABLE         = usableArea();
        const bool   isPrimaryHoriz = m_scrollingData->controller->isPrimaryHorizontal();
        const double usablePrimary  = isPrimaryHoriz ? USABLE.w : USABLE.h;
        const double newOffset      = std::clamp(m_scrollingData->controller->getOffset(), 0.0, std::max(m_scrollingData->maxWidth() - usablePrimary, 1.0));
        m_scrollingData->controller->setOffset(newOffset);
    }

    m_scrollingData->recalculate();
}

void CScrollingAlgorithm::resizeTarget(const Vector2D& delta, SP<ITarget> target, eRectCorner corner) {
    if (!validMapped(target->window()))
        return;

    const auto DATA = dataFor(target);

    if (!DATA) {
        const auto PWINDOW   = target->window();
        *PWINDOW->m_realSize = (PWINDOW->m_realSize->goal() + delta)
                                   .clamp(PWINDOW->m_ruleApplicator->minSize().valueOr(Vector2D{MIN_WINDOW_SIZE, MIN_WINDOW_SIZE}),
                                          PWINDOW->m_ruleApplicator->maxSize().valueOr(Vector2D{INFINITY, INFINITY}));
        PWINDOW->updateWindowDecos();
        return;
    }

    if (!DATA->column || !DATA->column->scrollingData)
        return;

    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");

    const auto        ADJUSTED_DELTA = m_scrollingData->controller->isPrimaryHorizontal() ? delta : Vector2D{delta.y, delta.x};
    const auto        USABLE         = usableArea();
    const auto        DELTA_AS_PERC  = ADJUSTED_DELTA / USABLE.size();
    Vector2D          modDelta       = ADJUSTED_DELTA;

    const auto        CURR_COLUMN = DATA->column.lock();
    const int64_t     COL_IDX     = m_scrollingData->idx(CURR_COLUMN);

    if (COL_IDX < 0)
        return;

    const double currentStart = m_scrollingData->controller->calculateStripStart(COL_IDX, USABLE, *PFSONONE);
    const double currentSize  = m_scrollingData->controller->calculateStripSize(COL_IDX, USABLE, *PFSONONE);
    const double currentEnd   = currentStart + currentSize;

    const double cameraOffset   = m_scrollingData->controller->getOffset();
    const bool   isPrimaryHoriz = m_scrollingData->controller->isPrimaryHorizontal();
    const double usablePrimary  = isPrimaryHoriz ? USABLE.w : USABLE.h;

    const double onScreenStart = currentStart - cameraOffset;
    const double onScreenEnd   = currentEnd - cameraOffset;

    // set the offset because we'll prevent centering during a drag
    m_scrollingData->controller->setOffset(cameraOffset);

    const bool RESIZING_LEFT = isPrimaryHoriz ? corner == CORNER_BOTTOMLEFT || corner == CORNER_TOPLEFT : corner == CORNER_TOPLEFT || corner == CORNER_TOPRIGHT;

    if (RESIZING_LEFT) {
        // resize from left edge (inner edge) - grow/shrink column width and adjust offset to keep RIGHT edge stationary
        const float oldWidth       = CURR_COLUMN->getColumnWidth();
        const float requestedDelta = -(float)DELTA_AS_PERC.x; // negative delta means grow when dragging left
        float       actualDelta    = requestedDelta;

        // clamp delta so we don't shrink below MIN or grow above MAX
        const float newWidthUnclamped = oldWidth + actualDelta;
        const float newWidthClamped   = std::clamp(newWidthUnclamped, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
        actualDelta                   = newWidthClamped - oldWidth;

        if (actualDelta * usablePrimary > onScreenStart)
            actualDelta = onScreenStart / usablePrimary;

        if (actualDelta != 0.F) {
            CURR_COLUMN->setColumnWidth(oldWidth + actualDelta);
            // adjust camera offset so the RIGHT edge stays stationary on screen
            // when column grows (actualDelta > 0), we need to increase offset by the same amount
            m_scrollingData->controller->adjustOffset(actualDelta * usablePrimary);
        }

    } else {
        // resize from right edge (outer edge) - adjust column width only, keep left edge fixed
        const float oldWidth       = CURR_COLUMN->getColumnWidth();
        const float requestedDelta = (float)DELTA_AS_PERC.x;
        float       actualDelta    = requestedDelta;

        // clamp delta so we don't shrink below MIN or grow above MAX
        const float newWidthUnclamped = oldWidth + actualDelta;
        const float newWidthClamped   = std::clamp(newWidthUnclamped, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
        actualDelta                   = newWidthClamped - oldWidth;

        // also clamp so right edge doesn't go past right viewport boundary
        if (onScreenEnd + (actualDelta * usablePrimary) > usablePrimary)
            actualDelta = (usablePrimary - onScreenEnd) / usablePrimary;

        if (actualDelta != 0.F)
            CURR_COLUMN->setColumnWidth(oldWidth + actualDelta);
    }

    if (DATA->column->targetDatas.size() > 1) {
        const auto& CURR_TD = DATA;
        const auto  NEXT_TD = DATA->column->next(DATA);
        const auto  PREV_TD = DATA->column->prev(DATA);
        if (corner == CORNER_NONE) {
            if (!PREV_TD)
                corner = CORNER_BOTTOMRIGHT;
            else {
                corner = CORNER_TOPRIGHT;
                modDelta.y *= -1.0f;
            }
        }

        switch (corner) {
            case CORNER_BOTTOMLEFT:
            case CORNER_BOTTOMRIGHT: {
                if (!NEXT_TD)
                    break;

                float nextSize = CURR_COLUMN->getTargetSize(NEXT_TD);
                float currSize = CURR_COLUMN->getTargetSize(CURR_TD);

                if (nextSize <= MIN_ROW_HEIGHT && delta.y >= 0)
                    break;

                float adjust = std::clamp((float)(delta.y / USABLE.h), (-currSize + MIN_ROW_HEIGHT), (nextSize - MIN_ROW_HEIGHT));

                CURR_COLUMN->setTargetSize(NEXT_TD, std::clamp(nextSize - adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT));
                CURR_COLUMN->setTargetSize(CURR_TD, std::clamp(currSize + adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT));
                break;
            }
            case CORNER_TOPLEFT:
            case CORNER_TOPRIGHT: {
                if (!PREV_TD)
                    break;

                float prevSize = CURR_COLUMN->getTargetSize(PREV_TD);
                float currSize = CURR_COLUMN->getTargetSize(CURR_TD);

                if ((prevSize <= MIN_ROW_HEIGHT && modDelta.y <= 0) || (currSize <= MIN_ROW_HEIGHT && delta.y >= 0))
                    break;

                float adjust = std::clamp((float)(modDelta.y / USABLE.h), -(prevSize - MIN_ROW_HEIGHT), (currSize - MIN_ROW_HEIGHT));

                CURR_COLUMN->setTargetSize(PREV_TD, std::clamp(prevSize + adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT));
                CURR_COLUMN->setTargetSize(CURR_TD, std::clamp(currSize - adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT));
                break;
            }

            default: break;
        }
    }

    m_scrollingData->recalculate(true);
}

void CScrollingAlgorithm::recalculate(eRecalculateReason reason) {
    // guard against recalculation during transitional monitor states
    // (e.g. monitor reconnecting after suspend where workspace/monitor may not be ready)
    if (!m_parent || !m_parent->space() || !m_parent->space()->workspace() || !m_parent->space()->workspace()->m_monitor)
        return;

    if (Desktop::focusState()->window()) {
        const auto TARGET = Desktop::focusState()->window()->layoutTarget();

        const auto TARGETDATA = dataFor(TARGET);

        if (TARGETDATA && !m_scrollingData->visible(TARGETDATA->column.lock(), true)) {

            /* guard against unwanted scrolling viewport moves - If recalculate() was called, it is assumed that either the INPUT_MODE will be HARD (i.e. it is meant to move the scrolling viewport) or
            it is not meant to move the scrolling viewport.
            (e.g. changing workspace to a scrolling layout workspace fits the focused window in that workspace into view) */
            if (Layout::isHardRecalculateReason(reason))
                focusOnInput(Desktop::focusState()->window()->layoutTarget(), INPUT_MODE_HARD);
        }
    }

    m_scrollingData->recalculate();
}

void CScrollingAlgorithm::syncFullscreenTargets() {
    for (auto it = m_fullscreenTargets.begin(); it != m_fullscreenTargets.end();) {
        const auto TARGET = it->target.lock();

        if (!TARGET || !TARGET->layoutManagedFullscreen() || TARGET->fullscreenMode() != FSMODE_FULLSCREEN || TARGET->space() != m_parent->space()) {
            it = m_fullscreenTargets.erase(it);
            continue;
        }

        const auto TDATA = dataFor(TARGET);
        if (!TDATA) {
            ++it;
            continue;
        }

        if (const auto COL = TDATA->column.lock())
            COL->setColumnWidth(fullscreenColumnWidth());

        ++it;
    }

    for (const auto& COL : m_scrollingData->columns) {
        for (const auto& TDATA : COL->targetDatas) {
            const auto TARGET = TDATA->target.lock();
            if (!TARGET || !TARGET->layoutManagedFullscreen() || TARGET->fullscreenMode() != FSMODE_FULLSCREEN || TARGET->space() != m_parent->space())
                continue;

            if (!fullscreenStateForTarget(TARGET))
                m_fullscreenTargets.emplace_back(SFullscreenScrollState{.target = TARGET, .restoreColumnWidth = COL ? std::optional<float>{COL->getColumnWidth()} : std::nullopt});

            COL->setColumnWidth(fullscreenColumnWidth());
        }
    }
}

CScrollingAlgorithm::SFullscreenScrollState* CScrollingAlgorithm::fullscreenStateForTarget(SP<ITarget> target) {
    if (!target)
        return nullptr;

    for (auto& state : m_fullscreenTargets) {
        if (state.target.lock() == target)
            return &state;
    }

    return nullptr;
}

CScrollingAlgorithm::SFullscreenScrollState* CScrollingAlgorithm::fullscreenStateForData(SP<SScrollingTargetData> target) {
    if (!target)
        return nullptr;

    return fullscreenStateForTarget(target->target.lock());
}

void CScrollingAlgorithm::expelTarget(SP<SScrollingTargetData> tdata, SP<SColumnData> srcCol, std::optional<int64_t> insertIdx) {
    auto col = !insertIdx ? m_scrollingData->add() : m_scrollingData->add(*insertIdx);
    srcCol->remove(tdata->target.lock());
    col->add(tdata);
    m_scrollingData->centerOrFitCol(col);
}

eFullscreenRequestResult CScrollingAlgorithm::requestFullscreen(const SFullscreenRequest& request) {
    if (!request.target || !m_parent || request.target->space() != m_parent->space())
        return FULLSCREEN_REQUEST_DEFAULT;

    const auto TDATA = dataFor(request.target);
    if (!TDATA)
        return FULLSCREEN_REQUEST_DEFAULT;

    if (request.effectiveMode == FSMODE_FULLSCREEN) {
        if (!fullscreenStateForTarget(request.target)) {
            const auto COL = TDATA->column.lock();
            m_fullscreenTargets.emplace_back(
                SFullscreenScrollState{.target = request.target, .restoreColumnWidth = COL ? std::optional<float>{COL->getColumnWidth()} : std::nullopt});
        }

        if (const auto COL = TDATA->column.lock()) {
            COL->setColumnWidth(fullscreenColumnWidth());
            m_scrollingData->centerOrFitCol(COL);
        }

        request.target->setFullscreenMode(FSMODE_FULLSCREEN);

        return FULLSCREEN_REQUEST_HANDLED_BY_LAYOUT;
    } else if (request.effectiveMode == FSMODE_MAXIMIZED) {
        // expel, then max width
        const auto CURRENT_COL = TDATA->column.lock();

        if (CURRENT_COL->targetDatas.size() > 1) {
            const auto lastTarget = CURRENT_COL->targetDatas.back();
            const auto currentIdx = m_scrollingData->idx(CURRENT_COL);
            const auto NEXT_COL   = m_scrollingData->next(CURRENT_COL);
            const auto insertIdx  = !NEXT_COL ? std::nullopt : std::optional<int64_t>{currentIdx};

            expelTarget(lastTarget, CURRENT_COL, insertIdx);

            TDATA->column->setColumnWidth(1.F);
        } else
            CURRENT_COL->setColumnWidth(1.F);

        request.target->setFullscreenMode(FSMODE_NONE);

        return FULLSCREEN_REQUEST_HANDLED_BY_LAYOUT;
    }

    if (isFullscreenTarget(TDATA) || request.target->layoutManagedFullscreen()) {
        clearFullscreenTarget(request.target);
        request.target->setFullscreenMode(FSMODE_NONE);
        return request.effectiveMode == FSMODE_NONE ? FULLSCREEN_REQUEST_HANDLED_BY_LAYOUT : FULLSCREEN_REQUEST_DEFAULT;
    }

    return FULLSCREEN_REQUEST_DEFAULT;
}

SP<ITarget> CScrollingAlgorithm::layoutFullscreenTarget() const {
    SP<SScrollingTargetData> fallback;

    for (const auto& COL : m_scrollingData->columns) {
        for (const auto& TDATA : COL->targetDatas) {
            if (!isFullscreenTarget(TDATA))
                continue;

            if (!fallback)
                fallback = TDATA;

            if (fullscreenColumnCoversMonitor(TDATA->column.lock()))
                return TDATA->target.lock();
        }
    }

    return fallback ? fallback->target.lock() : nullptr;
}

bool CScrollingAlgorithm::layoutFullscreenCoversMonitor() const {
    for (const auto& COL : m_scrollingData->columns) {
        for (const auto& TDATA : COL->targetDatas) {
            if (!isFullscreenTarget(TDATA))
                continue;

            if (fullscreenColumnCoversMonitor(TDATA->column.lock()))
                return true;
        }
    }

    return false;
}

SP<SScrollingTargetData> CScrollingAlgorithm::fullscreenTargetDataForColumn(SP<SColumnData> col) const {
    if (!col)
        return nullptr;

    for (const auto& TDATA : col->targetDatas) {
        if (!isFullscreenTarget(TDATA))
            continue;

        return TDATA;
    }

    return nullptr;
}

bool CScrollingAlgorithm::isFullscreenTarget(SP<SScrollingTargetData> target) const {
    if (!target)
        return false;

    const auto TARGET = target->target.lock();
    if (!TARGET || !TARGET->layoutManagedFullscreen() || TARGET->fullscreenMode() == FSMODE_NONE)
        return false;

    return dataFor(TARGET) == target;
}

float CScrollingAlgorithm::fullscreenColumnWidth() const {
    if (!m_parent || !m_parent->space() || !m_parent->space()->workspace() || !m_parent->space()->workspace()->m_monitor || !m_scrollingData || !m_scrollingData->controller)
        return 1.F;

    const auto   USABLE         = usableArea();
    const auto   MONBOX         = m_parent->space()->workspace()->m_monitor->logicalBox();
    const bool   PRIMARY_HORIZ  = m_scrollingData->controller->isPrimaryHorizontal();
    const double usablePrimary  = PRIMARY_HORIZ ? USABLE.w : USABLE.h;
    const double monitorPrimary = PRIMARY_HORIZ ? MONBOX.w : MONBOX.h;

    if (usablePrimary <= 0.0)
        return 1.F;

    return std::max(1.F, sc<float>(monitorPrimary / usablePrimary));
}

bool CScrollingAlgorithm::fullscreenColumnCoversMonitor(SP<SColumnData> col) const {
    if (!col || !m_scrollingData || !m_scrollingData->controller || !m_parent || !m_parent->space() || !m_parent->space()->workspace() ||
        !m_parent->space()->workspace()->m_monitor)
        return false;

    if (!fullscreenTargetDataForColumn(col))
        return false;

    const int64_t COL_IDX = m_scrollingData->idx(col);
    if (COL_IDX < 0)
        return false;

    static const auto PFSONONE = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");

    const auto        USABLE        = usableArea();
    const bool        PRIMARY_HORIZ = m_scrollingData->controller->isPrimaryHorizontal();
    const double      VIEW_SIZE     = PRIMARY_HORIZ ? USABLE.w : USABLE.h;
    const double      VIEW_START    = m_scrollingData->controller->getOffset();
    const double      VIEW_END      = VIEW_START + VIEW_SIZE;
    const double      COL_START     = m_scrollingData->controller->calculateStripStart(COL_IDX, USABLE, *PFSONONE);
    const double      COL_END       = COL_START + m_scrollingData->controller->calculateStripSize(COL_IDX, USABLE, *PFSONONE);

    return COL_START <= VIEW_START + 1.0 && COL_END >= VIEW_END - 1.0;
}

void CScrollingAlgorithm::updateFullscreenFade(bool coversMonitor) {
    if (m_lastFullscreenCover == coversMonitor)
        return;

    m_lastFullscreenCover = coversMonitor;

    if (!coversMonitor) {
        // prevent stuck focus
        g_pInputManager->unconstrainMouse();
        for (const auto& fs : m_fullscreenTargets) {
            if (!fs.target || !fs.target->window())
                continue;

            auto w = fs.target->window();

            w->m_layoutFlags.cantLockCursor = true;
        }
    } else {
        for (const auto& fs : m_fullscreenTargets) {
            if (!fs.target || !fs.target->window())
                continue;

            auto w = fs.target->window();

            w->m_layoutFlags.cantLockCursor = false;
        }
    }

    if (!m_parent || !m_parent->space() || !m_parent->space()->workspace())
        return;

    // properly update things on top / bottom
    m_parent->space()->workspace()->setNoMembersAboveFullscreen();

    g_pDesktopAnimationManager->setFullscreenFadeAnimation(m_parent->space()->workspace(),
                                                           coversMonitor ? CDesktopAnimationManager::ANIMATION_TYPE_IN : CDesktopAnimationManager::ANIMATION_TYPE_OUT);
}

void CScrollingAlgorithm::clearFullscreenTarget(SP<ITarget> target) {
    bool cleared = false;

    auto clear = [&](SP<ITarget> t) {
        t->setLayoutManagedFullscreen(false);
        if (t->window())
            t->window()->m_layoutFlags.cantLockCursor = false;
        cleared = true;
    };

    for (auto it = m_fullscreenTargets.begin(); it != m_fullscreenTargets.end();) {
        const auto TARGET = it->target.lock();

        if (!TARGET || (target && TARGET != target)) {
            if (!TARGET)
                it = m_fullscreenTargets.erase(it);
            else
                ++it;
            continue;
        }

        const auto TDATA = dataFor(TARGET);

        clear(TARGET);

        if (const auto COL = TDATA ? TDATA->column.lock() : nullptr; COL && it->restoreColumnWidth)
            COL->setColumnWidth(*it->restoreColumnWidth);

        it = m_fullscreenTargets.erase(it);
    }

    if (target && target->layoutManagedFullscreen())
        clear(target);
    else if (!target) {
        for (const auto& COL : m_scrollingData->columns) {
            for (const auto& TDATA : COL->targetDatas) {
                const auto TARGET = TDATA->target.lock();
                if (!TARGET || !TARGET->layoutManagedFullscreen())
                    continue;

                clear(TARGET);
            }
        }
    }
}

SP<SScrollingTargetData> CScrollingAlgorithm::closestNode(const Vector2D& posGlobglobgabgalab) {
    SP<SScrollingTargetData> res         = nullptr;
    double                   distClosest = -1;
    for (auto& c : m_scrollingData->columns) {
        for (auto& n : c->targetDatas) {
            if (n->target && Desktop::View::validMapped(n->target->window())) {
                auto distAnother = vecToRectDistanceSquared(posGlobglobgabgalab, n->layoutBox.pos(), n->layoutBox.pos() + n->layoutBox.size());
                if (!res || distAnother < distClosest) {
                    res         = n;
                    distClosest = distAnother;
                }
            }
        }
    }
    return res;
}

SP<ITarget> CScrollingAlgorithm::getNextCandidate(SP<ITarget> old) {
    const auto CENTER = old->position().middle();

    const auto NODE = closestNode(CENTER);

    if (!NODE)
        return nullptr;

    return NODE->target.lock();
}

void CScrollingAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {
    auto nodeA = dataFor(a);
    auto nodeB = dataFor(b);

    if (nodeA)
        nodeA->target = b;
    if (nodeB)
        nodeB->target = a;

    m_scrollingData->recalculate();
}

void CScrollingAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    moveTargetTo(t, dir, silent);
}

void CScrollingAlgorithm::moveTargetTo(SP<ITarget> t, Math::eDirection dir, bool silent) {
    static auto PMONITORFALLBACK = CConfigValue<Config::INTEGER>("binds:window_direction_monitor_fallback");

    const auto  DATA = dataFor(t);

    if (!DATA)
        return;

    const auto CURRENT_COL = DATA->column.lock();
    const auto current_idx = m_scrollingData->idx(CURRENT_COL);

    auto       rotateDir = [this](Math::eDirection dir) -> Math::eDirection {
        switch (m_scrollingData->controller->getDirection()) {
            case SCROLL_DIR_RIGHT: return dir;
            case SCROLL_DIR_LEFT: {
                if (dir == Math::DIRECTION_LEFT)
                    return Math::DIRECTION_RIGHT;
                if (dir == Math::DIRECTION_RIGHT)
                    return Math::DIRECTION_LEFT;
                return dir;
            }
            case SCROLL_DIR_UP: {
                switch (dir) {
                    case Math::DIRECTION_UP: return Math::DIRECTION_RIGHT;
                    case Math::DIRECTION_DOWN: return Math::DIRECTION_LEFT;
                    case Math::DIRECTION_LEFT: return Math::DIRECTION_UP;
                    case Math::DIRECTION_RIGHT: return Math::DIRECTION_DOWN;
                    default: break;
                }

                return dir;
            }
            case SCROLL_DIR_DOWN: {
                switch (dir) {
                    case Math::DIRECTION_UP: return Math::DIRECTION_LEFT;
                    case Math::DIRECTION_DOWN: return Math::DIRECTION_RIGHT;
                    case Math::DIRECTION_LEFT: return Math::DIRECTION_UP;
                    case Math::DIRECTION_RIGHT: return Math::DIRECTION_DOWN;
                    default: break;
                }

                return dir;
            }
            default: break;
        }

        return dir;
    };

    const auto ROTATED_DIR = rotateDir(dir);

    auto       commenceDir = [&]() -> bool {
        if (ROTATED_DIR == Math::DIRECTION_LEFT) {
            const auto COL = m_scrollingData->prev(DATA->column.lock());

            // ignore moves to the origin if we are alone
            if (!COL && current_idx == 0 && DATA->column->targetDatas.size() == 1)
                return false;

            DATA->column->remove(t);

            if (!COL) {
                const auto NEWCOL = m_scrollingData->add(-1);
                NEWCOL->add(DATA);
                m_scrollingData->centerOrFitCol(NEWCOL);
            } else {
                if (COL->targetDatas.size() > 0)
                    COL->add(DATA, COL->idxForHeight(g_pInputManager->getMouseCoordsInternal().y));
                else
                    COL->add(DATA);
                m_scrollingData->centerOrFitCol(COL);
            }

            return true;
        } else if (ROTATED_DIR == Math::DIRECTION_RIGHT) {
            const auto COL = m_scrollingData->next(DATA->column.lock());

            // ignore move to the right when there is no next column and we're alone
            if (!COL && current_idx == (int64_t)m_scrollingData->columns.size() - 1 && DATA->column->targetDatas.size() == 1)
                return false;

            DATA->column->remove(t);

            if (!COL) {
                // make a new one
                const auto NEWCOL = m_scrollingData->add();
                NEWCOL->add(DATA);
                m_scrollingData->centerOrFitCol(NEWCOL);
            } else {
                if (COL->targetDatas.size() > 0)
                    COL->add(DATA, COL->idxForHeight(g_pInputManager->getMouseCoordsInternal().y));
                else
                    COL->add(DATA);
                m_scrollingData->centerOrFitCol(COL);
            }

            return true;
        } else if (ROTATED_DIR == Math::DIRECTION_UP)
            return DATA->column->up(DATA);
        else if (ROTATED_DIR == Math::DIRECTION_DOWN)
            return DATA->column->down(DATA);

        return false;
    };

    if (!commenceDir()) {
        // dir wasn't commenced, move to a workspace if possible
        // with the original dir

        if (!*PMONITORFALLBACK)
            return; // noop

        const auto MONINDIR = g_pCompositor->getMonitorInDirection(m_parent->space()->workspace()->m_monitor.lock(), dir);
        if (MONINDIR && MONINDIR != m_parent->space()->workspace()->m_monitor && MONINDIR->m_activeWorkspace) {
            t->assignToSpace(MONINDIR->m_activeWorkspace->m_space, focalPointForDir(t, dir));

            m_scrollingData->recalculate();

            return;
        }
    }

    m_scrollingData->recalculate();
    focusTargetUpdate(t);
}

Config::ErrorResult CScrollingAlgorithm::layoutMsg(const std::string_view& sv) {
    const auto invalidArg = [](std::string msg) { return Config::configError(std::move(msg), Config::eConfigErrorLevel::ERROR, Config::eConfigErrorCode::INVALID_ARGUMENT); };
    const auto noTarget   = [](std::string msg) { return Config::configError(std::move(msg), Config::eConfigErrorLevel::WARNING, Config::eConfigErrorCode::NO_TARGET); };
    const auto notFound   = [](std::string msg) { return Config::configError(std::move(msg), Config::eConfigErrorLevel::WARNING, Config::eConfigErrorCode::NOT_FOUND); };
    const auto stateErr   = [](std::string msg) { return Config::configError(std::move(msg), Config::eConfigErrorLevel::WARNING, Config::eConfigErrorCode::INVALID_STATE); };

    auto       centerOrFit = [this](const SP<SColumnData> COL) -> void {
        static const auto PFITMETHOD = CConfigValue<Config::INTEGER>("scrolling:focus_fit_method");
        if (*PFITMETHOD == 1)
            m_scrollingData->fitCol(COL);
        else
            m_scrollingData->centerCol(COL);
    };

    const auto ARGS = CVarList(std::string{sv}, 0, ' ');
    if (ARGS[0] == "move") {
        if (ARGS[1] == "+col" || ARGS[1] == "col") {
            const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
            if (!TDATA)
                return noTarget("no window");

            const auto COL = m_scrollingData->next(TDATA->column.lock());
            if (!COL) {
                // move to max
                double maxOffset = m_scrollingData->maxWidth();
                m_scrollingData->controller->setOffset(maxOffset);
                m_scrollingData->recalculate();
                focusTargetUpdate(nullptr);
                return {};
            }

            centerOrFit(COL);
            m_scrollingData->recalculate();

            focusTargetUpdate(COL->targetDatas.front()->target.lock());
            if (COL->targetDatas.front()->target->window())
                g_pCompositor->warpCursorTo(COL->targetDatas.front()->target->window()->middle());

            return {};
        } else if (ARGS[1] == "-col") {
            const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
            if (!TDATA) {
                if (m_scrollingData->columns.size() > 0) {
                    m_scrollingData->centerCol(m_scrollingData->columns.back());
                    m_scrollingData->recalculate();
                    focusTargetUpdate((m_scrollingData->columns.back()->targetDatas.back())->target.lock());
                    if (m_scrollingData->columns.back()->targetDatas.back()->target->window())
                        g_pCompositor->warpCursorTo((m_scrollingData->columns.back()->targetDatas.back())->target->window()->middle());
                }

                return {};
            }

            const auto COL = m_scrollingData->prev(TDATA->column.lock());
            if (!COL)
                return {};

            centerOrFit(COL);
            m_scrollingData->recalculate();

            focusTargetUpdate(COL->targetDatas.back()->target.lock());
            if (COL->targetDatas.front()->target->window())
                g_pCompositor->warpCursorTo(COL->targetDatas.front()->target->window()->middle());

            return {};
        }

        const auto PLUSMINUS = getPlusMinusKeywordResult(ARGS[1], 0);

        if (!PLUSMINUS.has_value())
            return invalidArg("failed to parse offset");

        m_scrollingData->controller->adjustOffset(-(*PLUSMINUS));
        m_scrollingData->recalculate();

        const auto ATCENTER = m_scrollingData->atCenter();

        focusTargetUpdate(ATCENTER ? (*ATCENTER->targetDatas.begin())->target.lock() : nullptr);
    } else if (ARGS[0] == "colresize") {
        const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);

        if (!TDATA)
            return {};

        if (ARGS[1] == "all") {
            float abs = 0;
            try {
                abs = std::stof(ARGS[2]);
            } catch (...) { return {}; }

            for (const auto& c : m_scrollingData->columns) {
                c->setColumnWidth(abs);
            }

            m_scrollingData->recalculate();
            return {};
        }

        CScopeGuard x([this, TDATA] {
            auto col = TDATA->column.lock();
            if (col) {
                col->setColumnWidth(std::clamp(col->getColumnWidth(), MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH));
                m_scrollingData->centerOrFitCol(col);
            }
            m_scrollingData->recalculate();
        });

        if (ARGS[1][0] == '+' || ARGS[1][0] == '-') {
            if (ARGS[1] == "+conf") {
                auto col = TDATA->column.lock();
                if (col) {
                    for (size_t i = 0; i < m_config.configuredWidths.size(); ++i) {
                        if (m_config.configuredWidths[i] > col->getColumnWidth()) {
                            col->setColumnWidth(m_config.configuredWidths[i]);
                            break;
                        }

                        if (i == m_config.configuredWidths.size() - 1)
                            col->setColumnWidth(m_config.configuredWidths[0]);
                    }
                }

                return {};
            } else if (ARGS[1] == "-conf") {
                auto col = TDATA->column.lock();
                if (col) {
                    for (size_t i = m_config.configuredWidths.size() - 1;; --i) {
                        if (m_config.configuredWidths[i] < col->getColumnWidth()) {
                            col->setColumnWidth(m_config.configuredWidths[i]);
                            break;
                        }

                        if (i == 0) {
                            col->setColumnWidth(m_config.configuredWidths.back());
                            break;
                        }
                    }
                }

                return {};
            }

            const auto PLUSMINUS = getPlusMinusKeywordResult(ARGS[1], 0);

            if (!PLUSMINUS.has_value())
                return {};

            auto col = TDATA->column.lock();
            if (col)
                col->setColumnWidth(col->getColumnWidth() + *PLUSMINUS);
        } else {
            float abs = 0;
            try {
                abs = std::stof(ARGS[1]);
            } catch (...) { return {}; }

            auto col = TDATA->column.lock();
            if (col)
                col->setColumnWidth(abs);
        }
    } else if (ARGS[0] == "fit") {
        const auto PWINDOW = Desktop::focusState()->window();

        if (!PWINDOW)
            return noTarget("no focused window");

        const auto WDATA = dataFor(PWINDOW->layoutTarget());

        if (!WDATA || m_scrollingData->columns.size() == 0)
            return stateErr("can't fit: no window or columns");

        if (ARGS[1] == "active") {
            // fit the current column to 1.F
            const auto USABLE = usableArea();

            WDATA->column->setColumnWidth(1.F);

            double off = 0.F;
            for (size_t i = 0; i < m_scrollingData->columns.size(); ++i) {
                if (m_scrollingData->columns[i]->has(PWINDOW->layoutTarget()))
                    break;

                off += USABLE.w * m_scrollingData->columns[i]->getColumnWidth();
            }

            m_scrollingData->controller->setOffset(off);
            m_scrollingData->recalculate();
        } else if (ARGS[1] == "all") {
            // fit all columns on screen
            const size_t LEN = m_scrollingData->columns.size();
            for (const auto& c : m_scrollingData->columns) {
                c->setColumnWidth(1.F / (float)LEN);
            }

            m_scrollingData->controller->setOffset(0);
            m_scrollingData->recalculate();
        } else if (ARGS[1] == "toend") {
            // fit all columns on screen that start from the current and end on the last
            bool   begun   = false;
            size_t foundAt = 0;
            for (size_t i = 0; i < m_scrollingData->columns.size(); ++i) {
                if (!begun && !m_scrollingData->columns[i]->has(PWINDOW->layoutTarget()))
                    continue;

                if (!begun) {
                    begun   = true;
                    foundAt = i;
                }

                m_scrollingData->columns[i]->setColumnWidth(1.F / (float)(m_scrollingData->columns.size() - foundAt));
            }

            if (!begun)
                return notFound("couldn't find beginning");

            const auto USABLE = usableArea();

            double     off = 0;
            for (size_t i = 0; i < foundAt; ++i) {
                off += USABLE.w * m_scrollingData->columns[i]->getColumnWidth();
            }

            m_scrollingData->controller->setOffset(off);
            m_scrollingData->recalculate();
        } else if (ARGS[1] == "tobeg") {
            // fit all columns on screen that start from the current and end on the last
            bool   begun   = false;
            size_t foundAt = 0;
            for (int64_t i = (int64_t)m_scrollingData->columns.size() - 1; i >= 0; --i) {
                if (!begun && !m_scrollingData->columns[i]->has(PWINDOW->layoutTarget()))
                    continue;

                if (!begun) {
                    begun   = true;
                    foundAt = i;
                }

                m_scrollingData->columns[i]->setColumnWidth(1.F / (float)(foundAt + 1));
            }

            if (!begun)
                return {};

            m_scrollingData->controller->setOffset(0);
            m_scrollingData->recalculate();
        } else if (ARGS[1] == "visible") {
            // fit all columns on screen that start from the current and end on the last

            bool                         begun   = false;
            size_t                       foundAt = 0;
            std::vector<SP<SColumnData>> visible;
            for (size_t i = 0; i < m_scrollingData->columns.size(); ++i) {
                if (!begun && !m_scrollingData->visible(m_scrollingData->columns[i]))
                    continue;

                if (!begun) {
                    begun   = true;
                    foundAt = i;
                }

                if (!m_scrollingData->visible(m_scrollingData->columns[i]))
                    break;

                visible.emplace_back(m_scrollingData->columns[i]);
            }

            if (!begun)
                return {};

            double off = 0;

            if (foundAt != 0) {
                const auto USABLE = usableArea();

                for (size_t i = 0; i < foundAt; ++i) {
                    off += USABLE.w * m_scrollingData->columns[i]->getColumnWidth();
                }
            }

            for (const auto& v : visible) {
                v->setColumnWidth(1.F / (float)visible.size());
            }

            m_scrollingData->controller->setOffset(off);
            m_scrollingData->recalculate();
        }
    } else if (ARGS[0] == "focus") {
        const auto        TDATA          = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
        static const auto PNOFALLBACK    = CConfigValue<Config::INTEGER>("general:no_focus_fallback");
        static const auto PCONFWRAPFOCUS = CConfigValue<Config::INTEGER>("scrolling:wrap_focus");

        if (!TDATA || ARGS[1].empty())
            return noTarget("no window to focus");

        // Determine if we're in vertical scroll mode (strips are horizontal)
        const bool isVerticalScroll = (getDynamicDirection() == SCROLL_DIR_DOWN || getDynamicDirection() == SCROLL_DIR_UP);

        // Map direction keys based on scroll mode:
        // Horizontal scroll (RIGHT/LEFT): u/d move within strip, l/r move between strips
        // Vertical scroll (DOWN/UP): l/r move within strip, u/d move between strips
        char dirChar = ARGS[1][0];

        // Convert to semantic directions
        bool isPrevInStrip = (!isVerticalScroll && (dirChar == 'u' || dirChar == 't')) || (isVerticalScroll && dirChar == 'l');
        bool isNextInStrip = (!isVerticalScroll && (dirChar == 'b' || dirChar == 'd')) || (isVerticalScroll && dirChar == 'r');
        bool isPrevStrip   = (!isVerticalScroll && dirChar == 'l') || (isVerticalScroll && (dirChar == 'u' || dirChar == 't'));
        bool isNextStrip   = (!isVerticalScroll && dirChar == 'r') || (isVerticalScroll && (dirChar == 'b' || dirChar == 'd'));

        if (isPrevInStrip) {
            // Move to previous target within current strip
            auto PREV = TDATA->column->prev(TDATA);
            if (!PREV) {
                if (!*PNOFALLBACK)
                    PREV = TDATA->column->targetDatas.back();
                else
                    return notFound("fallback disabled (no target)");
            }

            focusTargetUpdate(PREV->target.lock());
            if (PREV->target->window())
                g_pCompositor->warpCursorTo(PREV->target->window()->middle());
        } else if (isNextInStrip) {
            // Move to next target within current strip
            auto NEXT = TDATA->column->next(TDATA);
            if (!NEXT) {
                if (!*PNOFALLBACK)
                    NEXT = TDATA->column->targetDatas.front();
                else
                    return notFound("fallback disabled (no target)");
            }

            focusTargetUpdate(NEXT->target.lock());
            if (NEXT->target->window())
                g_pCompositor->warpCursorTo(NEXT->target->window()->middle());
        } else if (isPrevStrip) {
            // Move to previous strip
            auto PREV = m_scrollingData->prev(TDATA->column.lock());
            if (!PREV) {
                if (*PNOFALLBACK) {
                    centerOrFit(TDATA->column.lock());
                    m_scrollingData->recalculate();
                    if (TDATA->target->window())
                        g_pCompositor->warpCursorTo(TDATA->target->window()->middle());
                    return {};
                } else
                    PREV = (*PCONFWRAPFOCUS == 1) ? m_scrollingData->columns.back() : m_scrollingData->columns.front();
            }

            auto pTargetData = findBestNeighbor(TDATA, PREV);
            if (pTargetData) {
                focusTargetUpdate(pTargetData->target.lock());
                centerOrFit(PREV);
                m_scrollingData->recalculate();
                if (pTargetData->target->window())
                    g_pCompositor->warpCursorTo(pTargetData->target->window()->middle());
            }
        } else if (isNextStrip) {
            // Move to next strip
            auto NEXT = m_scrollingData->next(TDATA->column.lock());
            if (!NEXT) {
                if (*PNOFALLBACK) {
                    centerOrFit(TDATA->column.lock());
                    m_scrollingData->recalculate();
                    if (TDATA->target->window())
                        g_pCompositor->warpCursorTo(TDATA->target->window()->middle());
                    return {};
                } else
                    NEXT = (*PCONFWRAPFOCUS == 1) ? m_scrollingData->columns.front() : m_scrollingData->columns.back();
            }

            auto pTargetData = findBestNeighbor(TDATA, NEXT);
            if (pTargetData) {
                focusTargetUpdate(pTargetData->target.lock());
                centerOrFit(NEXT);
                m_scrollingData->recalculate();
                if (pTargetData->target->window())
                    g_pCompositor->warpCursorTo(pTargetData->target->window()->middle());
            }
        }
    } else if (ARGS[0] == "promote" || ARGS[0] == "consume" || ARGS[0] == "expel" || ARGS[0] == "consume_or_expel") {
        const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
        if (!TDATA)
            return noTarget("no window focused");

        const auto CURRENT_COL = TDATA->column.lock();
        if (!CURRENT_COL)
            return stateErr("no current col");

        // consume the first target from adjCol into dstCol
        auto consumeTarget = [&](SP<SColumnData> dstCol, SP<SColumnData> adjCol) {
            const auto target = adjCol->targetDatas.front();
            adjCol->remove(target->target.lock());
            dstCol->add(target);
            m_scrollingData->centerOrFitCol(dstCol);
        };

        if (ARGS[0] == "promote") {
            auto idx = m_scrollingData->idx(CURRENT_COL);
            expelTarget(TDATA, CURRENT_COL, idx == -1 ? std::nullopt : std::optional<int64_t>{idx});
        } else if (ARGS[0] == "expel") {
            if (CURRENT_COL->targetDatas.size() < 2)
                return stateErr("column has only one window");

            const auto lastTarget = CURRENT_COL->targetDatas.back();
            const auto currentIdx = m_scrollingData->idx(CURRENT_COL);
            const auto NEXT_COL   = m_scrollingData->next(CURRENT_COL);
            const auto insertIdx  = !NEXT_COL ? std::nullopt : std::optional<int64_t>{currentIdx};

            expelTarget(lastTarget, CURRENT_COL, insertIdx);
        } else if (ARGS[0] == "consume") {
            const auto NEXT_COL = m_scrollingData->next(CURRENT_COL);
            if (!NEXT_COL)
                return notFound("no next column");

            consumeTarget(CURRENT_COL, NEXT_COL);
        } else if (ARGS[0] == "consume_or_expel") {
            if (ARGS.size() < 2)
                return invalidArg("not enough args");

            const std::string& direction = ARGS[1];
            const bool         prev      = direction == "prev";
            const bool         next      = direction == "next";

            if (!prev && !next)
                return invalidArg("invalid direction, expected prev or next");

            if (CURRENT_COL->targetDatas.size() > 1) {
                const auto currentIdx = m_scrollingData->idx(CURRENT_COL);
                expelTarget(TDATA, CURRENT_COL, prev ? currentIdx - 1 : currentIdx);
            } else {
                const auto ADJ_COL = prev ? m_scrollingData->prev(CURRENT_COL) : m_scrollingData->next(CURRENT_COL);
                if (!ADJ_COL)
                    return notFound("no adjacent column");

                CURRENT_COL->remove(TDATA->target.lock());
                ADJ_COL->add(TDATA);
                m_scrollingData->centerOrFitCol(ADJ_COL);
            }
        }

        m_scrollingData->recalculate();
    } else if (ARGS[0] == "swapcol") {
        static const auto PCONFWRAPSWAPCOL = CConfigValue<Config::INTEGER>("scrolling:wrap_swapcol");

        if (ARGS.size() < 2)
            return invalidArg("not enough args");

        const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
        if (!TDATA)
            return noTarget("no window");

        const auto CURRENT_COL = TDATA->column.lock();
        if (!CURRENT_COL)
            return stateErr("no current col");

        if (m_scrollingData->columns.size() < 2)
            return stateErr("not enough columns to swap");

        const int64_t currentIdx = m_scrollingData->idx(CURRENT_COL);
        const size_t  colCount   = m_scrollingData->columns.size();

        if (currentIdx == -1)
            return stateErr("no current column");

        const std::string& direction = ARGS[1];
        int64_t            targetIdx = -1;

        // wrap around swaps
        if (direction == "l")
            if (*PCONFWRAPSWAPCOL == 1)
                targetIdx = (currentIdx == 0) ? (colCount - 1) : (currentIdx - 1);
            else
                targetIdx = (currentIdx == 0) ? 0 : (currentIdx - 1);
        else if (direction == "r")
            if (*PCONFWRAPSWAPCOL == 1)
                targetIdx = (currentIdx == (int64_t)colCount - 1) ? 0 : (currentIdx + 1);
            else
                targetIdx = (currentIdx == (int64_t)colCount - 1) ? (colCount - 1) : (currentIdx + 1);
        else
            return invalidArg("no target (invalid direction?)");
        ;

        std::swap(m_scrollingData->columns.at(currentIdx), m_scrollingData->columns.at(targetIdx));

        m_scrollingData->controller->swapStrips(currentIdx, targetIdx);

        m_scrollingData->centerOrFitCol(CURRENT_COL);
        m_scrollingData->recalculate();
    } else if (ARGS[0] == "center") {
        const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
        if (!TDATA)
            return noTarget("no window");

        const auto CURRENT_COL = TDATA->column.lock();
        if (!CURRENT_COL)
            return stateErr("no current col");

        m_scrollingData->centerCol(CURRENT_COL);
        m_scrollingData->recalculate();
    } else
        return invalidArg("no such layoutmsg for scrolling");

    return {};
}

void CScrollingAlgorithm::moveTape(float delta) {
    if (delta == 0.F)
        return;

    m_scrollingData->controller->adjustOffset(-delta);
    m_scrollingData->recalculate();
}

void CScrollingAlgorithm::moveTapeNormalized(double delta) {
    const double primary = primaryViewportSize();
    if (primary <= 0.0 || delta == 0.0)
        return;

    moveTape(delta * primary);
}

void CScrollingAlgorithm::snapToGrid() {
    snapToProjectedOffset(normalizedTapeOffset());
}

SP<SColumnData> CScrollingAlgorithm::snapToProjectedOffset(double projectedNormalizedOffset) {
    static const auto PFSONONE   = CConfigValue<Config::INTEGER>("scrolling:fullscreen_on_one_column");
    static const auto PFITMETHOD = CConfigValue<Config::INTEGER>("scrolling:focus_fit_method");

    const auto        USABLE     = usableArea();
    auto&             controller = *m_scrollingData->controller;

    if (controller.stripCount() == 0)
        return nullptr;

    const double usablePrimary = controller.isPrimaryHorizontal() ? USABLE.w : USABLE.h;
    if (usablePrimary <= 0.0)
        return nullptr;

    const double maxExtent = controller.calculateMaxExtent(USABLE, *PFSONONE);
    if (maxExtent <= 0.0)
        return nullptr;

    const double projectedOffset = projectedNormalizedOffset * usablePrimary;

    double       bestOffset      = 0.0;
    double       bestDelta       = 0.0;
    double       bestCenterDelta = 0.0;
    size_t       bestIndex       = 0;
    bool         foundSnap       = false;

    auto         centerOffsetFor = [&](size_t index) {
        const double start = controller.calculateStripStart(index, USABLE, *PFSONONE);
        const double size  = controller.calculateStripSize(index, USABLE, *PFSONONE);

        return start - (usablePrimary - size) / 2.0;
    };

    auto fitOffsetFor = [&](size_t index) {
        const double start = controller.calculateStripStart(index, USABLE, *PFSONONE);
        const double size  = controller.calculateStripSize(index, USABLE, *PFSONONE);
        const double lo    = start - usablePrimary + size;
        const double hi    = start;

        if (lo > hi)
            return centerOffsetFor(index);

        const double center = centerOffsetFor(index);
        const double edge   = projectedOffset < center ? lo : hi;

        return std::abs(projectedOffset - center) <= std::abs(projectedOffset - edge) ? center : edge;
    };

    auto considerColumn = [&](size_t index) {
        const double offset         = *PFITMETHOD == 1 ? fitOffsetFor(index) : centerOffsetFor(index);
        const double delta          = std::abs(offset - projectedOffset);
        const double start          = controller.calculateStripStart(index, USABLE, *PFSONONE);
        const double size           = controller.calculateStripSize(index, USABLE, *PFSONONE);
        const double centerDelta    = std::abs((start + size / 2.0) - (projectedOffset + usablePrimary / 2.0));
        const bool   betterFit      = delta < bestDelta;
        const bool   betterTieBreak = delta == bestDelta && centerDelta < bestCenterDelta;

        if (!foundSnap || betterFit || betterTieBreak) {
            bestOffset      = offset;
            bestDelta       = delta;
            bestCenterDelta = centerDelta;
            bestIndex       = index;
            foundSnap       = true;
        }
    };

    for (size_t i = 0; i < controller.stripCount(); ++i)
        considerColumn(i);

    if (!foundSnap)
        return nullptr;

    controller.setOffset(bestOffset);
    m_scrollingData->recalculate();

    if (bestIndex < m_scrollingData->columns.size())
        return m_scrollingData->columns[bestIndex];

    return nullptr;
}

void CScrollingAlgorithm::focusColumn(SP<SColumnData> column) {
    if (!column || column->targetDatas.empty()) {
        focusTargetUpdate(nullptr);
        return;
    }

    auto targetData = column->lastFocusedTarget.lock();

    if (!targetData || targetData->column.lock() != column || !targetData->target || !Desktop::View::validMapped(targetData->target->window())) {
        targetData = nullptr;

        for (const auto& candidate : column->targetDatas) {
            if (candidate->target && Desktop::View::validMapped(candidate->target->window())) {
                targetData = candidate;
                break;
            }
        }
    }

    focusTargetUpdate(targetData ? targetData->target.lock() : nullptr);
}

SP<SColumnData> CScrollingAlgorithm::getColumnAtViewportCenter() {
    return m_scrollingData ? m_scrollingData->atCenter() : nullptr;
}

SP<SColumnData> CScrollingAlgorithm::currentColumn() {
    auto focus = Desktop::focusState()->window();

    if (!focus)
        return nullptr;

    auto data = dataFor(focus->layoutTarget());

    if (!data)
        return nullptr;

    return data->column.lock();
}

double CScrollingAlgorithm::primaryViewportSize() {
    const auto USABLE = usableArea();

    return m_scrollingData->controller->isPrimaryHorizontal() ? USABLE.w : USABLE.h;
}

double CScrollingAlgorithm::normalizedTapeOffset() {
    const double primary = primaryViewportSize();
    if (primary <= 0.0)
        return 0.0;

    return m_scrollingData->controller->getOffset() / primary;
}

std::optional<Vector2D> CScrollingAlgorithm::predictSizeForNewTarget() {
    return std::nullopt;
}

void CScrollingAlgorithm::focusTargetUpdate(SP<ITarget> target) {
    if (!target || !validMapped(target->window())) {
        Desktop::focusState()->fullWindowFocus(nullptr, Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
        return;
    }
    Desktop::focusState()->fullWindowFocus(target->window(), Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
    const auto TARGETDATA = dataFor(target);
    if (TARGETDATA) {
        if (auto col = TARGETDATA->column.lock())
            col->lastFocusedTarget = TARGETDATA;
    }
}

SP<SScrollingTargetData> CScrollingAlgorithm::findBestNeighbor(SP<SScrollingTargetData> pCurrent, SP<SColumnData> pTargetCol) {
    if (!pCurrent || !pTargetCol || pTargetCol->targetDatas.empty())
        return nullptr;

    const double                          currentTop    = pCurrent->layoutBox.y;
    const double                          currentBottom = pCurrent->layoutBox.y + pCurrent->layoutBox.h;
    std::vector<SP<SScrollingTargetData>> overlappingTargets;
    for (const auto& candidate : pTargetCol->targetDatas) {
        const double candidateTop    = candidate->layoutBox.y;
        const double candidateBottom = candidate->layoutBox.y + candidate->layoutBox.h;
        const bool   overlaps        = (candidateTop < currentBottom) && (candidateBottom > currentTop);

        if (overlaps)
            overlappingTargets.emplace_back(candidate);
    }
    if (!overlappingTargets.empty()) {
        auto lastFocused = pTargetCol->lastFocusedTarget.lock();

        if (lastFocused) {
            auto it = std::ranges::find(overlappingTargets, lastFocused);
            if (it != overlappingTargets.end())
                return lastFocused;
        }

        auto topmost = std::ranges::min_element(overlappingTargets, std::less<>{}, [](const SP<SScrollingTargetData>& t) { return t->layoutBox.y; });
        return *topmost;
    }
    if (!pTargetCol->targetDatas.empty())
        return pTargetCol->targetDatas.front();
    return nullptr;
}

SP<SScrollingTargetData> CScrollingAlgorithm::dataFor(SP<ITarget> t) const {
    if (!t)
        return nullptr;

    for (const auto& c : m_scrollingData->columns) {
        for (const auto& d : c->targetDatas) {
            if (d->target == t)
                return d;
        }
    }

    return nullptr;
}

eScrollDirection CScrollingAlgorithm::getDynamicDirection() {
    const auto  WORKSPACERULE = Config::workspaceRuleMgr()->getWorkspaceRuleFor(m_parent->space()->workspace());
    std::string directionString;
    if (WORKSPACERULE && WORKSPACERULE->m_layoutopts.contains("direction"))
        directionString = WORKSPACERULE->m_layoutopts.at("direction");

    static const auto PCONFDIRECTION  = CConfigValue<Config::STRING>("scrolling:direction");
    std::string       configDirection = *PCONFDIRECTION;

    // Workspace rule overrides global config
    if (!directionString.empty())
        configDirection = directionString;

    // Parse direction string
    if (configDirection == "left")
        return SCROLL_DIR_LEFT;
    else if (configDirection == "down")
        return SCROLL_DIR_DOWN;
    else if (configDirection == "up")
        return SCROLL_DIR_UP;
    else
        return SCROLL_DIR_RIGHT; // default
}

CBox CScrollingAlgorithm::usableArea() const {
    if (!m_parent || !m_parent->space())
        return {};

    CBox box = m_parent->space()->workArea();

    // doesn't matter, this happens when this algo is about to be destroyed
    if (!m_parent->space()->workspace() || !m_parent->space()->workspace()->m_monitor)
        return box;

    box.translate(-m_parent->space()->workspace()->m_monitor->m_position);

    // ensure dimensions are never zero or negative, which can happen during
    // monitor transitions (e.g. reconnection after suspend with stale reserved areas)
    box.w = std::max(box.w, 1.0);
    box.h = std::max(box.h, 1.0);

    return box;
}

float CScrollingAlgorithm::defaultColumnWidth() {
    static const auto PCOLWIDTH = CConfigValue<Config::FLOAT>("scrolling:column_width");
    return std::clamp(*PCOLWIDTH, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
}
