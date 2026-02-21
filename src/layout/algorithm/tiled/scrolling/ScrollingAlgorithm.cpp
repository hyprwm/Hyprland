#include "ScrollingAlgorithm.hpp"
#include "ScrollTapeController.hpp"

#include "../../Algorithm.hpp"
#include "../../../space/Space.hpp"
#include "../../../LayoutManager.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../config/ConfigValue.hpp"
#include "../../../../config/ConfigManager.hpp"
#include "../../../../render/Renderer.hpp"
#include "../../../../managers/input/InputManager.hpp"

#include <hyprutils/string/VarList2.hpp>
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
    for (size_t i = 0; i < targetDatas.size(); ++i) {
        if (targetDatas[i]->target->position().y < y)
            continue;
        return i - 1;
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

void SColumnData::up(SP<SScrollingTargetData> w) {
    for (size_t i = 1; i < targetDatas.size(); ++i) {
        if (targetDatas[i] != w)
            continue;

        std::swap(targetDatas[i], targetDatas[i - 1]);
        break;
    }
}

void SColumnData::down(SP<SScrollingTargetData> w) {
    for (size_t i = 0; i < targetDatas.size() - 1; ++i) {
        if (targetDatas[i] != w)
            continue;

        std::swap(targetDatas[i], targetDatas[i + 1]);
        break;
    }
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

SP<SColumnData> SScrollingData::add() {
    static const auto PCOLWIDTH = CConfigValue<Hyprlang::FLOAT>("scrolling:column_width");
    auto              col       = columns.emplace_back(makeShared<SColumnData>(self.lock()));
    col->self                   = col;

    size_t stripIdx                         = controller->addStrip(*PCOLWIDTH);
    controller->getStrip(stripIdx).userData = col;

    return col;
}

SP<SColumnData> SScrollingData::add(int after) {
    static const auto PCOLWIDTH = CConfigValue<Hyprlang::FLOAT>("scrolling:column_width");
    auto              col       = makeShared<SColumnData>(self.lock());
    col->self                   = col;
    columns.insert(columns.begin() + after + 1, col);

    controller->insertStrip(after, *PCOLWIDTH);
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

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();
    int64_t           colIdx   = idx(c);

    if (colIdx >= 0)
        controller->centerStrip(colIdx, USABLE, *PFSONONE);
}

void SScrollingData::fitCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();
    int64_t           colIdx   = idx(c);

    if (colIdx >= 0)
        controller->fitStrip(colIdx, USABLE, *PFSONONE);
}

void SScrollingData::centerOrFitCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("scrolling:focus_fit_method");

    if (*PFITMETHOD == 1)
        fitCol(c);
    else
        centerCol(c);
}

SP<SColumnData> SScrollingData::atCenter() {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();

    size_t            centerIdx = controller->getStripAtCenter(USABLE, *PFSONONE);

    if (centerIdx < columns.size())
        return columns[centerIdx];

    return nullptr;
}

void SScrollingData::recalculate(bool forceInstant) {
    if (algorithm->m_parent->space()->workspace()->m_hasFullscreenWindow)
        return;

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");

    const CBox        USABLE   = algorithm->usableArea();
    const auto        WORKAREA = algorithm->m_parent->space()->workArea();

    controller->setDirection(algorithm->getDynamicDirection());

    for (size_t i = 0; i < columns.size(); ++i) {
        const auto& COL = columns[i];

        for (size_t j = 0; j < COL->targetDatas.size(); ++j) {
            const auto& TARGET = COL->targetDatas[j];

            TARGET->layoutBox = controller->calculateTargetBox(i, j, USABLE, WORKAREA.pos(), *PFSONONE);

            if (TARGET->target)
                TARGET->target->setPositionGlobal(TARGET->layoutBox);
            if (forceInstant && TARGET->target)
                TARGET->target->warpPositionSize();
        }
    }
}

double SScrollingData::maxWidth() {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();

    return controller->calculateMaxExtent(USABLE, *PFSONONE);
}

bool SScrollingData::visible(SP<SColumnData> c) {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");
    const auto        USABLE   = algorithm->usableArea();
    int64_t           colIdx   = idx(c);

    if (colIdx >= 0)
        return controller->isStripVisible(colIdx, USABLE, *PFSONONE);

    return false;
}

