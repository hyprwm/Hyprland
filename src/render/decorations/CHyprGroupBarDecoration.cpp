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

//

void CHyprGroupBarDecoration::updateWindow(CWindow* pWindow) {
    damageEntire();

    const auto         PWORKSPACE = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);

    const auto         WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    static auto* const PRENDERTITLES  = &g_pConfigManager->getConfigValuePtr("misc:render_titles_in_groupbar")->intValue;
    static auto* const PTITLEFONTSIZE = &g_pConfigManager->getConfigValuePtr("misc:groupbar_titles_font_size")->intValue;

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
    wlr_box dm = {m_vLastWindowPos.x - m_seExtents.topLeft.x, m_vLastWindowPos.y - m_seExtents.topLeft.y, m_vLastWindowSize.x + m_seExtents.topLeft.x + m_seExtents.bottomRight.x,
                  m_seExtents.topLeft.y};
    g_pHyprRenderer->damageBox(&dm);
}

void CHyprGroupBarDecoration::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    // get how many bars we will draw
    int                barsToDraw = m_dwGroupMembers.size();

    static auto* const PRENDERTITLES  = &g_pConfigManager->getConfigValuePtr("misc:render_titles_in_groupbar")->intValue;
    static auto* const PTITLEFONTSIZE = &g_pConfigManager->getConfigValuePtr("misc:groupbar_titles_font_size")->intValue;
    static auto* const PGRADIENTS     = &g_pConfigManager->getConfigValuePtr("misc:groupbar_gradients")->intValue;

    const int          BORDERSIZE = m_pWindow->getRealBorderSize();

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    const int PAD = 2; //2px

    const int BARW = (m_vLastWindowSize.x - PAD * (barsToDraw - 1)) / barsToDraw;

    int       xoff = 0;

    for (int i = 0; i < barsToDraw; ++i) {
        wlr_box rect = {m_vLastWindowPos.x + xoff - pMonitor->vecPosition.x + offset.x,
                        m_vLastWindowPos.y - BAR_PADDING_OUTER_VERT - BORDERSIZE - BAR_INDICATOR_HEIGHT - pMonitor->vecPosition.y + offset.y, BARW, BAR_INDICATOR_HEIGHT};

        if (rect.width <= 0 || rect.height <= 0)
            break;

        scaleBox(&rect, pMonitor->scale);

        static auto* const PGROUPCOLACTIVE         = &g_pConfigManager->getConfigValuePtr("general:col.group_border_active")->data;
        static auto* const PGROUPCOLINACTIVE       = &g_pConfigManager->getConfigValuePtr("general:col.group_border")->data;
        static auto* const PGROUPCOLACTIVELOCKED   = &g_pConfigManager->getConfigValuePtr("general:col.group_border_locked_active")->data;
        static auto* const PGROUPCOLINACTIVELOCKED = &g_pConfigManager->getConfigValuePtr("general:col.group_border_locked")->data;

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
                pTitleTex =
                    m_sTitleTexs.titleTexs
                        .emplace_back(std::make_unique<CTitleTex>(m_dwGroupMembers[i], Vector2D{BARW * pMonitor->scale, (*PTITLEFONTSIZE + 2 * BAR_TEXT_PAD) * pMonitor->scale}))
                        .get();

            rect.height = (*PTITLEFONTSIZE + 2 * BAR_TEXT_PAD) * 0.8 * pMonitor->scale;

            rect.y -= rect.height;
            rect.width = BARW * pMonitor->scale;

            refreshGradients();

            if (*PGRADIENTS)
                g_pHyprOpenGL->renderTexture((m_dwGroupMembers[i] == g_pCompositor->m_pLastWindow ? m_tGradientActive : m_tGradientInactive), &rect, 1.0);

            rect.y -= (*PTITLEFONTSIZE + 2 * BAR_TEXT_PAD) * 0.2 * pMonitor->scale;
            rect.height = (*PTITLEFONTSIZE + 2 * BAR_TEXT_PAD) * pMonitor->scale;

            g_pHyprOpenGL->renderTexture(pTitleTex->tex, &rect, 1.f);
        }

        xoff += PAD + BARW;
    }

    if (*PRENDERTITLES)
        invalidateTextures();
}

SWindowDecorationExtents CHyprGroupBarDecoration::getWindowDecorationReservedArea() {
    static auto* const PRENDERTITLES  = &g_pConfigManager->getConfigValuePtr("misc:render_titles_in_groupbar")->intValue;
    static auto* const PTITLEFONTSIZE = &g_pConfigManager->getConfigValuePtr("misc:groupbar_titles_font_size")->intValue;
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

    static auto* const PTITLEFONTSIZE = &g_pConfigManager->getConfigValuePtr("misc:groupbar_titles_font_size")->intValue;
    static auto* const PTEXTCOLOR     = &g_pConfigManager->getConfigValuePtr("misc:groupbar_text_color")->intValue;

    const CColor       COLOR = CColor(*PTEXTCOLOR);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    // draw title using Pango
    PangoLayout* layout = pango_cairo_create_layout(CAIRO);
    pango_layout_set_text(layout, szContent.c_str(), -1);

    PangoFontDescription* fontDesc = pango_font_description_from_string("Sans");
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

    static auto* const PGROUPCOLACTIVE         = &g_pConfigManager->getConfigValuePtr("general:col.group_border_active")->data;
    static auto* const PGROUPCOLINACTIVE       = &g_pConfigManager->getConfigValuePtr("general:col.group_border")->data;
    static auto* const PGROUPCOLACTIVELOCKED   = &g_pConfigManager->getConfigValuePtr("general:col.group_border_locked_active")->data;
    static auto* const PGROUPCOLINACTIVELOCKED = &g_pConfigManager->getConfigValuePtr("general:col.group_border_locked")->data;

    const bool         GROUPLOCKED  = m_pWindow->getGroupHead()->m_sGroupData.locked;
    const auto* const  PCOLACTIVE   = GROUPLOCKED ? PGROUPCOLACTIVELOCKED : PGROUPCOLACTIVE;
    const auto* const  PCOLINACTIVE = GROUPLOCKED ? PGROUPCOLINACTIVELOCKED : PGROUPCOLINACTIVE;

    renderGradientTo(m_tGradientActive, ((CGradientValueData*)PCOLACTIVE->get())->m_vColors[0]);
    renderGradientTo(m_tGradientInactive, ((CGradientValueData*)PCOLINACTIVE->get())->m_vColors[0]);
}

bool CHyprGroupBarDecoration::allowsInput() {
    return true;
}
