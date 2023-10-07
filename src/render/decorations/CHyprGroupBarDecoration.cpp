#include "CHyprGroupBarDecoration.hpp"
#include "../../Compositor.hpp"
#include <ranges>
#include <pango/pangocairo.h>

// shared things to conserve VRAM
static CTexture m_tGradientActive;
static CTexture m_tGradientInactive;
static CTexture m_tGradientLockedActive;
static CTexture m_tGradientLockedInactive;

CHyprGroupBarDecoration::CHyprGroupBarDecoration(CWindow* pWindow) : IHyprWindowDecoration(pWindow) {
    m_pWindow = pWindow;
    loadConfig();
    if (m_tGradientActive.m_iTexID == 0)
        refreshGradients();
}

CHyprGroupBarDecoration::~CHyprGroupBarDecoration() {}

SWindowDecorationExtents CHyprGroupBarDecoration::getWindowDecorationExtents() {
    return m_seExtents;
}

eDecorationType CHyprGroupBarDecoration::getDecorationType() {
    return DECORATION_GROUPBAR;
}

constexpr int BAR_INDICATOR_HEIGHT   = 3;
constexpr int BAR_INTERNAL_PADDING   = 3;
constexpr int BAR_EXTERNAL_PADDING   = 3;
constexpr int BAR_HORIZONTAL_PADDING = 5;
constexpr int BAR_TEXT_PAD           = 3;

//

void CHyprGroupBarDecoration::updateWindow(CWindow* pWindow) {
    damageEntire();

    const auto PWORKSPACE      = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
    const auto WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    if (pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET != m_vLastWindowPos || pWindow->m_vRealSize.vec() != m_vLastWindowSize) {
        m_vLastWindowPos  = pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET;
        m_vLastWindowSize = pWindow->m_vRealSize.vec();

        invalidateTextures();
    }

    if (!m_pWindow->m_sGroupData.pNextWindow) {
        m_pWindow->m_vDecosToRemove.push_back(this);
        return;
    }

    m_dwGroupMembers.clear();

    CWindow* HEAD = pWindow->getGroupHead();
    CWindow* curr = HEAD;
    do {
        m_dwGroupMembers.push_back(curr);
        curr = curr->m_sGroupData.pNextWindow;
    } while (curr != HEAD);

    damageEntire();
}

void CHyprGroupBarDecoration::damageEntire() {
    const int  BORDERSIZE = m_pWindow->getRealBorderSize();
    const auto RESERVED   = getWindowDecorationExtents();
    wlr_box    dm         = {m_vLastWindowPos.x,
                             m_vLastWindowPos.y +
                                 (m_bOnTop ? -BAR_INTERNAL_PADDING - m_iBarInternalHeight - (m_bInternalBar ? 0 : BORDERSIZE) :
                                             m_vLastWindowSize.y + BAR_INTERNAL_PADDING + (m_bInternalBar ? 0 : BORDERSIZE)),
                             m_vLastWindowSize.x, m_iBarInternalHeight};
    g_pHyprRenderer->damageBox(&dm);
}

