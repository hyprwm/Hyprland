#include "DecorationPositioner.hpp"
#include "../../desktop/Window.hpp"
#include "../../managers/HookSystemManager.hpp"
#include "../../managers/LayoutManager.hpp"

CDecorationPositioner::CDecorationPositioner() {
    static auto P = g_pHookSystem->hookDynamic("closeWindow", [this](void* call, SCallbackInfo& info, std::any data) {
        auto PWINDOW = std::any_cast<PHLWINDOW>(data);
        this->onWindowUnmap(PWINDOW);
    });

    static auto P2 = g_pHookSystem->hookDynamic("openWindow", [this](void* call, SCallbackInfo& info, std::any data) {
        auto PWINDOW = std::any_cast<PHLWINDOW>(data);
        this->onWindowMap(PWINDOW);
    });
}

Vector2D CDecorationPositioner::getEdgeDefinedPoint(uint32_t edges, PHLWINDOW pWindow) {
    const bool TOP    = edges & DECORATION_EDGE_TOP;
    const bool BOTTOM = edges & DECORATION_EDGE_BOTTOM;
    const bool LEFT   = edges & DECORATION_EDGE_LEFT;
    const bool RIGHT  = edges & DECORATION_EDGE_RIGHT;

    const int  EDGESNO = TOP + BOTTOM + LEFT + RIGHT;

    if (EDGESNO == 0 || EDGESNO == 3 || EDGESNO > 4) {
        NDebug::log(ERR, "getEdgeDefinedPoint: invalid number of edges");
        return {};
    }

    CBox wb = pWindow->getWindowMainSurfaceBox();

    if (EDGESNO == 4)
        return wb.pos();

    if (EDGESNO == 1) {
        if (TOP)
            return wb.pos() + Vector2D{wb.size().x / 2.0, 0.0};
        else if (BOTTOM)
            return wb.pos() + Vector2D{wb.size().x / 2.0, wb.size().y};
        else if (LEFT)
            return wb.pos() + Vector2D{0.0, wb.size().y / 2.0};
        else if (RIGHT)
            return wb.pos() + Vector2D{wb.size().x, wb.size().y / 2.0};
        UNREACHABLE();
    } else {
        if (TOP && LEFT)
            return wb.pos();
        if (TOP && RIGHT)
            return wb.pos() + Vector2D{wb.size().x, 0.0};
        if (BOTTOM && RIGHT)
            return wb.pos() + wb.size();
        if (BOTTOM && LEFT)
            return wb.pos() + Vector2D{0.0, wb.size().y};
        UNREACHABLE();
    }
    UNREACHABLE();
    return {};
}

void CDecorationPositioner::uncacheDecoration(IHyprWindowDecoration* deco) {
    std::erase_if(m_vWindowPositioningDatas, [&](const auto& data) { return !data->pWindow.lock() || data->pDecoration == deco; });

    const auto WIT = std::find_if(m_mWindowDatas.begin(), m_mWindowDatas.end(), [&](const auto& other) { return other.first.lock() == deco->m_pWindow.lock(); });
    if (WIT == m_mWindowDatas.end())
        return;

    WIT->second.needsRecalc = true;
}

void CDecorationPositioner::repositionDeco(IHyprWindowDecoration* deco) {
    uncacheDecoration(deco);
    onWindowUpdate(deco->m_pWindow.lock());
}

CDecorationPositioner::SWindowPositioningData* CDecorationPositioner::getDataFor(IHyprWindowDecoration* pDecoration, PHLWINDOW pWindow) {
    auto it = std::find_if(m_vWindowPositioningDatas.begin(), m_vWindowPositioningDatas.end(), [&](const auto& el) { return el->pDecoration == pDecoration; });

    if (it != m_vWindowPositioningDatas.end())
        return it->get();

    const auto DATA = m_vWindowPositioningDatas.emplace_back(makeUnique<CDecorationPositioner::SWindowPositioningData>(pWindow, pDecoration)).get();

    DATA->positioningInfo = pDecoration->getPositioningInfo();

    return DATA;
}

void CDecorationPositioner::sanitizeDatas() {
    std::erase_if(m_mWindowDatas, [](const auto& other) { return !valid(other.first); });
    std::erase_if(m_vWindowPositioningDatas, [](const auto& other) {
        if (!validMapped(other->pWindow))
            return true;
        if (std::find_if(other->pWindow->m_dWindowDecorations.begin(), other->pWindow->m_dWindowDecorations.end(),
                         [&](const auto& el) { return el.get() == other->pDecoration; }) == other->pWindow->m_dWindowDecorations.end())
            return true;
        return false;
    });
}

