#include "ScrollingAlgorithm.hpp"

#include "../../Algorithm.hpp"
#include "../../../space/Space.hpp"
#include "../../../LayoutManager.hpp"

#include "../../../../Compositor.hpp"
#include "../../../../desktop/state/FocusState.hpp"
#include "../../../../config/ConfigValue.hpp"
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
void SColumnData::add(SP<ITarget> t) {
    for (auto& td : targetDatas) {
        td->windowSize *= (float)targetDatas.size() / (float)(targetDatas.size() + 1);
    }

    targetDatas.emplace_back(makeShared<SScrollingTargetData>(t, self.lock(), 1.F / (float)(targetDatas.size() + 1)));
}

void SColumnData::add(SP<ITarget> t, int after) {
    for (auto& td : targetDatas) {
        td->windowSize *= (float)targetDatas.size() / (float)(targetDatas.size() + 1);
    }

    targetDatas.insert(targetDatas.begin() + after + 1, makeShared<SScrollingTargetData>(t, self.lock(), 1.F / (float)(targetDatas.size() + 1)));
}

void SColumnData::add(SP<SScrollingTargetData> w) {
    for (auto& td : targetDatas) {
        td->windowSize *= (float)targetDatas.size() / (float)(targetDatas.size() + 1);
    }

    targetDatas.emplace_back(w);
    w->column     = self;
    w->windowSize = 1.F / (float)(targetDatas.size());
}

void SColumnData::add(SP<SScrollingTargetData> w, int after) {
    for (auto& td : targetDatas) {
        td->windowSize *= (float)targetDatas.size() / (float)(targetDatas.size() + 1);
    }

    targetDatas.insert(targetDatas.begin() + after + 1, w);
    w->column     = self;
    w->windowSize = 1.F / (float)(targetDatas.size());
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
    std::erase_if(targetDatas, [&t](const auto& e) { return e->target == t; });

    if (SIZE_BEFORE == targetDatas.size() && SIZE_BEFORE > 0)
        return;

    float newMaxSize = 0.F;
    for (auto& td : targetDatas) {
        newMaxSize += td->windowSize;
    }

    for (auto& td : targetDatas) {
        td->windowSize *= 1.F / newMaxSize;
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

SP<SColumnData> SScrollingData::add() {
    static const auto PCOLWIDTH = CConfigValue<Hyprlang::FLOAT>("scrolling:column_width");
    auto              col       = columns.emplace_back(makeShared<SColumnData>(self.lock()));
    col->self                   = col;
    col->columnWidth            = *PCOLWIDTH;
    return col;
}

SP<SColumnData> SScrollingData::add(int after) {
    static const auto PCOLWIDTH = CConfigValue<Hyprlang::FLOAT>("scrolling:column_width");
    auto              col       = makeShared<SColumnData>(self.lock());
    col->self                   = col;
    col->columnWidth            = *PCOLWIDTH;
    columns.insert(columns.begin() + after + 1, col);
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
    std::erase(columns, c);
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

    const auto        USABLE      = algorithm->usableArea();
    double            currentLeft = 0;

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        if (COL != c)
            currentLeft += ITEM_WIDTH;
        else {
            leftOffset = currentLeft - (USABLE.w - ITEM_WIDTH) / 2.F;
            return;
        }
    }
}

void SScrollingData::fitCol(SP<SColumnData> c) {
    if (!c)
        return;

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");

    const auto        USABLE      = algorithm->usableArea();
    double            currentLeft = 0;

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        if (COL != c)
            currentLeft += ITEM_WIDTH;
        else {
            leftOffset = std::clamp((double)leftOffset, currentLeft - USABLE.w + ITEM_WIDTH, currentLeft);
            return;
        }
    }
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

    const auto        USABLE      = algorithm->usableArea();
    double            currentLeft = leftOffset;

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        currentLeft += ITEM_WIDTH;

        if (currentLeft >= USABLE.w / 2.0 - 2)
            return COL;
    }

    return nullptr;
}