void CHyprGroupBarDecoration::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    if (!m_pWindow->m_sSpecialRenderData.decorate || !m_bEnabled)
        return;

    int                barsToDraw = m_dwGroupMembers.size();

    static auto* const PRENDERTITLES = &g_pConfigManager->getConfigValuePtr("group:groupbar:render_titles")->intValue;

    static auto* const PGROUPCOLACTIVE         = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.active")->data;
    static auto* const PGROUPCOLINACTIVE       = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.inactive")->data;
    static auto* const PGROUPCOLACTIVELOCKED   = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_active")->data;
    static auto* const PGROUPCOLINACTIVELOCKED = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_inactive")->data;
    static auto* const PGROUPCOLBACKGROUND     = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.background")->data;

    static auto* const PMAXWIDTH = &g_pConfigManager->getConfigValuePtr("group:groupbar:max_width")->intValue;

    const bool         GROUPLOCKED  = m_pWindow->getGroupHead()->m_sGroupData.locked;
    const auto* const  PCOLACTIVE   = GROUPLOCKED ? PGROUPCOLACTIVELOCKED : PGROUPCOLACTIVE;
    const auto* const  PCOLINACTIVE = GROUPLOCKED ? PGROUPCOLINACTIVELOCKED : PGROUPCOLINACTIVE;

    const auto         PWORKSPACE = g_pCompositor->getWorkspaceByID(m_pWindow->m_iWorkspaceID);
    const int          ROUNDING   = m_pWindow->getRealRounding();
    const int          BORDERSIZE = m_pWindow->getRealBorderSize();

    m_fBarWidth = (m_vLastWindowSize.x - 2 * ROUNDING - BAR_HORIZONTAL_PADDING * (barsToDraw + 1)) / barsToDraw;

    if (*PMAXWIDTH >= 1) {
        m_fBarWidth = std::min(m_fBarWidth, (float)*PMAXWIDTH);
    }

    float currentOffset = BAR_HORIZONTAL_PADDING;

    // Bottom left of groupbar
    Vector2D pos = Vector2D(
        m_vLastWindowPos.x - pMonitor->vecPosition.x + offset.x + ROUNDING,
        std::floor(m_vLastWindowPos.y) - pMonitor->vecPosition.y + offset.y +
            (m_bOnTop ? (m_bInternalBar ? 0 : -BORDERSIZE) : std::floor(m_vLastWindowSize.y) + m_iBarInternalHeight + BAR_INTERNAL_PADDING + (m_bInternalBar ? 0 : BORDERSIZE)));

    wlr_box barBox  = {pos.x, pos.y - m_iBarHeight + (m_bOnTop ? -BAR_INTERNAL_PADDING : 0), m_fBarWidth, m_iBarHeight};
    wlr_box gradBox = {pos.x, pos.y - m_iBarHeight + (m_bOnTop ? -2 * BAR_INTERNAL_PADDING : -BAR_INTERNAL_PADDING) - m_iGradientHeight, m_fBarWidth, m_iGradientHeight};
    wlr_box textBox = m_iGradientHeight != 0 ? gradBox : barBox;
    textBox.y += BAR_TEXT_PAD;
    textBox.height -= 2 * BAR_TEXT_PAD;

    scaleBox(&barBox, pMonitor->scale);
    scaleBox(&textBox, pMonitor->scale);
    scaleBox(&gradBox, pMonitor->scale);

    if (m_bInternalBar) {
        float alpha     = m_pWindow->m_fAlpha.fl() * (m_pWindow->m_bPinned ? 1.f : PWORKSPACE->m_fAlpha.fl()) * m_pWindow->m_fActiveInactiveAlpha.fl();
        auto  color     = ((CGradientValueData*)PGROUPCOLBACKGROUND->get())->m_vColors[0];
        color           = CColor(color.r, color.g, color.b, alpha);
        wlr_box backBox = {pos.x - ROUNDING, pos.y - m_iBarFullHeight + (m_bOnTop ? 0 : -2 * ROUNDING + BAR_EXTERNAL_PADDING), m_vLastWindowSize.x,
                           m_iBarFullHeight + 2 * ROUNDING};
        scaleBox(&backBox, pMonitor->scale);

        if (ROUNDING != 0) {
            wlr_box backStencilBox = {pos.x - ROUNDING, pos.y + (m_bOnTop ? 0 : -2 * ROUNDING - m_iBarInternalHeight - BAR_INTERNAL_PADDING), m_vLastWindowSize.x, 2 * ROUNDING};

            scaleBox(&backStencilBox, pMonitor->scale);

            g_pHyprOpenGL->scissor(&backBox);

            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);

            glEnable(GL_STENCIL_TEST);

            glStencilFunc(GL_ALWAYS, 1, -1);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

            g_pHyprOpenGL->renderRect(&backStencilBox, CColor(0, 0, 0, 0), ROUNDING * pMonitor->scale);

            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

            glStencilFunc(GL_NOTEQUAL, 1, -1);
            glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

            if (backBox.width > 0 && backBox.height > 0)
                g_pHyprOpenGL->renderRect(&backBox, color, ROUNDING * pMonitor->scale);

            // cleanup stencil
            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);
            glDisable(GL_STENCIL_TEST);
            glStencilMask(-1);
            glStencilFunc(GL_ALWAYS, 1, 0xFF);
        } else if (backBox.width > 0 && backBox.height > 0)
            g_pHyprOpenGL->renderRect(&backBox, color, ROUNDING * pMonitor->scale);
    }

    for (int i = 0; i < barsToDraw; ++i) {

        barBox.x  = pos.x + currentOffset;
        gradBox.x = pos.x + currentOffset;
        textBox.x = pos.x + currentOffset;

        CColor color =
            m_dwGroupMembers[i] == g_pCompositor->m_pLastWindow ? ((CGradientValueData*)PCOLACTIVE->get())->m_vColors[0] : ((CGradientValueData*)PCOLINACTIVE->get())->m_vColors[0];
        color.a *= a;
        if (barBox.width > 0 && barBox.height > 0)
            g_pHyprOpenGL->renderRect(&barBox, color, std::sqrt(m_iBarHeight) / 2);

        // render title if necessary
        if (*PRENDERTITLES && textBox.width > 0 && textBox.height > 0) {
            CTitleTex* pTitleTex = textureFromTitle(m_dwGroupMembers[i]->m_szTitle);
            if (!pTitleTex)
                pTitleTex = m_sTitleTexs.titleTexs.emplace_back(std::make_unique<CTitleTex>(m_dwGroupMembers[i], Vector2D(textBox.width, textBox.height))).get();

            g_pHyprOpenGL->renderTexture(pTitleTex->tex, &textBox, 1.f);
        }

        if (gradBox.width > 0 && gradBox.height > 0) {
            if (m_tGradientActive.m_iTexID == 0) // no idea why it doesn't work
                refreshGradients();

            g_pHyprOpenGL->renderTexture((m_dwGroupMembers[i] == g_pCompositor->m_pLastWindow ? (GROUPLOCKED ? m_tGradientLockedActive : m_tGradientActive) :
                                                                                                (GROUPLOCKED ? m_tGradientLockedInactive : m_tGradientInactive)),
                                         &gradBox, 1.0);
        }

        currentOffset += (float)(BAR_HORIZONTAL_PADDING + m_fBarWidth) * pMonitor->scale;
    }

    if (*PRENDERTITLES)
        invalidateTextures();
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

    static auto* const PTITLEFONTSIZE = &g_pConfigManager->getConfigValuePtr("group:groupbar:titles_font_size")->intValue;
    static auto* const PTEXTCOLOR     = &g_pConfigManager->getConfigValuePtr("group:groupbar:text_color")->intValue;
    static auto* const PFONT          = &g_pConfigManager->getConfigValuePtr("group:groupbar:font")->strValue;

    const CColor       COLOR = CColor(*PTEXTCOLOR);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw title using Pango
    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, szContent.c_str(), -1);

    PangoFontDescription* fontDesc = pango_font_description_from_string(PFONT->c_str());
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
    if (m_tGradientActive.m_iTexID > 0) {
        m_tGradientActive.destroyTexture();
        m_tGradientInactive.destroyTexture();
        m_tGradientLockedActive.destroyTexture();
        m_tGradientLockedInactive.destroyTexture();
    }

    static auto* const PGROUPCOLACTIVE         = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.active")->data;
    static auto* const PGROUPCOLINACTIVE       = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.inactive")->data;
    static auto* const PGROUPCOLACTIVELOCKED   = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_active")->data;
    static auto* const PGROUPCOLINACTIVELOCKED = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_inactive")->data;

    renderGradientTo(m_tGradientActive, ((CGradientValueData*)PGROUPCOLACTIVE->get())->m_vColors[0]);
    renderGradientTo(m_tGradientInactive, ((CGradientValueData*)PGROUPCOLINACTIVE->get())->m_vColors[0]);
    renderGradientTo(m_tGradientLockedActive, ((CGradientValueData*)PGROUPCOLACTIVELOCKED->get())->m_vColors[0]);
    renderGradientTo(m_tGradientLockedInactive, ((CGradientValueData*)PGROUPCOLINACTIVELOCKED->get())->m_vColors[0]);
}