void CDecorationPositioner::forceRecalcFor(PHLWINDOW pWindow) {
    const auto WIT = std::find_if(m_mWindowDatas.begin(), m_mWindowDatas.end(), [&](const auto& other) { return other.first.lock() == pWindow; });
    if (WIT == m_mWindowDatas.end())
        return;

    const auto WINDOWDATA = &WIT->second;

    WINDOWDATA->needsRecalc = true;
}

void CDecorationPositioner::onWindowUpdate(PHLWINDOW pWindow) {
    if (!validMapped(pWindow))
        return;

    const auto WIT = std::find_if(m_mWindowDatas.begin(), m_mWindowDatas.end(), [&](const auto& other) { return other.first.lock() == pWindow; });
    if (WIT == m_mWindowDatas.end())
        return;

    const auto WINDOWDATA = &WIT->second;

    sanitizeDatas();

    //
    std::vector<CDecorationPositioner::SWindowPositioningData*> datas;
    // reserve to avoid reallocations
    datas.reserve(pWindow->m_dWindowDecorations.size());

    for (auto const& wd : pWindow->m_dWindowDecorations) {
        datas.push_back(getDataFor(wd.get(), pWindow));
    }

    if (WINDOWDATA->lastWindowSize == pWindow->m_vRealSize->value() /* position not changed */
        && std::all_of(m_vWindowPositioningDatas.begin(), m_vWindowPositioningDatas.end(),
                       [pWindow](const auto& data) { return pWindow != data->pWindow.lock() || !data->needsReposition; })
        /* all window datas are either not for this window or don't need a reposition */
        && !WINDOWDATA->needsRecalc /* window doesn't need recalc */
    )
        return;

    WINDOWDATA->lastWindowSize = pWindow->m_vRealSize->value();
    WINDOWDATA->needsRecalc    = false;
    const bool EPHEMERAL       = pWindow->m_vRealSize->isBeingAnimated();

    std::sort(datas.begin(), datas.end(), [](const auto& a, const auto& b) { return a->positioningInfo.priority > b->positioningInfo.priority; });

    CBox wb = pWindow->getWindowMainSurfaceBox();

    // calc reserved
    float reservedXL = 0, reservedYT = 0, reservedXR = 0, reservedYB = 0;
    for (size_t i = 0; i < datas.size(); ++i) {
        auto* const wd = datas[i];

        if (!wd->positioningInfo.reserved)
            continue;

        const bool TOP    = wd->positioningInfo.edges & DECORATION_EDGE_TOP;
        const bool BOTTOM = wd->positioningInfo.edges & DECORATION_EDGE_BOTTOM;
        const bool LEFT   = wd->positioningInfo.edges & DECORATION_EDGE_LEFT;
        const bool RIGHT  = wd->positioningInfo.edges & DECORATION_EDGE_RIGHT;

        if (LEFT)
            reservedXL += wd->positioningInfo.desiredExtents.topLeft.x;
        if (RIGHT)
            reservedXR += wd->positioningInfo.desiredExtents.bottomRight.x;
        if (TOP)
            reservedYT += wd->positioningInfo.desiredExtents.topLeft.y;
        if (BOTTOM)
            reservedYB += wd->positioningInfo.desiredExtents.bottomRight.y;
    }

    WINDOWDATA->reserved = {{reservedXL, reservedYT}, {reservedXR, reservedYB}};

    float stickyOffsetXL = 0, stickyOffsetYT = 0, stickyOffsetXR = 0, stickyOffsetYB = 0;

    for (size_t i = 0; i < datas.size(); ++i) {
        auto* const wd = datas[i];

        wd->needsReposition = false;

        const bool TOP     = wd->positioningInfo.edges & DECORATION_EDGE_TOP;
        const bool BOTTOM  = wd->positioningInfo.edges & DECORATION_EDGE_BOTTOM;
        const bool LEFT    = wd->positioningInfo.edges & DECORATION_EDGE_LEFT;
        const bool RIGHT   = wd->positioningInfo.edges & DECORATION_EDGE_RIGHT;
        const int  EDGESNO = TOP + BOTTOM + LEFT + RIGHT;
        const bool SOLID   = !(wd->pDecoration->getDecorationFlags() & DECORATION_NON_SOLID);

        if (wd->positioningInfo.policy == DECORATION_POSITION_ABSOLUTE) {

            if (SOLID) {
                if (LEFT)
                    stickyOffsetXL += wd->positioningInfo.desiredExtents.topLeft.x;
                if (RIGHT)
                    stickyOffsetXR += wd->positioningInfo.desiredExtents.bottomRight.x;
                if (TOP)
                    stickyOffsetYT += wd->positioningInfo.desiredExtents.topLeft.y;
                if (BOTTOM)
                    stickyOffsetYB += wd->positioningInfo.desiredExtents.bottomRight.y;
            }

            wd->lastReply = {};
            wd->pDecoration->onPositioningReply({});
            continue;
        }

        if (wd->positioningInfo.policy == DECORATION_POSITION_STICKY) {
            if (EDGESNO != 1 && EDGESNO != 4) {
                wd->lastReply = {};
                wd->pDecoration->onPositioningReply({});
                continue;
            }

            auto desiredSize = 0;
            if (LEFT)
                desiredSize = wd->positioningInfo.desiredExtents.topLeft.x;
            else if (RIGHT)
                desiredSize = wd->positioningInfo.desiredExtents.bottomRight.x;
            else if (TOP)
                desiredSize = wd->positioningInfo.desiredExtents.topLeft.y;
            else
                desiredSize = wd->positioningInfo.desiredExtents.bottomRight.y;

            const auto EDGEPOINT = getEdgeDefinedPoint(wd->positioningInfo.edges, pWindow);

            Vector2D   pos, size;

            if (EDGESNO == 4) {
                pos  = wb.pos() - EDGEPOINT - Vector2D{stickyOffsetXL + desiredSize, stickyOffsetYT + desiredSize};
                size = wb.size() + Vector2D{stickyOffsetXL + stickyOffsetXR + desiredSize * 2, stickyOffsetYB + stickyOffsetYT + desiredSize * 2};

                stickyOffsetXL += desiredSize;
                stickyOffsetXR += desiredSize;
                stickyOffsetYT += desiredSize;
                stickyOffsetYB += desiredSize;
            } else if (LEFT) {
                pos = wb.pos() - EDGEPOINT - Vector2D{stickyOffsetXL, -stickyOffsetYT};
                pos.x -= desiredSize;
                size = {(double)desiredSize, wb.size().y + stickyOffsetYB + stickyOffsetYT};

                if (SOLID)
                    stickyOffsetXL += desiredSize;
            } else if (RIGHT) {
                pos  = wb.pos() + Vector2D{wb.size().x, 0.0} - EDGEPOINT + Vector2D{stickyOffsetXR, -stickyOffsetYT};
                size = {(double)desiredSize, wb.size().y + stickyOffsetYB + stickyOffsetYT};

                if (SOLID)
                    stickyOffsetXR += desiredSize;
            } else if (TOP) {
                pos = wb.pos() - EDGEPOINT - Vector2D{stickyOffsetXL, stickyOffsetYT};
                pos.y -= desiredSize;
                size = {wb.size().x + stickyOffsetXL + stickyOffsetXR, (double)desiredSize};

                if (SOLID)
                    stickyOffsetYT += desiredSize;
            } else {
                pos  = wb.pos() + Vector2D{0.0, wb.size().y} - EDGEPOINT - Vector2D{stickyOffsetXL, stickyOffsetYB};
                size = {wb.size().x + stickyOffsetXL + stickyOffsetXR, (double)desiredSize};

                if (SOLID)
                    stickyOffsetYB += desiredSize;
            }

            wd->lastReply = {{pos, size}, EPHEMERAL};
            wd->pDecoration->onPositioningReply(wd->lastReply);

            continue;
        } else {
            // invalid
            wd->lastReply = {};
            wd->pDecoration->onPositioningReply({});
            continue;
        }
    }

    if (WINDOWDATA->extents != SBoxExtents{{stickyOffsetXL + reservedXL, stickyOffsetYT + reservedYT}, {stickyOffsetXR + reservedXR, stickyOffsetYB + reservedYB}}) {
        WINDOWDATA->extents = {{stickyOffsetXL + reservedXL, stickyOffsetYT + reservedYT}, {stickyOffsetXR + reservedXR, stickyOffsetYB + reservedYB}};
        g_pLayoutManager->getCurrentLayout()->recalculateWindow(pWindow);
    }
}