void SScrollingData::recalculate(bool forceInstant) {
    if (algorithm->m_parent->space()->workspace()->m_hasFullscreenWindow)
        return;

    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");

    const auto        MAX_WIDTH = maxWidth();
    const CBox        USABLE    = algorithm->usableArea();
    const auto        WORKAREA  = algorithm->m_parent->space()->workArea();

    double            currentLeft = 0;
    const double      cameraLeft  = MAX_WIDTH < USABLE.w ? std::round((MAX_WIDTH - USABLE.w) / 2.0) : leftOffset;

    for (size_t i = 0; i < columns.size(); ++i) {
        const auto&  COL        = columns[i];
        double       currentTop = 0.0;
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        for (const auto& TARGET : COL->targetDatas) {
            TARGET->layoutBox = CBox{currentLeft, currentTop, ITEM_WIDTH, TARGET->windowSize * USABLE.h}.translate(WORKAREA.pos() + Vector2D{-cameraLeft, 0.0});

            currentTop += TARGET->windowSize * USABLE.h;

            if (TARGET->target)
                TARGET->target->setPositionGlobal(TARGET->layoutBox);
            if (forceInstant && TARGET->target)
                TARGET->target->warpPositionSize();
        }

        currentLeft += ITEM_WIDTH;
        if (currentLeft == USABLE.width)
            currentLeft++; // avoid ffm from "grabbing" the target on the right
    }
}

double SScrollingData::maxWidth() {
    static const auto PFSONONE = CConfigValue<Hyprlang::INT>("scrolling:fullscreen_on_one_column");

    const auto        USABLE      = algorithm->usableArea();
    double            currentLeft = 0;

    for (const auto& COL : columns) {
        const double ITEM_WIDTH = *PFSONONE && columns.size() == 1 ? USABLE.w : USABLE.w * COL->columnWidth;

        currentLeft += ITEM_WIDTH;
    }

    return currentLeft;
}

bool SScrollingData::visible(SP<SColumnData> c) {
    const auto USABLE    = algorithm->usableArea();
    float      totalLeft = 0;
    for (const auto& col : columns) {
        if (col == c) {
            const float colLeft   = totalLeft;
            const float colRight  = totalLeft + col->columnWidth * USABLE.w;
            const float viewLeft  = leftOffset;
            const float viewRight = leftOffset + USABLE.w;
            return colLeft < viewRight && viewLeft < colRight;
        }

        totalLeft += col->columnWidth * USABLE.w;
    }

    return false;
}