void CHyprGroupBarDecoration::loadConfig() {
    static auto* const PENABLED     = &g_pConfigManager->getConfigValuePtr("group:groupbar:enabled")->intValue;
    static auto* const PMODE        = &g_pConfigManager->getConfigValuePtr("group:groupbar:mode")->intValue;
    static auto* const PHEIGHT      = &g_pConfigManager->getConfigValuePtr("group:groupbar:height")->intValue;
    static auto* const PINTERNALBAR = &g_pConfigManager->getConfigValuePtr("group:groupbar:internal_bar")->intValue;

    m_bEnabled           = *PENABLED;
    m_iBarInternalHeight = std::max((long int)0, *PHEIGHT) + (*PMODE == 1 ? BAR_INDICATOR_HEIGHT + BAR_INTERNAL_PADDING : 0);
    m_iBarFullHeight     = m_iBarInternalHeight + BAR_INTERNAL_PADDING + BAR_EXTERNAL_PADDING;

    m_bOnTop       = g_pConfigManager->getConfigValuePtr("group:groupbar:top")->intValue;
    m_bInternalBar = *PINTERNALBAR;

    if (m_bEnabled) {
        m_seExtents.topLeft              = Vector2D(0, m_bOnTop ? m_iBarFullHeight : 0);
        m_seExtents.bottomRight          = Vector2D(0, m_bOnTop ? 0 : m_iBarFullHeight);
        m_seExtents.isInternalDecoration = *PINTERNALBAR;
    } else {
        m_seExtents.topLeft     = Vector2D(0, 0);
        m_seExtents.bottomRight = Vector2D(0, 0);
    }

    m_iBarHeight      = *PMODE != 1 ? *PHEIGHT : BAR_INDICATOR_HEIGHT;
    m_iGradientHeight = *PMODE == 1 ? *PHEIGHT : 0;
}