CScrollingAlgorithm::CScrollingAlgorithm() {
    static const auto PCONFWIDTHS    = CConfigValue<Hyprlang::STRING>("scrolling:explicit_column_widths");
    static const auto PCONFDIRECTION = CConfigValue<Hyprlang::STRING>("scrolling:direction");

    m_scrollingData       = makeShared<SScrollingData>(this);
    m_scrollingData->self = m_scrollingData;

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

    m_configCallback = g_pHookSystem->hookDynamic("configReloaded", [this, parseDirection](void* hk, SCallbackInfo& info, std::any param) {
        static const auto PCONFDIRECTION = CConfigValue<Hyprlang::STRING>("scrolling:direction");

        m_config.configuredWidths.clear();

        CConstVarList widths(*PCONFWIDTHS, 0, ',');
        for (auto& w : widths) {
            try {
                m_config.configuredWidths.emplace_back(std::stof(std::string{w}));
            } catch (...) { Log::logger->log(Log::ERR, "scrolling: Failed to parse width {} as float", w); }
        }
        if (m_config.configuredWidths.empty())
            m_config.configuredWidths = {0.333, 0.5, 0.667, 1.0};

        // Update scroll direction
        m_scrollingData->controller->setDirection(parseDirection(*PCONFDIRECTION));
    });

    m_mouseButtonCallback = g_pHookSystem->hookDynamic("mouseButton", [this](void* self, SCallbackInfo& info, std::any e) {
        auto E = std::any_cast<IPointer::SButtonEvent>(e);
        if (E.state == WL_POINTER_BUTTON_STATE_RELEASED && Desktop::focusState()->window())
            focusOnInput(Desktop::focusState()->window()->layoutTarget(), true);
    });

    m_focusCallback = g_pHookSystem->hookDynamic("activeWindow", [this](void* hk, SCallbackInfo& info, std::any param) {
        const auto E       = std::any_cast<Desktop::View::SWindowActiveEvent>(param);
        const auto PWINDOW = E.window;

        if (!PWINDOW)
            return;

        static const auto PFOLLOW_FOCUS = CConfigValue<Hyprlang::INT>("scrolling:follow_focus");

        if (!*PFOLLOW_FOCUS && !Desktop::isHardInputFocusReason(E.reason))
            return;

        if (PWINDOW->m_workspace != m_parent->space()->workspace())
            return;

        const auto TARGET = PWINDOW->layoutTarget();
        if (!TARGET || TARGET->floating())
            return;

        focusOnInput(TARGET, Desktop::isHardInputFocusReason(E.reason));
    });

    // Initialize default widths and direction
    m_config.configuredWidths = {0.333, 0.5, 0.667, 1.0};
    m_scrollingData->controller->setDirection(parseDirection(*PCONFDIRECTION));
}

CScrollingAlgorithm::~CScrollingAlgorithm() {
    m_configCallback.reset();
    m_focusCallback.reset();
}

