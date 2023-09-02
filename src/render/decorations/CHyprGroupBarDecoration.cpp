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
}

CHyprGroupBarDecoration::~CHyprGroupBarDecoration() {}

SWindowDecorationExtents CHyprGroupBarDecoration::getWindowDecorationExtents() {
    return m_seExtents;
}

eDecorationType CHyprGroupBarDecoration::getDecorationType() {
    return DECORATION_GROUPBAR;
}

constexpr int BAR_INDICATOR_HEIGHT = 3;
constexpr int BAR_INTERNAL_PADDING = 2;
constexpr int BAR_EXTERNAL_PADDING = 2;
constexpr int BAR_TEXT_PAD         = 3;

//

void CHyprGroupBarDecoration::updateWindow(CWindow* pWindow) {
    damageEntire();

    const auto         PWORKSPACE      = g_pCompositor->getWorkspaceByID(pWindow->m_iWorkspaceID);
    const auto         WORKSPACEOFFSET = PWORKSPACE && !pWindow->m_bPinned ? PWORKSPACE->m_vRenderOffset.vec() : Vector2D();

    static auto* const PLOCATIONTOP = &g_pConfigManager->getConfigValuePtr("group:groupbar:top")->intValue;

    if (pWindow->m_vRealPosition.vec() + WORKSPACEOFFSET != m_vLastWindowPos || pWindow->m_vRealSize.vec() != m_vLastWindowSize) {
        const int BORDERSIZE = pWindow->getRealBorderSize();
        m_seExtents          = {{0, *PLOCATIONTOP ? BORDERSIZE + BAR_INTERNAL_PADDING + BAR_EXTERNAL_PADDING + getBarHeight() : 0},
                                {0, *PLOCATIONTOP ? 0 : BORDERSIZE + BAR_INTERNAL_PADDING + BAR_EXTERNAL_PADDING + getBarHeight()}};

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
    wlr_box dm = {m_vLastWindowPos.x, m_vLastWindowPos.y + (m_seExtents.topLeft.y != 0 ? -m_seExtents.topLeft.y : m_vLastWindowSize.y), m_vLastWindowSize.x,
                  m_seExtents.topLeft.y + m_seExtents.bottomRight.y};
    g_pHyprRenderer->damageBox(&dm);
}

void CHyprGroupBarDecoration::draw(CMonitor* pMonitor, float a, const Vector2D& offset) {
    // get how many bars we will draw
    int                barsToDraw = m_dwGroupMembers.size();

    static auto* const PMODE         = &g_pConfigManager->getConfigValuePtr("group:groupbar:mode")->intValue;
    static auto* const PRENDERTITLES = &g_pConfigManager->getConfigValuePtr("group:groupbar:render_titles")->intValue;
    static auto* const PLOCATIONTOP  = &g_pConfigManager->getConfigValuePtr("group:groupbar:top")->intValue;

    const int          BARHEIGHT      = *PMODE != 1 ? g_pConfigManager->getConfigValuePtr("group:groupbar:height")->intValue : BAR_INDICATOR_HEIGHT;
    const int          GRADIENTHEIGHT = *PMODE == 1 ? g_pConfigManager->getConfigValuePtr("group:groupbar:height")->intValue : 0;
    const int          BORDERSIZE     = m_pWindow->getRealBorderSize();

    if (!m_pWindow->m_sSpecialRenderData.decorate)
        return;

    static auto* const PGROUPCOLACTIVE         = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.active")->data;
    static auto* const PGROUPCOLINACTIVE       = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.inactive")->data;
    static auto* const PGROUPCOLACTIVELOCKED   = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_active")->data;
    static auto* const PGROUPCOLINACTIVELOCKED = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_inactive")->data;

    const bool         GROUPLOCKED  = m_pWindow->getGroupHead()->m_sGroupData.locked;
    const auto* const  PCOLACTIVE   = GROUPLOCKED ? PGROUPCOLACTIVELOCKED : PGROUPCOLACTIVE;
    const auto* const  PCOLINACTIVE = GROUPLOCKED ? PGROUPCOLINACTIVELOCKED : PGROUPCOLINACTIVE;

    // TODO: fix mouse event with rounding
    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(m_pWindow->m_iWorkspaceID);
    const int  ROUNDING =
        (m_pWindow->m_bIsFullscreen && PWORKSPACE->m_efFullscreenMode == FULLSCREEN_FULL) || (!m_pWindow->m_sSpecialRenderData.rounding) ? 0 : m_pWindow->rounding();

    const int PAD  = 2; //2px
    const int BARW = (m_vLastWindowSize.x - 2 * ROUNDING - PAD * (barsToDraw - 1)) / barsToDraw;

    //  TODO: check for invalid config
    if (BARW <= 0)
        return;

    // Bottom left of groupbar
    Vector2D pos = Vector2D(m_vLastWindowPos.x - pMonitor->vecPosition.x + offset.x + ROUNDING,
                            m_vLastWindowPos.y - pMonitor->vecPosition.y + offset.y +
                                (*PLOCATIONTOP ? -BORDERSIZE : m_vLastWindowSize.y + BORDERSIZE + getBarHeight() + BAR_INTERNAL_PADDING));

    wlr_box  barBox  = {pos.x, pos.y - BARHEIGHT + (*PLOCATIONTOP ? -BAR_INTERNAL_PADDING : 0), BARW, BARHEIGHT};
    wlr_box  gradBox = {pos.x, pos.y - BARHEIGHT + (*PLOCATIONTOP ? -2 * BAR_INTERNAL_PADDING : -BAR_INTERNAL_PADDING) - GRADIENTHEIGHT, BARW, GRADIENTHEIGHT};
    wlr_box  textBox = *PMODE == 1 ? gradBox : barBox;
    textBox.y += BAR_TEXT_PAD;
    textBox.height -= 2 * BAR_TEXT_PAD;

    scaleBox(&barBox, pMonitor->scale);
    scaleBox(&textBox, pMonitor->scale);
    scaleBox(&gradBox, pMonitor->scale);

    for (int i = 0; i < barsToDraw; ++i) {

        CColor color =
            m_dwGroupMembers[i] == g_pCompositor->m_pLastWindow ? ((CGradientValueData*)PCOLACTIVE->get())->m_vColors[0] : ((CGradientValueData*)PCOLINACTIVE->get())->m_vColors[0];
        color.a *= a;
        g_pHyprOpenGL->renderRect(&barBox, color);

        // render title if necessary
        if (*PRENDERTITLES) {
            CTitleTex* pTitleTex = textureFromTitle(m_dwGroupMembers[i]->m_szTitle);
            if (!pTitleTex)
                pTitleTex = m_sTitleTexs.titleTexs.emplace_back(std::make_unique<CTitleTex>(m_dwGroupMembers[i], Vector2D(textBox.width, textBox.height))).get();
            g_pHyprOpenGL->renderTexture(pTitleTex->tex, &textBox, 1.f);
        }

        if (*PMODE == 1) {
            refreshGradients();
            g_pHyprOpenGL->renderTexture((m_dwGroupMembers[i] == g_pCompositor->m_pLastWindow ? (GROUPLOCKED ? m_tGradientLockedActive : m_tGradientActive) :
                                                                                                (GROUPLOCKED ? m_tGradientLockedInactive : m_tGradientInactive)),
                                         &gradBox, 1.0);
        }

        barBox.x += (PAD + BARW) * pMonitor->scale;
        gradBox.x += (PAD + BARW) * pMonitor->scale;
        textBox.x += (PAD + BARW) * pMonitor->scale;
    }

    if (*PRENDERTITLES)
        invalidateTextures();
}

SWindowDecorationExtents CHyprGroupBarDecoration::getWindowDecorationReservedArea() {
    static auto* const PLOCATIONTOP = &g_pConfigManager->getConfigValuePtr("group:groupbar:top")->intValue;
    return {{0, *PLOCATIONTOP ? +BAR_INTERNAL_PADDING + BAR_EXTERNAL_PADDING + getBarHeight() : 0},
            {0, *PLOCATIONTOP ? 0 : BAR_INTERNAL_PADDING + BAR_EXTERNAL_PADDING + getBarHeight()}};
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
    if (m_tGradientActive.m_iTexID > 0)
        return;

    static auto* const PGROUPCOLACTIVE         = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.active")->data;
    static auto* const PGROUPCOLINACTIVE       = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.inactive")->data;
    static auto* const PGROUPCOLACTIVELOCKED   = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_active")->data;
    static auto* const PGROUPCOLINACTIVELOCKED = &g_pConfigManager->getConfigValuePtr("group:groupbar:col.locked_inactive")->data;

    renderGradientTo(m_tGradientActive, ((CGradientValueData*)PGROUPCOLACTIVE->get())->m_vColors[0]);
    renderGradientTo(m_tGradientInactive, ((CGradientValueData*)PGROUPCOLINACTIVE->get())->m_vColors[0]);
    renderGradientTo(m_tGradientLockedActive, ((CGradientValueData*)PGROUPCOLACTIVELOCKED->get())->m_vColors[0]);
    renderGradientTo(m_tGradientLockedInactive, ((CGradientValueData*)PGROUPCOLINACTIVELOCKED->get())->m_vColors[0]);
}

int CHyprGroupBarDecoration::getBarHeight() {
    static auto* const PMODE   = &g_pConfigManager->getConfigValuePtr("group:groupbar:mode")->intValue;
    static auto* const PHEIGHT = &g_pConfigManager->getConfigValuePtr("group:groupbar:height")->intValue;
    return *PHEIGHT + (*PMODE == 1 ? BAR_INDICATOR_HEIGHT + BAR_INTERNAL_PADDING : 0);
}

void CHyprGroupBarDecoration::forceReload(CWindow* pWindow) {
    m_tGradientActive.destroyTexture();
    m_tGradientInactive.destroyTexture();
    updateWindow(pWindow);
}

bool CHyprGroupBarDecoration::allowsInput() {
    return true;
}