void CHyprGroupBarDecoration::forceReload() {
    loadConfig();
    refreshGradients();
}

CRegion CHyprGroupBarDecoration::getWindowDecorationRegion() {
    const int BORDERSIZE = m_pWindow->getRealBorderSize();
    return CRegion(m_vLastWindowPos.x,
                   m_vLastWindowPos.y +
                       (m_bOnTop ? -BAR_INTERNAL_PADDING - m_iBarInternalHeight - (m_bInternalBar ? 0 : BORDERSIZE) :
                                   m_vLastWindowSize.y + BAR_INTERNAL_PADDING + (m_bInternalBar ? 0 : BORDERSIZE)),
                   m_vLastWindowSize.x, m_iBarInternalHeight);
}

bool CHyprGroupBarDecoration::allowsInput() {
    return true;
}

void CHyprGroupBarDecoration::dragWindowToDecoration(CWindow* m_pDraggedWindow, const Vector2D& pos) {
    const int   SIZE         = m_dwGroupMembers.size();
    const int   ROUNDING     = m_pWindow->getRealRounding();
    const float BARRELATIVEX = pos.x - (m_vLastWindowPos.x + ROUNDING) - (m_fBarWidth / 2 + BAR_HORIZONTAL_PADDING);
    const int   WINDOWINDEX  = std::min(BARRELATIVEX < 0 ? -1 : (int)((BARRELATIVEX) / (m_fBarWidth + BAR_HORIZONTAL_PADDING)), SIZE - 1);

    CWindow*    pWindowInsertAfter = m_pWindow->getGroupWindowByIndex(WINDOWINDEX);

    pWindowInsertAfter->insertWindowToGroup(m_pDraggedWindow);

    if (WINDOWINDEX == -1)
        std::swap(m_pDraggedWindow->m_sGroupData.head, m_pDraggedWindow->m_sGroupData.pNextWindow->m_sGroupData.head);
}

void CHyprGroupBarDecoration::clickDecoration(const Vector2D& pos) {
    const int   SIZE         = m_dwGroupMembers.size();
    const int   ROUNDING     = m_pWindow->getRealRounding();
    const float BARRELATIVEX = pos.x - (m_vLastWindowPos.x + ROUNDING);
    const int   WINDOWINDEX  = (BARRELATIVEX) / (m_fBarWidth + BAR_HORIZONTAL_PADDING);

    if (BARRELATIVEX - (m_fBarWidth + BAR_HORIZONTAL_PADDING) * WINDOWINDEX < BAR_HORIZONTAL_PADDING || WINDOWINDEX > SIZE - 1) {
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

void CHyprGroupBarDecoration::dragFromDecoration(const Vector2D& pos) {
    const int   SIZE         = m_dwGroupMembers.size();
    const int   ROUNDING     = m_pWindow->getRealRounding();
    const float BARRELATIVEX = pos.x - (m_vLastWindowPos.x + ROUNDING);
    const int   WINDOWINDEX  = (BARRELATIVEX) / (m_fBarWidth + BAR_HORIZONTAL_PADDING);

    if (BARRELATIVEX - (m_fBarWidth + BAR_HORIZONTAL_PADDING) * WINDOWINDEX < BAR_HORIZONTAL_PADDING || WINDOWINDEX > SIZE - 1)
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
