#include "CHyprGroupBarDecoration.hpp"
#include "../../Compositor.hpp"

CHyprGroupBarDecoration::CHyprGroupBarDecoration(CWindow* pWindow) {
    m_pWindow = pWindow;
}

CHyprGroupBarDecoration::~CHyprGroupBarDecoration() {}

SWindowDecorationExtents CHyprGroupBarDecoration::getWindowDecorationExtents() {
    return m_seExtents;
}

eDecorationType CHyprGroupBarDecoration::getDecorationType() {
    return DECORATION_GROUPBAR;
}

void CHyprGroupBarDecoration::updateWindow(CWindow* pWindow) {
    damageEntire();

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    if (pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET != m_vLastWindowPos || pWindow->m_vRealSize.vec() != m_vLastWindowSize) {
        // we draw 3px above the window's border with 3px
        const auto PBORDERSIZE = &g_pConfigManager->getConfigValuePtr("general:border_size")->intValue;

        m_seExtents.topLeft     = Vector2D(0, *PBORDERSIZE + 3 + 3);
        m_seExtents.bottomRight = Vector2D();

        m_vLastWindowPos  = pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET;
        m_vLastWindowSize = pWindow->m_vRealSize.vec();
    }

    if (!m_pWindow->m_sGroupData.pNextWindow) {
        m_pWindow->m_vDecosToRemove.push_back(this);
        return;
    }

    m_dwGroupMembers.clear();
    CWindow* curr = pWindow;
    CWindow* head = nullptr;
    while (!curr->m_sGroupData.head) {
        curr = curr->m_sGroupData.pNextWindow;
    }

    head = curr;
    m_dwGroupMembers.push_back(curr);
    curr = curr->m_sGroupData.pNextWindow;
    while (curr != head) {
        m_dwGroupMembers.push_back(curr);
        curr = curr->m_sGroupData.pNextWindow;
    }

    damageEntire();

    if (m_dwGroupMembers.size() == 0) {
        m_pWindow->m_vDecosToRemove.push_back(this);
        return;
    }
}

void CHyprGroupBarDecoration::damageEntire() {
    wlr_box dm = {m_vLastWindowPos.x - m_seExtents.topLeft.x, m_vLastWindowPos.y - m_seExtents.topLeft.y, m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x,
                  m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}

void CHyprGroupBarDecoration::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    // get how many bars we will draw
    int barsToDraw = m_dwGroupMembers.size();

    if (barsToDraw < 1 || m_pWindow->isHidden() || !g_pCompositor->windowValidMapped(m_pWindow))
        return;

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    const int PAD = 2; //2px

    const int BARW = (m_vLastWindowSize.x - PAD * (barsToDraw - 1)) / barsToDraw;

    int       xoff = 0;

    for (int i = 0; i < barsToDraw; ++i) {
        wlr_box rect = {m_vLastWindowPos.x + xoff - pMonitor->vecPosition.x + offset.x, m_vLastWindowPos.y - m_seExtents.topLeft.y - pMonitor->vecPosition.y + offset.y, BARW, 3};

        if (rect.width <= 0 || rect.height <= 0)
            break;

        scaleBox(&rect, pMonitor->scale);

        static auto* const PGROUPCOLACTIVE   = &g_pConfigManager->getConfigValuePtr("general:col.group_border_active")->data;
        static auto* const PGROUPCOLINACTIVE = &g_pConfigManager->getConfigValuePtr("general:col.group_border")->data;

        CColor             color = m_dwGroupMembers[i] == g_pCompositor->m_pLastWindow ? ((CGradientValueData*)PGROUPCOLACTIVE->get())->m_vColors[0] :
                                                                                         ((CGradientValueData*)PGROUPCOLINACTIVE->get())->m_vColors[0];
        color.a *= a;
        g_pHyprOpenGL->renderRect(&rect, color);

        xoff += PAD + BARW;
    }
}