CScrollingAlgorithm::CScrollingAlgorithm() {
    static const auto PCONFWIDTHS = CConfigValue<Hyprlang::STRING>("scrolling:explicit_column_widths");

    m_scrollingData       = makeShared<SScrollingData>(this);
    m_scrollingData->self = m_scrollingData;

    m_configCallback = g_pHookSystem->hookDynamic("configReloaded", [this](void* hk, SCallbackInfo& info, std::any param) {
        m_config.configuredWidths.clear();

        CConstVarList widths(*PCONFWIDTHS, 0, ',');
        for (auto& w : widths) {
            try {
                m_config.configuredWidths.emplace_back(std::stof(std::string{w}));
            } catch (...) { Log::logger->log(Log::ERR, "scrolling: Failed to parse width {} as float", w); }
        }
        if (m_config.configuredWidths.empty())
            m_config.configuredWidths = {0.333, 0.5, 0.667, 1.0};
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

    // Initialize default widths
    m_config.configuredWidths = {0.333, 0.5, 0.667, 1.0};
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

        const auto   MON_BOX    = m_parent->space()->workspace()->m_monitor->logicalBox();
        const auto   TARGET_POS = target->position();
        const double VISIBLE_X  = std::abs(std::min(MON_BOX.x + MON_BOX.w, TARGET_POS.x + TARGET_POS.w) - (std::max(MON_BOX.x, TARGET_POS.x)));

        // if the amount of visible X is below minimum, reject
        if (VISIBLE_X < MON_BOX.x * std::clamp(*PFOLLOW_FOCUS_MIN_PERC, 0.F, 1.F))
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

    if (!m_scrollingData->next(DATA->column.lock())) {
        // move the view if this is the last column
        const auto USABLE = usableArea();
        m_scrollingData->leftOffset -= USABLE.w * DATA->column->columnWidth;
    }

    DATA->column->remove(target);

    m_scrollingData->recalculate();

    if (!DATA->column) {
        // column got removed, let's ensure we don't leave any cringe extra space
        const auto USABLE           = usableArea();
        m_scrollingData->leftOffset = std::clamp((double)m_scrollingData->leftOffset, 0.0, std::max(m_scrollingData->maxWidth() - USABLE.w, 1.0));
    }
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

    const auto USABLE        = usableArea();
    const auto DELTA_AS_PERC = delta / USABLE.size();
    Vector2D   modDelta      = delta;

    const auto CURR_COLUMN = DATA->column.lock();
    const auto NEXT_COLUMN = m_scrollingData->next(CURR_COLUMN);
    const auto PREV_COLUMN = m_scrollingData->prev(CURR_COLUMN);

    switch (corner) {
        case CORNER_BOTTOMLEFT:
        case CORNER_TOPLEFT: {
            if (!PREV_COLUMN)
                break;

            PREV_COLUMN->columnWidth = std::clamp(PREV_COLUMN->columnWidth + (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            CURR_COLUMN->columnWidth = std::clamp(CURR_COLUMN->columnWidth - (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            break;
        }
        case CORNER_BOTTOMRIGHT:
        case CORNER_TOPRIGHT: {
            if (!NEXT_COLUMN)
                break;

            NEXT_COLUMN->columnWidth = std::clamp(NEXT_COLUMN->columnWidth - (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            CURR_COLUMN->columnWidth = std::clamp(CURR_COLUMN->columnWidth + (float)DELTA_AS_PERC.x, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            break;
        }

        default: break;
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

                if (NEXT_TD->windowSize <= MIN_ROW_HEIGHT && delta.y >= 0)
                    break;

                float adjust = std::clamp((float)(delta.y / USABLE.h), (-CURR_TD->windowSize + MIN_ROW_HEIGHT), (NEXT_TD->windowSize - MIN_ROW_HEIGHT));

                NEXT_TD->windowSize = std::clamp(NEXT_TD->windowSize - adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                CURR_TD->windowSize = std::clamp(CURR_TD->windowSize + adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                break;
            }
            case CORNER_TOPLEFT:
            case CORNER_TOPRIGHT: {
                if (!PREV_TD)
                    break;

                if ((PREV_TD->windowSize <= MIN_ROW_HEIGHT && modDelta.y <= 0) || (CURR_TD->windowSize <= MIN_ROW_HEIGHT && delta.y >= 0))
                    break;

                float adjust = std::clamp((float)(modDelta.y / USABLE.h), -(PREV_TD->windowSize - MIN_ROW_HEIGHT), (CURR_TD->windowSize - MIN_ROW_HEIGHT));

                PREV_TD->windowSize = std::clamp(PREV_TD->windowSize + adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                CURR_TD->windowSize = std::clamp(CURR_TD->windowSize - adjust, MIN_ROW_HEIGHT, MAX_ROW_HEIGHT);
                break;
            }

            default: break;
        }
    }

    m_scrollingData->recalculate(true);
}

void CScrollingAlgorithm::recalculate() {
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

    if (dir == Math::DIRECTION_LEFT) {
        const auto COL = m_scrollingData->prev(DATA->column.lock());

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
                return {};

            const auto COL = m_scrollingData->next(TDATA->column.lock());
            if (!COL) {
                // move to max
                m_scrollingData->leftOffset = m_scrollingData->maxWidth();
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
            return {};

        m_scrollingData->leftOffset -= *PLUSMINUS;
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
                c->columnWidth = abs;
            }

            m_scrollingData->recalculate();
            return {};
        }

        CScopeGuard x([this, TDATA] {
            TDATA->column->columnWidth = std::clamp(TDATA->column->columnWidth, MIN_COLUMN_WIDTH, MAX_COLUMN_WIDTH);
            m_scrollingData->centerOrFitCol(TDATA->column.lock());
            m_scrollingData->recalculate();
        });

        if (ARGS[1][0] == '+' || ARGS[1][0] == '-') {
            if (ARGS[1] == "+conf") {
                for (size_t i = 0; i < m_config.configuredWidths.size(); ++i) {
                    if (m_config.configuredWidths[i] > TDATA->column->columnWidth) {
                        TDATA->column->columnWidth = m_config.configuredWidths[i];
                        break;
                    }

                    if (i == m_config.configuredWidths.size() - 1)
                        TDATA->column->columnWidth = m_config.configuredWidths[0];
                }

                return {};
            } else if (ARGS[1] == "-conf") {
                for (size_t i = m_config.configuredWidths.size() - 1;; --i) {
                    if (m_config.configuredWidths[i] < TDATA->column->columnWidth) {
                        TDATA->column->columnWidth = m_config.configuredWidths[i];
                        break;
                    }

                    if (i == 0) {
                        TDATA->column->columnWidth = m_config.configuredWidths.back();
                        break;
                    }
                }

                return {};
            }

            const auto PLUSMINUS = getPlusMinusKeywordResult(ARGS[1], 0);

            if (!PLUSMINUS.has_value())
                return {};

            TDATA->column->columnWidth += *PLUSMINUS;
        } else {
            float abs = 0;
            try {
                abs = std::stof(ARGS[1]);
            } catch (...) { return {}; }

            TDATA->column->columnWidth = abs;
        }
    } else if (ARGS[0] == "movewindowto") {
        if (ARGS[1].empty())
            return std::unexpected("no dir");

        const auto TARGET = Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr;
        if (TARGET)
            moveTargetTo(TARGET, Math::fromChar(ARGS[1][0]), false);
    } else if (ARGS[0] == "focus") {
        const auto        TDATA       = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
        static const auto PNOFALLBACK = CConfigValue<Hyprlang::INT>("general:no_focus_fallback");

        if (!TDATA || ARGS[1].empty())
            return {};

        switch (ARGS[1][0]) {
            case 'u':
            case 't': {
                auto PREV = TDATA->column->prev(TDATA);
                if (!PREV) {
                    if (*PNOFALLBACK)
                        break;
                    else
                        PREV = TDATA->column->targetDatas.back();
                }

                focusTargetUpdate(PREV->target.lock());
                if (PREV->target->window())
                    g_pCompositor->warpCursorTo(PREV->target->window()->middle());
                break;
            }

            case 'b':
            case 'd': {
                auto NEXT = TDATA->column->next(TDATA);
                if (!NEXT) {
                    if (*PNOFALLBACK)
                        break;
                    else
                        NEXT = TDATA->column->targetDatas.front();
                }

                focusTargetUpdate(NEXT->target.lock());
                if (NEXT->target->window())
                    g_pCompositor->warpCursorTo(NEXT->target->window()->middle());
                break;
            }

            case 'l': {
                auto PREV = m_scrollingData->prev(TDATA->column.lock());
                if (!PREV) {
                    if (*PNOFALLBACK) {
                        centerOrFit(TDATA->column.lock());
                        m_scrollingData->recalculate();
                        if (TDATA->target->window())
                            g_pCompositor->warpCursorTo(TDATA->target->window()->middle());
                        break;
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
                break;
            }

            case 'r': {
                auto NEXT = m_scrollingData->next(TDATA->column.lock());
                if (!NEXT) {
                    if (*PNOFALLBACK) {
                        centerOrFit(TDATA->column.lock());
                        m_scrollingData->recalculate();
                        if (TDATA->target->window())
                            g_pCompositor->warpCursorTo(TDATA->target->window()->middle());
                        break;
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
                break;
            }

            default: return {};
        }
    } else if (ARGS[0] == "promote") {
        const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);

        if (!TDATA)
            return {};

        auto idx = m_scrollingData->idx(TDATA->column.lock());
        auto col = idx == -1 ? m_scrollingData->add() : m_scrollingData->add(idx);

        TDATA->column->remove(TDATA->target.lock());

        col->add(TDATA);

        m_scrollingData->recalculate();
    } else if (ARGS[0] == "swapcol") {
        if (ARGS.size() < 2)
            return {};

        const auto TDATA = dataFor(Desktop::focusState()->window() ? Desktop::focusState()->window()->layoutTarget() : nullptr);
        if (!TDATA)
            return {};

        const auto CURRENT_COL = TDATA->column.lock();
        if (!CURRENT_COL)
            return {};

        if (m_scrollingData->columns.size() < 2)
            return {};

        const int64_t current_idx = m_scrollingData->idx(CURRENT_COL);
        const size_t  col_count   = m_scrollingData->columns.size();

        if (current_idx == -1)
            return {};

        const std::string& direction  = ARGS[1];
        int64_t            target_idx = -1;

        if (direction == "l")
            target_idx = (current_idx == 0) ? (col_count - 1) : (current_idx - 1);
        else if (direction == "r")
            target_idx = (current_idx == (int64_t)col_count - 1) ? 0 : (current_idx + 1);
        else
            return {};

        std::swap(m_scrollingData->columns[current_idx], m_scrollingData->columns[target_idx]);
        m_scrollingData->centerOrFitCol(CURRENT_COL);
        m_scrollingData->recalculate();
    }

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

CBox CScrollingAlgorithm::usableArea() {
    CBox box = m_parent->space()->workArea();
    box.translate(-m_parent->space()->workspace()->m_monitor->m_position);
    return box;
}