void CDecorationPositioner::onWindowUnmap(PHLWINDOW pWindow) {
    std::erase_if(m_vWindowPositioningDatas, [&](const auto& data) { return data->pWindow.lock() == pWindow; });
    m_mWindowDatas.erase(pWindow);
}

void CDecorationPositioner::onWindowMap(PHLWINDOW pWindow) {
    m_mWindowDatas[pWindow] = {};
}

SBoxExtents CDecorationPositioner::getWindowDecorationReserved(PHLWINDOW pWindow) {
    try {
        const auto E = m_mWindowDatas.at(pWindow);
        return E.reserved;
    } catch (std::out_of_range& e) { return {}; }
}

SBoxExtents CDecorationPositioner::getWindowDecorationExtents(PHLWINDOW pWindow, bool inputOnly) {
    CBox const mainSurfaceBox = pWindow->getWindowMainSurfaceBox();
    CBox       accum          = mainSurfaceBox;

    for (auto const& data : m_vWindowPositioningDatas) {
        if (!data->pDecoration || (inputOnly && !(data->pDecoration->getDecorationFlags() & DECORATION_ALLOWS_MOUSE_INPUT)))
            continue;

        auto const window = data->pWindow.lock();
        if (!window || window != pWindow)
            continue;

        CBox decoBox;
        if (data->positioningInfo.policy == DECORATION_POSITION_ABSOLUTE) {
            decoBox = mainSurfaceBox;
            decoBox.addExtents(data->positioningInfo.desiredExtents);
        } else {
            decoBox = data->lastReply.assignedGeometry;
            decoBox.translate(getEdgeDefinedPoint(data->positioningInfo.edges, pWindow));
        }

        // Check bounds only if decoBox extends beyond accum
        SBoxExtents extentsToAdd;
        bool        needUpdate = false;

        if (decoBox.x < accum.x) {
            extentsToAdd.topLeft.x = accum.x - decoBox.x;
            needUpdate             = true;
        }
        if (decoBox.y < accum.y) {
            extentsToAdd.topLeft.y = accum.y - decoBox.y;
            needUpdate             = true;
        }
        if (decoBox.x + decoBox.w > accum.x + accum.w) {
            extentsToAdd.bottomRight.x = (decoBox.x + decoBox.w) - (accum.x + accum.w);
            needUpdate                 = true;
        }
        if (decoBox.y + decoBox.h > accum.y + accum.h) {
            extentsToAdd.bottomRight.y = (decoBox.y + decoBox.h) - (accum.y + accum.h);
            needUpdate                 = true;
        }

        if (needUpdate)
            accum.addExtents(extentsToAdd);
    }

    return accum.extentsFrom(mainSurfaceBox);
}

