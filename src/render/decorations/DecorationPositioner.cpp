#include "DecorationPositioner.hpp"
#include "../../Compositor.hpp"

CDecorationPositioner::CDecorationPositioner() {
    g_pHookSystem->hookDynamic("closeWindow", [this](void* call, SCallbackInfo& info, std::any data) {
        auto* const PWINDOW = std::any_cast<CWindow*>(data);
        this->onWindowUnmap(PWINDOW);
    });

    g_pHookSystem->hookDynamic("openWindow", [this](void* call, SCallbackInfo& info, std::any data) {
        auto* const PWINDOW = std::any_cast<CWindow*>(data);
        this->onWindowMap(PWINDOW);
    });
}

Vector2D CDecorationPositioner::getEdgeDefinedPoint(uint32_t edges, CWindow* pWindow) {
    const bool TOP    = edges & DECORATION_EDGE_TOP;
    const bool BOTTOM = edges & DECORATION_EDGE_BOTTOM;
    const bool LEFT   = edges & DECORATION_EDGE_LEFT;
    const bool RIGHT  = edges & DECORATION_EDGE_RIGHT;

    const int  EDGESNO = TOP + BOTTOM + LEFT + RIGHT;

    if (EDGESNO == 0 || EDGESNO > 2) {
        Debug::log(ERR, "getEdgeDefinedPoint: invalid number of edges");
        return {};
    }

    CBox       wb         = pWindow->getWindowMainSurfaceBox();
    const auto BORDERSIZE = pWindow->getRealBorderSize();
    wb.expand(BORDERSIZE);

    if (EDGESNO == 1) {
        if (TOP)
            return wb.pos() + Vector2D{wb.size().x / 2.0, 0};
        else if (BOTTOM)
            return wb.pos() + Vector2D{wb.size().x / 2.0, wb.size().y};
        else if (LEFT)
            return wb.pos() + Vector2D{0, wb.size().y / 2.0};
        else if (RIGHT)
            return wb.pos() + Vector2D{wb.size().x, wb.size().y / 2.0};
        UNREACHABLE();
    } else {
        if (TOP && LEFT)
            return wb.pos();
        if (TOP && RIGHT)
            return wb.pos() + Vector2D{wb.size().x, 0};
        if (BOTTOM && RIGHT)
            return wb.pos() + wb.size();
        if (BOTTOM && LEFT)
            return wb.pos() + Vector2D{0, wb.size().y};
        UNREACHABLE();
    }
    UNREACHABLE();
    return {};
}

void CDecorationPositioner::uncacheDecoration(IHyprWindowDecoration* deco) {
    std::erase_if(m_vWindowPositioningDatas, [&](const auto& data) { return data->pDecoration == deco; });

    const auto WIT = std::find_if(m_mWindowDatas.begin(), m_mWindowDatas.end(), [&](const auto& other) { return other.first == deco->m_pWindow; });
    if (WIT == m_mWindowDatas.end())
        return;

    WIT->second.needsRecalc = true;
}

void CDecorationPositioner::repositionDeco(IHyprWindowDecoration* deco) {
    uncacheDecoration(deco);
    onWindowUpdate(deco->m_pWindow);
}

CDecorationPositioner::SWindowPositioningData* CDecorationPositioner::getDataFor(IHyprWindowDecoration* pDecoration, CWindow* pWindow) {
    auto it = std::find_if(m_vWindowPositioningDatas.begin(), m_vWindowPositioningDatas.end(), [&](const auto& el) { return el->pDecoration == pDecoration; });

    if (it != m_vWindowPositioningDatas.end())
        return it->get();

    const auto DATA = m_vWindowPositioningDatas.emplace_back(std::make_unique<CDecorationPositioner::SWindowPositioningData>(pWindow, pDecoration)).get();

    DATA->positioningInfo = pDecoration->getPositioningInfo();

    return DATA;
}

void CDecorationPositioner::sanitizeDatas() {
    std::erase_if(m_mWindowDatas, [](const auto& other) { return !g_pCompositor->windowExists(other.first); });
    std::erase_if(m_vWindowPositioningDatas, [](const auto& other) {
        if (!g_pCompositor->windowExists(other->pWindow))
            return true;
        if (std::find_if(other->pWindow->m_dWindowDecorations.begin(), other->pWindow->m_dWindowDecorations.end(),
                         [&](const auto& el) { return el.get() == other->pDecoration; }) == other->pWindow->m_dWindowDecorations.end())
            return true;
        return false;
    });
}

void CDecorationPositioner::forceRecalcFor(CWindow* pWindow) {
    const auto WIT = std::find_if(m_mWindowDatas.begin(), m_mWindowDatas.end(), [&](const auto& other) { return other.first == pWindow; });
    if (WIT == m_mWindowDatas.end())
        return;

    const auto WINDOWDATA = &WIT->second;

    WINDOWDATA->needsRecalc = true;
}

