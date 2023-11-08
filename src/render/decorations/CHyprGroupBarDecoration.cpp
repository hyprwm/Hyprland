#include "CHyprGroupBarDecoration.hpp"
#include "../../Compositor.hpp"
#include <ranges>
#include <pango/pangocairo.h>

// shared things to conserve VRAM
static CTexture m_tGradientActive;
static CTexture m_tGradientInactive;

CHyprGroupBarDecoration::CHyprGroupBarDecoration(CWindow* pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;
}

CHyprGroupBarDecoration::~CHyprGroupBarDecoration() {}

SWindowDecorationExtents CHyprGroupBarDecoration::getWindowDecorationExtents() {
    return m_seExtents;
}

eDecorationType CHyprGroupBarDecoration::getDecorationType() {
    return DECORATION_GROUPBAR;
}

constexpr int BAR_INDICATOR_HEIGHT   = 3;
constexpr int BAR_PADDING_OUTER_VERT = 2;
constexpr int BAR_TEXT_PAD           = 2;
constexpr int BAR_HORIZONTAL_PADDING = 2;

//

void CHyprGroupBarDecoration::updateWindow(CWindow* pWindow) {
    damageEntire();

    const auto         PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto         WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    static auto* const PRENDERTITLES  = &g_pConfigManager->getConfigValuePtr("group:groupbar:render_titles")->intValue;
    static auto* const PTITLEFONTSIZE = &g_pConfigManager->getConfigValuePtr("group:groupbar:font_size")->intValue;

    if (pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET != m_vLastWindowPos || pWindow->m_vRealSize.vec() != m_vLastWindowSize) {
        // we draw 3px above the window's border with 3px

        const int BORDERSIZE = pWindow->getRealBorderSize();

        m_seExtents.topLeft     = Vector2D(0, BORDERSIZE + BAR_PADDING_OUTER_VERT * 2 + BAR_INDICATOR_HEIGHT + (*PRENDERTITLES ? *PTITLEFONTSIZE : 0) + 2);
        m_seExtents.bottomRight = Vector2D();

        m_vLastWindowPos  = pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET;
        m_vLastWindowSize = pWindow->m_vRealSize.vec();

        invalidateTextures();
    }

    if (!m_pWindow->m_sGroupData.pNextWindow) {
        m_pWindow->m_vDecosToRemove.push_back(this);
        return;
    }

    m_dwGroupMembers.clear();
    CWindow* head = pWindow->getGroupHead();
    m_dwGroupMembers.push_back(head);

    CWindow* curr = head->m_sGroupData.pNextWindow;
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
    CBox dm = {m_vLastWindowPos.x - m_seExtents.topLeft.x, m_vLastWindowPos.y - m_seExtents.topLeft.y, m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x,
               m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}

void CHyprGroupBarDecoration::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    // get how many bars we will draw
    int                barsToDraw = m_dwGroupMembers.size();

    static auto* const PRENDERTITLES  = &g_pConfigManager->getConfigValuePtr("group:groupbar:render_titles")->intValue;
    static auto* const PTITLEFONTSIZE = &g_pConfigManager->getConfigValuePtr("group:groupbar:font_size")->intValue;
    static auto* const PGRADIENTS     = &g_pConfigManager->getConfigValuePtr("group:groupbar:gradients")->intValue;

    const int          BORDERSIZE = m_pWindow->getRealBorderSize();

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    m_fBarWidth = (m_vLastWindowSize.x - BAR_HORIZONTAL_PADDING * (barsToDraw - 1)) / barsToDraw;

    int xoff = 0;

    for (int i = 0; i < barsToDraw; ++i) {
        CBox rect = {m_vLastWindowPos.x + xoff - pMonitor->vecPosition.x + offset.x,
                     m_vLastWindowPos.y - BAR_PADDING_OUTER_VERT - BORDERSIZE - BAR_INDICATOR_HEIGHT - pMonitor->vecPosition.y + offset.y, m_fBarWidth, BAR_INDICATOR_HEIGHT};

        if (rect.width <= 0 || rect.height <= 0)
            break;

        rect.scale(pMonitor->scale);

        static auto* const PGROUPCOLACTIVE         = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.active")->data;
        static auto* const PGROUPCOLINACTIVE       = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.inactive")->data;
        static auto* const PGROUPCOLACTIVELOCKED   = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_active")->data;
        static auto* const PGROUPCOLINACTIVELOCKED = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_inactive")->data;

        const bool         GROUPLOCKED  = m_pWindow->getGroupHead()->m_sGroupData.locked;
        const auto* const  PCOLACTIVE   = GROUPLOCKED ? PGROUPCOLACTIVELOCKED : PGROUPCOLACTIVE;
        const auto* const  PCOLINACTIVE = GROUPLOCKED ? PGROUPCOLINACTIVELOCKED : PGROUPCOLINACTIVE;

        CColor             color =
            m_dwGroupMembers[i] == g_pCompositor->m_pLastWindow ? ((CGradientValueData*)PCOLACTIVE->get())->m_vColors[0] : ((CGradientValueData*)PCOLINACTIVE->get())->m_vColors[0];
        color.a *= a;
        g_pHyprOpenGL->renderRect(&rect, color);

        // render title if necessary
        if (*PRENDERTITLES) {
            CTitleTex* pTitleTex = textureFromTitle(m_dwGroupMembers[i]->m_szTitle);

            if (!pTitleTex)
                pTitleTex = m_sTitleTexs.titleTexs
                                .emplace_back(std::make_unique<CTitleTex>(m_dwGroupMembers[i],
                                                                          Vector2D{m_fBarWidth * pMonitor->scale, (*PTITLEFONTSIZE + 2 * BAR_TEXT_PAD) * pMonitor->scale}))
                                .get();

            rect.height = (*PTITLEFONTSIZE + 2 * BAR_TEXT_PAD) * 0.8 * pMonitor->scale;

            rect.y -= rect.height;
            rect.width = m_fBarWidth * pMonitor->scale;

            refreshGradients();

            if (*PGRADIENTS)
                g_pHyprOpenGL->renderTexture((m_dwGroupMembers[i] == g_pCompositor->m_pLastWindow ? m_tGradientActive : m_tGradientInactive), &rect, 1.0);

            rect.y -= (*PTITLEFONTSIZE + 2 * BAR_TEXT_PAD) * 0.2 * pMonitor->scale;
            rect.height = (*PTITLEFONTSIZE + 2 * BAR_TEXT_PAD) * pMonitor->scale;

            g_pHyprOpenGL->renderTexture(pTitleTex->tex, &rect, 1.f);
        }

        xoff += BAR_HORIZONTAL_PADDING + m_fBarWidth;
    }

    if (*PRENDERTITLES)
        invalidateTextures();
}