void CScrollingAlgorithm::focusOnInput(SP<ITarget> target, bool hardInput) {
    static const auto PFOLLOW_FOCUS_MIN_PERC = CConfigValue<Hyprlang::FLOAT>("scrolling:follow_min_visible");

    if (!target || target->space() != m_parent->space())
        return;

    const auto TARGETDATA = dataFor(target);
    if (!TARGETDATA)
        return;

    if (*PFOLLOW_FOCUS_MIN_PERC > 0.F && !hardInput) {
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

    static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("scrolling:focus_fit_method");
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

    if (!droppingColumn) {
        auto col = m_scrollingData->add();
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
            auto col = idx == -1 ? m_scrollingData->add() : m_scrollingData->add(idx);
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

    if (!m_scrollingData->next(DATA->column.lock()) && DATA->column->targetDatas.size() <= 1) {
        // move the view if this is the last column
        const auto USABLE = usableArea();
        m_scrollingData->controller->adjustOffset(-(USABLE.w * DATA->column->getColumnWidth()));
    }

    DATA->column->remove(target);

    if (!DATA->column) {
        // column got removed, let's ensure we don't leave any cringe extra space
        const auto USABLE    = usableArea();
        double     newOffset = std::clamp(m_scrollingData->controller->getOffset(), 0.0, std::max(m_scrollingData->maxWidth() - USABLE.w, 1.0));
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

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");

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

void CScrollingAlgorithm::recalculate() {
    if (Desktop::focusState()->window())
        focusOnInput(Desktop::focusState()->window()->layoutTarget(), true);

    m_scrollingData->recalculate();
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
    const auto DATA = dataFor(t);

    if (!DATA)
        return;

    const auto TAPE_DIR    = getDynamicDirection();
    const auto CURRENT_COL = DATA->column.lock();
    const auto current_idx = m_scrollingData->idx(CURRENT_COL);

    if (dir == Math::DIRECTION_LEFT) {
        const auto COL = m_scrollingData->prev(DATA->column.lock());

        // ignore moves to the "origin" when on first column and moving opposite to tape direction
        if (!COL && current_idx == 0 && (TAPE_DIR == SCROLL_DIR_RIGHT || TAPE_DIR == SCROLL_DIR_DOWN))
            return;

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
    } else if (dir == Math::DIRECTION_RIGHT) {
        const auto COL = m_scrollingData->next(DATA->column.lock());

        // ignore moves to the "origin" when on last column and moving opposite to tape direction
        if (!COL && current_idx == (int64_t)m_scrollingData->columns.size() - 1 && (TAPE_DIR == SCROLL_DIR_LEFT || TAPE_DIR == SCROLL_DIR_UP))
            return;

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

    } else if (dir == Math::DIRECTION_UP)
        DATA->column->up(DATA);
    else if (dir == Math::DIRECTION_DOWN)
        DATA->column->down(DATA);

    m_scrollingData->recalculate();
    focusTargetUpdate(t);
    if (t->window())
        g_pCompositor->warpCursorTo(t->window()->middle());
}

std::expected<void, std::string> CScrollingAlgorithm::layoutMsg(const std::string_view& sv) {
    auto centerOrFit = [this](const SP<SColumnData> COL) -> void {
        static const auto PFITMETHOD = CConfigValue<Hyprlang::INT>("scrolling:focus_fit_method");
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
                return std::unexpected("no window");

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
            return std::unexpected("failed to parse offset");

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
            return std::unexpected("no focused window");

        const auto WDATA = dataFor(PWINDOW->layoutTarget());

        if (!WDATA || m_scrollingData->columns.size() == 0)
            return std::unexpected("can't fit: no window or columns");

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
                return std::unexpected("couldn't find beginning");

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
        const auto        TDATA       = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
        static const auto PNOFALLBACK = CConfigValue<Hyprlang::INT>("general:no_focus_fallback");

        if (!TDATA || ARGS[1].empty())
            return std::unexpected("no window to focus");

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
                    return std::unexpected("fallback disabled (no target)");
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
                    return std::unexpected("fallback disabled (no target)");
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
                    PREV = m_scrollingData->columns.back();
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
                    NEXT = m_scrollingData->columns.front();
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
    } else if (ARGS[0] == "promote") {
        const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);

        if (!TDATA)
            return std::unexpected("no window focused");

        auto idx = m_scrollingData->idx(TDATA->column.lock());
        auto col = idx == -1 ? m_scrollingData->add() : m_scrollingData->add(idx);

        TDATA->column->remove(TDATA->target.lock());

        col->add(TDATA);

        m_scrollingData->recalculate();
    } else if (ARGS[0] == "swapcol") {
        if (ARGS.size() < 2)
            return std::unexpected("not enough args");

        const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
        if (!TDATA)
            return std::unexpected("no window");

        const auto CURRENT_COL = TDATA->column.lock();
        if (!CURRENT_COL)
            return std::unexpected("no current col");

        if (m_scrollingData->columns.size() < 2)
            return std::unexpected("not enough columns to swap");

        const int64_t currentIdx = m_scrollingData->idx(CURRENT_COL);
        const size_t  colCount   = m_scrollingData->columns.size();

        if (currentIdx == -1)
            return std::unexpected("no current column");

        const std::string& direction = ARGS[1];
        int64_t            targetIdx = -1;

        // wrap around swaps
        if (direction == "l")
            targetIdx = (currentIdx == 0) ? (colCount - 1) : (currentIdx - 1);
        else if (direction == "r")
            targetIdx = (currentIdx == (int64_t)colCount - 1) ? 0 : (currentIdx + 1);
        else
            return std::unexpected("no target (invalid direction?)");
        ;

        std::swap(m_scrollingData->columns.at(currentIdx), m_scrollingData->columns.at(targetIdx));

        m_scrollingData->controller->swapStrips(currentIdx, targetIdx);

        m_scrollingData->centerOrFitCol(CURRENT_COL);
        m_scrollingData->recalculate();
    } else
        return std::unexpected("no such layoutmsg for scrolling");

    return {};
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

SP<SScrollingTargetData> CScrollingAlgorithm::dataFor(SP<ITarget> t) {
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
    const auto  WORKSPACERULE = g_pConfigManager->getWorkspaceRuleFor(m_parent->space()->workspace());
    std::string directionString;
    if (WORKSPACERULE.layoutopts.contains("direction"))
        directionString = WORKSPACERULE.layoutopts.at("direction");

    static const auto PCONFDIRECTION  = CConfigValue<Hyprlang::STRING>("scrolling:direction");
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

CBox CScrollingAlgorithm::usableArea() {
    CBox box = m_parent->space()->workArea();
    box.translate(-m_parent->space()->workspace()->m_monitor->m_position);
    return box;
}