void CDecorationPositioner::onWindowUpdate(CWindow* pWindow) {
    if (!g_pCompositor->windowExists(pWindow) || !pWindow->m_bIsMapped)
        return;

    const auto WIT = std::find_if(m_mWindowDatas.begin(), m_mWindowDatas.end(), [&](const auto& other) { return other.first == pWindow; });
    if (WIT == m_mWindowDatas.end())
        return;

    const auto WINDOWDATA = &WIT->second;

    sanitizeDatas();

    //
    std::vector<CDecorationPositioner::SWindowPositioningData*> datas;
    for (auto& wd : pWindow->m_dWindowDecorations) {
        datas.push_back(getDataFor(wd.get(), pWindow));
    }

    if (WINDOWDATA->lastWindowSize == pWindow->m_vRealSize.vec() /* position not changed */
        &&
        std::all_of(m_vWindowPositioningDatas.begin(), m_vWindowPositioningDatas.end(), [pWindow](const auto& data) { return pWindow != data->pWindow || !data->needsReposition; })
        /* all window datas are either not for this window or don't need a reposition */
        && !WINDOWDATA->needsRecalc /* window doesn't need recalc */
    )
        return;

    WINDOWDATA->lastWindowSize = pWindow->m_vRealSize.vec();
    WINDOWDATA->needsRecalc    = false;
    const bool EPHEMERAL       = pWindow->m_vRealSize.isBeingAnimated();

    std::sort(datas.begin(), datas.end(), [](const auto& a, const auto& b) { return a->positioningInfo.priority > b->positioningInfo.priority; });

    CBox       wb         = pWindow->getWindowMainSurfaceBox();
    const auto BORDERSIZE = pWindow->getRealBorderSize();
    wb.expand(BORDERSIZE);

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
            if (EDGESNO != 1) {
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

            if (LEFT) {
                pos = wb.pos() - EDGEPOINT - Vector2D{stickyOffsetXL, 0};
                pos.x -= desiredSize;
                size = {desiredSize, wb.size().y};

                if (SOLID)
                    stickyOffsetXL += desiredSize;
            } else if (RIGHT) {
                pos  = wb.pos() + Vector2D{wb.size().x, 0} - EDGEPOINT + Vector2D{stickyOffsetXR, 0};
                size = {desiredSize, wb.size().y};

                if (SOLID)
                    stickyOffsetXR += desiredSize;
            } else if (TOP) {
                pos = wb.pos() - EDGEPOINT - Vector2D{0, stickyOffsetYT};
                pos.y -= desiredSize;
                size = {wb.size().x, desiredSize};

                if (SOLID)
                    stickyOffsetYT += desiredSize;
            } else {
                pos  = wb.pos() + Vector2D{0, wb.size().y} - EDGEPOINT - Vector2D{0, stickyOffsetYB};
                size = {wb.size().x, desiredSize};

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

    WINDOWDATA->extents = {{stickyOffsetXL + reservedXL, stickyOffsetYT + reservedYT}, {stickyOffsetXR + reservedXR, stickyOffsetYB + reservedYB}};
}

void CDecorationPositioner::onWindowUnmap(CWindow* pWindow) {
    std::erase_if(m_vWindowPositioningDatas, [&](const auto& data) { return data->pWindow == pWindow; });
    m_mWindowDatas.erase(pWindow);
}

void CDecorationPositioner::onWindowMap(CWindow* pWindow) {
    m_mWindowDatas[pWindow] = {};
}

SWindowDecorationExtents CDecorationPositioner::getWindowDecorationReserved(CWindow* pWindow) {
    try {
        const auto E = m_mWindowDatas.at(pWindow);
        return E.reserved;
    } catch (std::out_of_range& e) { return {}; }
}

SWindowDecorationExtents CDecorationPositioner::getWindowDecorationExtents(CWindow* pWindow, bool inputOnly) {
    CBox accum = pWindow->getWindowMainSurfaceBox().expand(pWindow->getRealBorderSize());

    for (auto& data : m_vWindowPositioningDatas) {
        if (data->pWindow != pWindow)
            continue;

        if (!data->pWindow || !data->pDecoration)
            continue;

        if (!(data->pDecoration->getDecorationFlags() & DECORATION_ALLOWS_MOUSE_INPUT) && inputOnly)
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

        SWindowDecorationExtents extentsToAdd;

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

    return accum.extentsFrom(pWindow->getWindowMainSurfaceBox());
}

CBox CDecorationPositioner::getBoxWithIncludedDecos(CWindow* pWindow) {
    CBox accum = pWindow->getWindowMainSurfaceBox().expand(pWindow->getRealBorderSize());

    for (auto& data : m_vWindowPositioningDatas) {
        if (data->pWindow != pWindow)
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

        SWindowDecorationExtents extentsToAdd;

        if (decoBox.x < accum.x)
            extentsToAdd.topLeft.x = accum.x - decoBox.x;
        if (decoBox.y < accum.y)
            extentsToAdd.topLeft.y = accum.y - decoBox.y;
        if (decoBox.x + decoBox.w > accum.x + accum.w)
            extentsToAdd.bottomRight.x = accum.x + accum.w - (decoBox.x + decoBox.w);
        if (decoBox.y + decoBox.h > accum.y + accum.h)
            extentsToAdd.bottomRight.y = accum.y + accum.h - (decoBox.y + decoBox.h);

        accum.addExtents(extentsToAdd);
    }

    return accum;
}

CBox CDecorationPositioner::getWindowDecorationBox(IHyprWindowDecoration* deco) {
    const auto DATA = getDataFor(deco, deco->m_pWindow);

    CBox       box = DATA->lastReply.assignedGeometry;
    box.translate(getEdgeDefinedPoint(DATA->positioningInfo.edges, deco->m_pWindow));
    return box;
}