SWindowDecorationExtents CHyprGroupBarDecoration::getWindowDecorationReservedArea() {
    static auto* const PRENDERTITLES  = &g_pConfigManager->getConfigValuePtr("group:groupbar:render_titles")->intValue;
    static auto* const PTITLEFONTSIZE = &g_pConfigManager->getConfigValuePtr("group:groupbar:font_size")->intValue;
    return SWindowDecorationExtents{{0, BAR_INDICATOR_HEIGHT + BAR_PADDING_OUTER_VERT * 2 + (*PRENDERTITLES ? *PTITLEFONTSIZE : 0)}, {}};
}

CTitleTex* CHyprGroupBarDecoration::textureFromTitle(const std::string& title) {
    for (auto& tex : m_sTitleTexs.titleTexs) {
        if (tex->szContent == title)
            return tex.get();
    }

    return nullptr;
}

void CHyprGroupBarDecoration::invalidateTextures() {
    m_sTitleTexs.titleTexs.clear();
}

CTitleTex::CTitleTex(CWindow* pWindow, const Vector2D& bufferSize) {
    szContent                       = pWindow->m_szTitle;
    pWindowOwner                    = pWindow;
    const auto         CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto         CAIRO        = cairo_create(CAIROSURFACE);

    static auto* const PTITLEFONTFAMILY = &g_pConfigManager->getConfigValuePtr("group:groupbar:font_family")->strValue;
    static auto* const PTITLEFONTSIZE   = &g_pConfigManager->getConfigValuePtr("group:groupbar:font_size")->intValue;
    static auto* const PTEXTCOLOR       = &g_pConfigManager->getConfigValuePtr("group:groupbar:text_color")->intValue;

    const CColor       COLOR = CColor(*PTEXTCOLOR);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw title using Pango
    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, szContent.c_str(), -1);

    PangoFontDescription* fontDesc = pango_font_description_from_string(PTITLEFONTFAMILY->c_str());
    pango_font_description_set_size(fontDesc, *PTITLEFONTSIZE * PANGO_SCALE);
    pango_layout_set_font_description(layout, fontDesc);
    pango_font_description_free(fontDesc);

    const int maxWidth = bufferSize.x;

    pango_layout_set_width(layout, maxWidth * PANGO_SCALE);
    pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

    cairo_set_source_rgba(CAIRO, COLOR.r, COLOR.g, COLOR.b, COLOR.a);

    int layoutWidth, layoutHeight;
    pango_layout_get_size(layout, &layoutWidth, &layoutHeight);
    const int xOffset = std::round((bufferSize.x / 2.0 - layoutWidth / PANGO_SCALE / 2.0));
    const int yOffset = std::round((bufferSize.y / 2.0 - layoutHeight / PANGO_SCALE / 2.0));

    cairo_move_to(CAIRO, xOffset, yOffset);
    pango_cairo_show_layout(CAIRO, layout);

    g_object_unref(layout);

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    tex.allocate();
    glBindTexture(GL_TEXTURE_2D, tex.m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferSize.x, bufferSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    // delete cairo
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

CTitleTex::~CTitleTex() {
    tex.destroyTexture();
}

void renderGradientTo(CTexture& tex, const CColor& grad) {

    const Vector2D& bufferSize = g_pCompositor->m_pLastMonitor->vecPixelSize;

    const auto      CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto      CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    cairo_pattern_t* pattern;
    pattern = cairo_pattern_create_linear(0, 0, 0, bufferSize.y);
    cairo_pattern_add_color_stop_rgba(pattern, 1, grad.r, grad.g, grad.b, grad.a);
    cairo_pattern_add_color_stop_rgba(pattern, 0, grad.r, grad.g, grad.b, 0);
    cairo_rectangle(CAIRO, 0, 0, bufferSize.x, bufferSize.y);
    cairo_set_source(CAIRO, pattern);
    cairo_fill(CAIRO);
    cairo_pattern_destroy(pattern);

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    tex.allocate();
    glBindTexture(GL_TEXTURE_2D, tex.m_iTexID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

#ifndef GLES2
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
#endif

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferSize.x, bufferSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    // delete cairo
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

void CHyprGroupBarDecoration::refreshGradients() {
    if (m_tGradientActive.m_iTexID > 0)
        return;

    static auto* const PGROUPCOLACTIVE         = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.active")->data;
    static auto* const PGROUPCOLINACTIVE       = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.inactive")->data;
    static auto* const PGROUPCOLACTIVELOCKED   = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_active")->data;
    static auto* const PGROUPCOLINACTIVELOCKED = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_inactive")->data;

    const bool         GROUPLOCKED  = m_pWindow->getGroupHead()->m_sGroupData.locked;
    const auto* const  PCOLACTIVE   = GROUPLOCKED ? PGROUPCOLACTIVELOCKED : PGROUPCOLACTIVE;
    const auto* const  PCOLINACTIVE = GROUPLOCKED ? PGROUPCOLINACTIVELOCKED : PGROUPCOLINACTIVE;

    renderGradientTo(m_tGradientActive, ((CGradientValueData*)PCOLACTIVE->get())->m_vColors[0]);
    renderGradientTo(m_tGradientInactive, ((CGradientValueData*)PCOLINACTIVE->get())->m_vColors[0]);
}

bool CHyprGroupBarDecoration::onEndWindowDragOnDeco(CWindow* pDraggedWindow, const Vector2D& pos) {

    if (!pDraggedWindow->canBeGroupedInto(m_pWindow))
        return true;

    const float BARRELATIVEX = pos.x - m_vLastWindowPos.x - m_fBarWidth / 2;
    const int   WINDOWINDEX  = BARRELATIVEX < 0 ? -1 : (BARRELATIVEX) / (m_fBarWidth + BAR_HORIZONTAL_PADDING);

    CWindow*    pWindowInsertAfter = m_pWindow->getGroupWindowByIndex(WINDOWINDEX);
    CWindow*    pWindowInsertEnd   = pWindowInsertAfter->m_sGroupData.pNextWindow;
    CWindow*    pDraggedHead       = pDraggedWindow->m_sGroupData.pNextWindow ? pDraggedWindow->getGroupHead() : pDraggedWindow;

    if (pDraggedWindow->m_sGroupData.pNextWindow) {

        // stores group data
        std::vector<CWindow*> members;
        CWindow*              curr      = pDraggedHead;
        const bool            WASLOCKED = pDraggedHead->m_sGroupData.locked;
        do {
            members.push_back(curr);
            curr = curr->m_sGroupData.pNextWindow;
        } while (curr != members[0]);

        // removes all windows
        for (CWindow* w : members) {
            w->m_sGroupData.pNextWindow = nullptr;
            w->m_sGroupData.head        = false;
            w->m_sGroupData.locked      = false;
            g_pLayoutManager->getCurrentLayout()->onWindowRemoved(w);
        }

        // restores the group
        for (auto it = members.begin(); it != members.end(); ++it) {
            if (std::next(it) != members.end())
                (*it)->m_sGroupData.pNextWindow = *std::next(it);
            else
                (*it)->m_sGroupData.pNextWindow = members[0];
        }
        members[0]->m_sGroupData.head   = true;
        members[0]->m_sGroupData.locked = WASLOCKED;
    } else {
        g_pLayoutManager->getCurrentLayout()->onWindowRemoved(pDraggedWindow);
    }

    pWindowInsertAfter->insertWindowToGroup(pDraggedWindow);

    if (WINDOWINDEX == -1)
        std::swap(pDraggedHead->m_sGroupData.head, pWindowInsertEnd->m_sGroupData.head);

    m_pWindow->setGroupCurrent(pDraggedWindow);
    pDraggedWindow->applyGroupRules();
    pDraggedWindow->updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(pDraggedWindow);

    return false;
}

void CHyprGroupBarDecoration::onMouseButtonOnDeco(const Vector2D& pos, wlr_pointer_button_event* e) {
    if (e->state != WLR_BUTTON_PRESSED)
        return;

    const float BARRELATIVEX = pos.x - m_vLastWindowPos.x;
    const int   WINDOWINDEX  = (BARRELATIVEX) / (m_fBarWidth + BAR_HORIZONTAL_PADDING);

    if (BARRELATIVEX - (m_fBarWidth + BAR_HORIZONTAL_PADDING) * WINDOWINDEX > m_fBarWidth) {
        if (!g_pCompositor->isWindowActive(m_pWindow))
            g_pCompositor->focusWindow(m_pWindow);
        return;
    }

    CWindow* pWindow = m_pWindow->getGroupWindowByIndex(WINDOWINDEX);

    if (pWindow != m_pWindow)
        pWindow->setGroupCurrent(pWindow);

    if (!g_pCompositor->isWindowActive(pWindow))
        g_pCompositor->focusWindow(pWindow);
}

void CHyprGroupBarDecoration::onBeginWindowDragOnDeco(const Vector2D& pos) {
    const float BARRELATIVEX = pos.x - m_vLastWindowPos.x;
    const int   WINDOWINDEX  = (BARRELATIVEX) / (m_fBarWidth + BAR_HORIZONTAL_PADDING);

    if (BARRELATIVEX - (m_fBarWidth + BAR_HORIZONTAL_PADDING) * WINDOWINDEX > m_fBarWidth)
        return;

    CWindow* pWindow = m_pWindow->getGroupWindowByIndex(WINDOWINDEX);

    // hack
    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(pWindow);
    if (!pWindow->m_bIsFloating) {
        const bool GROUPSLOCKEDPREV        = g_pKeybindManager->m_bGroupsLocked;
        g_pKeybindManager->m_bGroupsLocked = true;
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(pWindow);
        g_pKeybindManager->m_bGroupsLocked = GROUPSLOCKEDPREV;
    }

    g_pInputManager->currentlyDraggedWindow = pWindow;

    if (!g_pCompositor->isWindowActive(pWindow))
        g_pCompositor->focusWindow(pWindow);
}

eDecorationLayer CHyprGroupBarDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

uint64_t CHyprGroupBarDecoration::getDecorationFlags() {
    return DECORATION_ALLOWS_MOUSE_INPUT;
}