CBox CDecorationPositioner::getBoxWithIncludedDecos(PHLWINDOW pWindow) {
    CBox accum = pWindow->getWindowMainSurfaceBox();

    for (auto const& data : m_vWindowPositioningDatas) {
        if (data->pWindow.lock() != pWindow)
            continue;

        if (!(data->pDecoration->getDecorationFlags() & DECORATION_PART_OF_MAIN_WINDOW))
            continue;

        CBox decoBox;

        if (data->positioningInfo.policy == DECORATION_POSITION_ABSOLUTE) {
            decoBox = data->pWindow->getWindowMainSurfaceBox();
            decoBox.addExtents(data->positioningInfo.desiredExtents);
        } else {
            decoBox              = data->lastReply.assignedGeometry;
            const auto EDGEPOINT = getEdgeDefinedPoint(data->positioningInfo.edges, pWindow);
            decoBox.translate(EDGEPOINT);
        }

        SBoxExtents extentsToAdd;

        if (decoBox.x < accum.x)
            extentsToAdd.topLeft.x = accum.x - decoBox.x;
        if (decoBox.y < accum.y)
            extentsToAdd.topLeft.y = accum.y - decoBox.y;
        if (decoBox.x + decoBox.w > accum.x + accum.w)
            extentsToAdd.bottomRight.x = (decoBox.x + decoBox.w) - (accum.x + accum.w);
        if (decoBox.y + decoBox.h > accum.y + accum.h)
            extentsToAdd.bottomRight.y = (decoBox.y + decoBox.h) - (accum.y + accum.h);

        accum.addExtents(extentsToAdd);
    }

    return accum;
}

CBox CDecorationPositioner::getWindowDecorationBox(IHyprWindowDecoration* deco) {
    auto const window = deco->m_pWindow.lock();
    const auto DATA   = getDataFor(deco, window);

    CBox       box = DATA->lastReply.assignedGeometry;
    box.translate(getEdgeDefinedPoint(DATA->positioningInfo.edges, window));
    return box;
}
