#include "DecorationPositioner.hpp"
#include "../../Compositor.hpp"

CDecorationPositioner::CDecorationPositioner() {
    g_pHookSystem->hookDynamic("closeWindow", [this](void* call, SCallbackInfo& info, std::any data) {
        auto* const PWINDOW = std::any_cast<CWindow*>(data);
        this->onWindowUnmap(PWINDOW);
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

void CDecorationPositioner::onWindowUpdate(CWindow* pWindow) {
    if (!g_pCompositor->windowValidMapped(pWindow))
        return;

    auto* const WINDOWDATA     = &m_mWindowDatas[pWindow];
    WINDOWDATA->lastWindowSize = pWindow->m_vRealSize.vec();
    const bool EPHEMERAL       = pWindow->m_vRealSize.isBeingAnimated();

    //
    std::vector<CDecorationPositioner::SWindowPositioningData*> datas;
    for (auto& wd : pWindow->m_dWindowDecorations) {
        datas.push_back(getDataFor(wd.get(), pWindow));
    }

    std::sort(datas.begin(), datas.end(), [](const auto& a, const auto& b) { return a->positioningInfo.priority > b->positioningInfo.priority; });

    CBox       wb         = pWindow->getWindowMainSurfaceBox();
    const auto BORDERSIZE = pWindow->getRealBorderSize();
    wb.expand(BORDERSIZE);

    // calc reserved
    float reservedXL = 0, reservedYT = 0, reservedXR = 0, reservedYB = 0;
    for (size_t i = 0; i < datas.size(); ++i) {
        auto* const wd = datas[i];

        if (wd->positioningInfo.policy != DECORATION_POSITION_STICKY)
            continue;

        if (!wd->positioningInfo.reserved)
            continue;

        const auto DESIREDSIZE =
            wd->positioningInfo.edges & (DECORATION_EDGE_TOP | DECORATION_EDGE_BOTTOM) ? wd->positioningInfo.desiredGeometry.h : wd->positioningInfo.desiredGeometry.w;

        const bool TOP     = wd->positioningInfo.edges & DECORATION_EDGE_TOP;
        const bool BOTTOM  = wd->positioningInfo.edges & DECORATION_EDGE_BOTTOM;
        const bool LEFT    = wd->positioningInfo.edges & DECORATION_EDGE_LEFT;
        const bool RIGHT   = wd->positioningInfo.edges & DECORATION_EDGE_RIGHT;
        const int  EDGESNO = TOP + BOTTOM + LEFT + RIGHT;

        if (EDGESNO != 1)
            continue;

        if (LEFT)
            reservedXL += DESIREDSIZE;
        else if (RIGHT)
            reservedXR += DESIREDSIZE;
        else if (TOP)
            reservedYT += DESIREDSIZE;
        else
            reservedYB += DESIREDSIZE;
    }

    WINDOWDATA->reserved = {{reservedXL, reservedYT}, {reservedXR, reservedYB}};

    float stickyOffsetXL = 0, stickyOffsetYT = 0, stickyOffsetXR = 0, stickyOffsetYB = 0;

    for (size_t i = 0; i < datas.size(); ++i) {
        auto* const wd = datas[i];

        if (wd->positioningInfo.policy == DECORATION_POSITION_ABSOLUTE) {
            wd->lastReply = {};
            wd->pDecoration->onPositioningReply({});
            continue;
        }

        const bool TOP     = wd->positioningInfo.edges & DECORATION_EDGE_TOP;
        const bool BOTTOM  = wd->positioningInfo.edges & DECORATION_EDGE_BOTTOM;
        const bool LEFT    = wd->positioningInfo.edges & DECORATION_EDGE_LEFT;
        const bool RIGHT   = wd->positioningInfo.edges & DECORATION_EDGE_RIGHT;
        const int  EDGESNO = TOP + BOTTOM + LEFT + RIGHT;

        if (wd->positioningInfo.policy == DECORATION_POSITION_STICKY) {
            if (EDGESNO != 1) {
                wd->lastReply = {};
                wd->pDecoration->onPositioningReply({});
                continue;
            }

            const auto DESIREDSIZE =
                wd->positioningInfo.edges & (DECORATION_EDGE_TOP | DECORATION_EDGE_BOTTOM) ? wd->positioningInfo.desiredGeometry.h : wd->positioningInfo.desiredGeometry.w;

            const auto EDGEPOINT = getEdgeDefinedPoint(wd->positioningInfo.edges, pWindow);

            Vector2D   pos, size;

            if (LEFT) {
                pos = wb.pos() - EDGEPOINT - Vector2D{stickyOffsetXL, 0};
                pos.x -= DESIREDSIZE;
                size = {DESIREDSIZE, wb.size().y};

                stickyOffsetXL += DESIREDSIZE;
            } else if (RIGHT) {
                pos  = wb.pos() + Vector2D{wb.size().x, 0} - EDGEPOINT + Vector2D{stickyOffsetXR, 0};
                size = {DESIREDSIZE, wb.size().y};

                stickyOffsetXR += DESIREDSIZE;
            } else if (TOP) {
                pos = wb.pos() - EDGEPOINT - Vector2D{0, stickyOffsetYT};
                pos.y -= DESIREDSIZE;
                size = {wb.size().x, DESIREDSIZE};

                stickyOffsetYT += DESIREDSIZE;
            } else {
                pos  = wb.pos() + Vector2D{0, wb.size().y} - EDGEPOINT - Vector2D{0, stickyOffsetYB};
                size = {wb.size().x, DESIREDSIZE};

                stickyOffsetYB += DESIREDSIZE;
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

SWindowDecorationExtents CDecorationPositioner::getWindowDecorationReserved(CWindow* pWindow) {
    return m_mWindowDatas[pWindow].reserved;
}

SWindowDecorationExtents CDecorationPositioner::getWindowDecorationExtents(CWindow* pWindow, bool inputOnly) {
    if (!inputOnly)
        return m_mWindowDatas[pWindow].extents;

    // TODO:
    return m_mWindowDatas[pWindow].extents;
}

CBox CDecorationPositioner::getBoxWithIncludedDecos(CWindow* pWindow) {
    CBox accum = pWindow->getWindowMainSurfaceBox().expand(pWindow->getRealBorderSize());

    for (auto& data : m_vWindowPositioningDatas) {
        if (data->pWindow != pWindow)
            continue;

        if (!(data->pDecoration->getDecorationFlags() & DECORATION_PART_OF_MAIN_WINDOW))
            continue;

        CBox       decoBox   = data->lastReply.assignedGeometry;
        const auto EDGEPOINT = getEdgeDefinedPoint(data->positioningInfo.edges, pWindow);
        decoBox.translate(EDGEPOINT);

        SWindowDecorationExtents extentsToAdd;

        if (decoBox.x < accum.x)
            extentsToAdd.topLeft.x = accum.x - decoBox.x;
        if (decoBox.y < accum.y)
            extentsToAdd.topLeft.y = accum.y - decoBox.y;
        if (decoBox.x + decoBox.w > accum.x + accum.w)
            extentsToAdd.bottomRight.x = accum.x + accum.w - (decoBox.x + decoBox.w);
        if (decoBox.y + decoBox.h > accum.y + accum.h)
            extentsToAdd.bottomRight.y = accum.y + accum.h - (decoBox.y + decoBox.h);
    }

    return accum;
}

CBox CDecorationPositioner::getWindowDecorationBox(IHyprWindowDecoration* deco) {
    const auto DATA = getDataFor(deco, deco->m_pWindow);

    CBox       box = DATA->lastReply.assignedGeometry;
    box.translate(getEdgeDefinedPoint(DATA->positioningInfo.edges, deco->m_pWindow));
    return box;
}