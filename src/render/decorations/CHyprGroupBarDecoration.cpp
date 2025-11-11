#include "CHyprGroupBarDecoration.hpp"
#include "../../Compositor.hpp"
#include "../../config/ConfigValue.hpp"
#include "managers/LayoutManager.hpp"
#include <ranges>
#include <pango/pangocairo.h>
#include "../pass/TexPassElement.hpp"
#include "../pass/RectPassElement.hpp"
#include "../Renderer.hpp"
#include "../../managers/input/InputManager.hpp"

// shared things to conserve VRAM
static SP<CTexture> m_tGradientActive         = makeShared<CTexture>();
static SP<CTexture> m_tGradientInactive       = makeShared<CTexture>();
static SP<CTexture> m_tGradientLockedActive   = makeShared<CTexture>();
static SP<CTexture> m_tGradientLockedInactive = makeShared<CTexture>();

constexpr int       BAR_TEXT_PAD = 2;

CHyprGroupBarDecoration::CHyprGroupBarDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_window(pWindow) {
    static auto PGRADIENTS = CConfigValue<Hyprlang::INT>("group:groupbar:enabled");
    static auto PENABLED   = CConfigValue<Hyprlang::INT>("group:groupbar:gradients");

    if (m_tGradientActive->m_texID == 0 && *PENABLED && *PGRADIENTS)
        refreshGroupBarGradients();
}

SDecorationPositioningInfo CHyprGroupBarDecoration::getPositioningInfo() {
    static auto                PHEIGHT          = CConfigValue<Hyprlang::INT>("group:groupbar:height");
    static auto                PINDICATORGAP    = CConfigValue<Hyprlang::INT>("group:groupbar:indicator_gap");
    static auto                PINDICATORHEIGHT = CConfigValue<Hyprlang::INT>("group:groupbar:indicator_height");
    static auto                PRENDERTITLES    = CConfigValue<Hyprlang::INT>("group:groupbar:render_titles");
    static auto                PGRADIENTS       = CConfigValue<Hyprlang::INT>("group:groupbar:gradients");
    static auto                PPRIORITY        = CConfigValue<Hyprlang::INT>("group:groupbar:priority");
    static auto                PSTACKED         = CConfigValue<Hyprlang::INT>("group:groupbar:stacked");
    static auto                POUTERGAP        = CConfigValue<Hyprlang::INT>("group:groupbar:gaps_out");
    static auto                PKEEPUPPERGAP    = CConfigValue<Hyprlang::INT>("group:groupbar:keep_upper_gap");

    SDecorationPositioningInfo info;
    info.policy   = DECORATION_POSITION_STICKY;
    info.edges    = DECORATION_EDGE_TOP;
    info.priority = *PPRIORITY;
    info.reserved = true;

    if (visible()) {
        if (*PSTACKED) {
            const auto ONEBARHEIGHT = *POUTERGAP + *PINDICATORHEIGHT + *PINDICATORGAP + (*PGRADIENTS || *PRENDERTITLES ? *PHEIGHT : 0);
            info.desiredExtents     = {{0, (ONEBARHEIGHT * m_dwGroupMembers.size()) + (*PKEEPUPPERGAP * *POUTERGAP)}, {0, 0}};
        } else
            info.desiredExtents = {{0, *POUTERGAP * (1 + *PKEEPUPPERGAP) + *PINDICATORHEIGHT + *PINDICATORGAP + (*PGRADIENTS || *PRENDERTITLES ? *PHEIGHT : 0)}, {0, 0}};
    } else
        info.desiredExtents = {{0, 0}, {0, 0}};
    return info;
}

void CHyprGroupBarDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    m_assignedBox = reply.assignedGeometry;
}

eDecorationType CHyprGroupBarDecoration::getDecorationType() {
    return DECORATION_GROUPBAR;
}

//

void CHyprGroupBarDecoration::updateWindow(PHLWINDOW pWindow) {
    if (m_window->m_groupData.pNextWindow.expired()) {
        m_window->removeWindowDeco(this);
        return;
    }

    m_dwGroupMembers.clear();
    PHLWINDOW head = pWindow->getGroupHead();
    m_dwGroupMembers.emplace_back(head);

    PHLWINDOW curr = head->m_groupData.pNextWindow.lock();
    while (curr != head) {
        m_dwGroupMembers.emplace_back(curr);
        curr = curr->m_groupData.pNextWindow.lock();
    }

    damageEntire();

    if (m_dwGroupMembers.empty()) {
        m_window->removeWindowDeco(this);
        return;
    }
}

void CHyprGroupBarDecoration::damageEntire() {
    auto box = assignedBoxGlobal();
    box.translate(m_window->m_floatingOffset);
    g_pHyprRenderer->damageBox(box);
}

void CHyprGroupBarDecoration::draw(PHLMONITOR pMonitor, float const& a) {
    // get how many bars we will draw
    int        barsToDraw = m_dwGroupMembers.size();

    const bool VISIBLE = visible();

    if (VISIBLE != m_bLastVisibilityStatus)
        g_pDecorationPositioner->repositionDeco(this);

    if (!VISIBLE)
        return;

    static auto PRENDERTITLES              = CConfigValue<Hyprlang::INT>("group:groupbar:render_titles");
    static auto PTITLEFONTSIZE             = CConfigValue<Hyprlang::INT>("group:groupbar:font_size");
    static auto PHEIGHT                    = CConfigValue<Hyprlang::INT>("group:groupbar:height");
    static auto PINDICATORGAP              = CConfigValue<Hyprlang::INT>("group:groupbar:indicator_gap");
    static auto PINDICATORHEIGHT           = CConfigValue<Hyprlang::INT>("group:groupbar:indicator_height");
    static auto PGRADIENTS                 = CConfigValue<Hyprlang::INT>("group:groupbar:gradients");
    static auto PSTACKED                   = CConfigValue<Hyprlang::INT>("group:groupbar:stacked");
    static auto PROUNDING                  = CConfigValue<Hyprlang::INT>("group:groupbar:rounding");
    static auto PROUNDINGPOWER             = CConfigValue<Hyprlang::FLOAT>("group:groupbar:rounding_power");
    static auto PGRADIENTROUNDING          = CConfigValue<Hyprlang::INT>("group:groupbar:gradient_rounding");
    static auto PGRADIENTROUNDINGPOWER     = CConfigValue<Hyprlang::FLOAT>("group:groupbar:gradient_rounding_power");
    static auto PGRADIENTROUNDINGONLYEDGES = CConfigValue<Hyprlang::INT>("group:groupbar:gradient_round_only_edges");
    static auto PROUNDONLYEDGES            = CConfigValue<Hyprlang::INT>("group:groupbar:round_only_edges");
    static auto PGROUPCOLACTIVE            = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:col.active");
    static auto PGROUPCOLINACTIVE          = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:col.inactive");
    static auto PGROUPCOLACTIVELOCKED      = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:col.locked_active");
    static auto PGROUPCOLINACTIVELOCKED    = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:col.locked_inactive");
    static auto POUTERGAP                  = CConfigValue<Hyprlang::INT>("group:groupbar:gaps_out");
    static auto PINNERGAP                  = CConfigValue<Hyprlang::INT>("group:groupbar:gaps_in");
    static auto PKEEPUPPERGAP              = CConfigValue<Hyprlang::INT>("group:groupbar:keep_upper_gap");
    static auto PTEXTOFFSET                = CConfigValue<Hyprlang::INT>("group:groupbar:text_offset");
    static auto PBLUR                      = CConfigValue<Hyprlang::INT>("group:groupbar:blur");
    auto* const GROUPCOLACTIVE             = sc<CGradientValueData*>((PGROUPCOLACTIVE.ptr())->getData());
    auto* const GROUPCOLINACTIVE           = sc<CGradientValueData*>((PGROUPCOLINACTIVE.ptr())->getData());
    auto* const GROUPCOLACTIVELOCKED       = sc<CGradientValueData*>((PGROUPCOLACTIVELOCKED.ptr())->getData());
    auto* const GROUPCOLINACTIVELOCKED     = sc<CGradientValueData*>((PGROUPCOLINACTIVELOCKED.ptr())->getData());

    const auto  ASSIGNEDBOX = assignedBoxGlobal();

    const auto  ONEBARHEIGHT = *POUTERGAP + *PINDICATORHEIGHT + *PINDICATORGAP + (*PGRADIENTS || *PRENDERTITLES ? *PHEIGHT : 0);
    m_barWidth               = *PSTACKED ? ASSIGNEDBOX.w : (ASSIGNEDBOX.w - *PINNERGAP * (barsToDraw - 1)) / barsToDraw;
    m_barHeight              = *PSTACKED ? ((ASSIGNEDBOX.h - *POUTERGAP * *PKEEPUPPERGAP) - *POUTERGAP * (barsToDraw)) / barsToDraw : ASSIGNEDBOX.h - *POUTERGAP * *PKEEPUPPERGAP;

    const auto DESIREDHEIGHT = *PSTACKED ? (ONEBARHEIGHT * m_dwGroupMembers.size()) + *POUTERGAP * *PKEEPUPPERGAP : *POUTERGAP * (1 + *PKEEPUPPERGAP) + ONEBARHEIGHT;
    if (DESIREDHEIGHT != ASSIGNEDBOX.h)
        g_pDecorationPositioner->repositionDeco(this);

    float xoff = 0;
    float yoff = 0;

    bool  blur = *PBLUR != 0;

    for (int i = 0; i < barsToDraw; ++i) {
        const auto WINDOWINDEX = *PSTACKED ? m_dwGroupMembers.size() - i - 1 : i;

        CBox       rect = {ASSIGNEDBOX.x + xoff - pMonitor->m_position.x + m_window->m_floatingOffset.x,
                           ASSIGNEDBOX.y + ASSIGNEDBOX.h - floor(yoff) - *PINDICATORHEIGHT - *POUTERGAP - pMonitor->m_position.y + m_window->m_floatingOffset.y, m_barWidth,
                           *PINDICATORHEIGHT};

        rect.scale(pMonitor->m_scale).round();

        const bool        GROUPLOCKED  = m_window->getGroupHead()->m_groupData.locked || g_pKeybindManager->m_groupsLocked;
        const auto* const PCOLACTIVE   = GROUPLOCKED ? GROUPCOLACTIVELOCKED : GROUPCOLACTIVE;
        const auto* const PCOLINACTIVE = GROUPLOCKED ? GROUPCOLINACTIVELOCKED : GROUPCOLINACTIVE;

        CHyprColor        color = m_dwGroupMembers[WINDOWINDEX].lock() == g_pCompositor->m_lastWindow.lock() ? PCOLACTIVE->m_colors[0] : PCOLINACTIVE->m_colors[0];
        color.a *= a;

        if (!rect.empty()) {
            CRectPassElement::SRectData rectdata;
            rectdata.color = color;
            rectdata.blur  = blur;
            rectdata.box   = rect;
            if (*PROUNDING) {
                rectdata.round         = *PROUNDING;
                rectdata.roundingPower = *PROUNDINGPOWER;
                if (*PROUNDONLYEDGES) {
                    if (i == 0) {
                        rectdata.round   = *PROUNDING;
                        rectdata.clipBox = rect;
                        rectdata.box     = CBox{rect.pos(), Vector2D{rect.w + (*PROUNDING * 2), rect.h}};
                    } else if (i == barsToDraw - 1) {
                        double offset    = *PROUNDING * 2;
                        rectdata.round   = *PROUNDING;
                        rectdata.clipBox = rect;
                        rectdata.box     = CBox{rect.pos() - Vector2D{offset, 0.F}, Vector2D{rect.w + offset, rect.h}};
                    }
                }
            }
            g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectdata));
        }

        rect = {ASSIGNEDBOX.x + xoff - pMonitor->m_position.x + m_window->m_floatingOffset.x,
                ASSIGNEDBOX.y + ASSIGNEDBOX.h - floor(yoff) - ONEBARHEIGHT - pMonitor->m_position.y + m_window->m_floatingOffset.y, m_barWidth,
                (*PGRADIENTS || *PRENDERTITLES ? *PHEIGHT : 0)};
        rect.scale(pMonitor->m_scale);

        if (!rect.empty()) {
            if (*PGRADIENTS) {
                const auto GRADIENTTEX = (m_dwGroupMembers[WINDOWINDEX] == g_pCompositor->m_lastWindow ? (GROUPLOCKED ? m_tGradientLockedActive : m_tGradientActive) :
                                                                                                         (GROUPLOCKED ? m_tGradientLockedInactive : m_tGradientInactive));
                if (GRADIENTTEX->m_texID) {
                    CTexPassElement::SRenderData data;
                    data.tex  = GRADIENTTEX;
                    data.blur = blur;
                    data.box  = rect;
                    if (*PGRADIENTROUNDING) {
                        data.round         = *PGRADIENTROUNDING;
                        data.roundingPower = *PGRADIENTROUNDINGPOWER;
                        if (*PGRADIENTROUNDINGONLYEDGES) {
                            if (i == 0) {
                                data.round   = *PGRADIENTROUNDING;
                                data.clipBox = rect;
                                data.box     = CBox{rect.pos(), Vector2D{rect.w + (*PGRADIENTROUNDING * 2), rect.h}};
                            } else if (i == barsToDraw - 1) {
                                double offset = *PGRADIENTROUNDING * 2;
                                data.round    = *PGRADIENTROUNDING;
                                data.clipBox  = rect;
                                data.box      = CBox{rect.pos() - Vector2D{offset, 0.F}, Vector2D{rect.w + offset, rect.h}};
                            }
                        }
                    }
                    g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(data));
                }
            }

            if (*PRENDERTITLES) {
                CTitleTex* pTitleTex = textureFromTitle(m_dwGroupMembers[WINDOWINDEX]->m_title);

                if (!pTitleTex)
                    pTitleTex = m_titleTexs.titleTexs
                                    .emplace_back(makeUnique<CTitleTex>(m_dwGroupMembers[WINDOWINDEX].lock(),
                                                                        Vector2D{m_barWidth * pMonitor->m_scale, (*PTITLEFONTSIZE + 2L * BAR_TEXT_PAD) * pMonitor->m_scale},
                                                                        pMonitor->m_scale))
                                    .get();

                SP<CTexture> titleTex;
                if (m_dwGroupMembers[WINDOWINDEX] == g_pCompositor->m_lastWindow)
                    titleTex = GROUPLOCKED ? pTitleTex->m_texLockedActive : pTitleTex->m_texActive;
                else
                    titleTex = GROUPLOCKED ? pTitleTex->m_texLockedInactive : pTitleTex->m_texInactive;

                rect.y += std::ceil(((rect.height - titleTex->m_size.y) / 2.0) - (*PTEXTOFFSET * pMonitor->m_scale));
                rect.height = titleTex->m_size.y;
                rect.width  = titleTex->m_size.x;
                rect.x += std::round(((m_barWidth * pMonitor->m_scale) / 2.0) - (titleTex->m_size.x / 2.0));
                rect.round();

                CTexPassElement::SRenderData data;
                data.tex = titleTex;
                data.box = rect;
                data.a   = a;
                g_pHyprRenderer->m_renderPass.add(makeUnique<CTexPassElement>(std::move(data)));
            }
        }

        if (*PSTACKED)
            yoff += ONEBARHEIGHT;
        else
            xoff += *PINNERGAP + m_barWidth;
    }

    if (*PRENDERTITLES)
        invalidateTextures();
}

CTitleTex* CHyprGroupBarDecoration::textureFromTitle(const std::string& title) {
    for (auto const& tex : m_titleTexs.titleTexs) {
        if (tex->m_content == title)
            return tex.get();
    }

    return nullptr;
}

void CHyprGroupBarDecoration::invalidateTextures() {
    m_titleTexs.titleTexs.clear();
}

CTitleTex::CTitleTex(PHLWINDOW pWindow, const Vector2D& bufferSize, const float monitorScale) : m_content(pWindow->m_title), m_windowOwner(pWindow) {
    static auto      FALLBACKFONT             = CConfigValue<std::string>("misc:font_family");
    static auto      PTITLEFONTFAMILY         = CConfigValue<std::string>("group:groupbar:font_family");
    static auto      PTITLEFONTSIZE           = CConfigValue<Hyprlang::INT>("group:groupbar:font_size");
    static auto      PTEXTCOLORACTIVE         = CConfigValue<Hyprlang::INT>("group:groupbar:text_color");
    static auto      PTEXTCOLORINACTIVE       = CConfigValue<Hyprlang::INT>("group:groupbar:text_color_inactive");
    static auto      PTEXTCOLORLOCKEDACTIVE   = CConfigValue<Hyprlang::INT>("group:groupbar:text_color_locked_active");
    static auto      PTEXTCOLORLOCKEDINACTIVE = CConfigValue<Hyprlang::INT>("group:groupbar:text_color_locked_inactive");

    static auto      PTITLEFONTWEIGHTACTIVE   = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:font_weight_active");
    static auto      PTITLEFONTWEIGHTINACTIVE = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:font_weight_inactive");

    const auto       FONTWEIGHTACTIVE   = sc<CFontWeightConfigValueData*>((PTITLEFONTWEIGHTACTIVE.ptr())->getData());
    const auto       FONTWEIGHTINACTIVE = sc<CFontWeightConfigValueData*>((PTITLEFONTWEIGHTINACTIVE.ptr())->getData());

    const CHyprColor COLORACTIVE         = CHyprColor(*PTEXTCOLORACTIVE);
    const CHyprColor COLORINACTIVE       = *PTEXTCOLORINACTIVE == -1 ? COLORACTIVE : CHyprColor(*PTEXTCOLORINACTIVE);
    const CHyprColor COLORLOCKEDACTIVE   = *PTEXTCOLORLOCKEDACTIVE == -1 ? COLORACTIVE : CHyprColor(*PTEXTCOLORLOCKEDACTIVE);
    const CHyprColor COLORLOCKEDINACTIVE = *PTEXTCOLORLOCKEDINACTIVE == -1 ? COLORINACTIVE : CHyprColor(*PTEXTCOLORLOCKEDINACTIVE);

    const auto       FONTFAMILY = *PTITLEFONTFAMILY != STRVAL_EMPTY ? *PTITLEFONTFAMILY : *FALLBACKFONT;

#define RENDER_TEXT(color, weight) g_pHyprOpenGL->renderText(pWindow->m_title, (color), *PTITLEFONTSIZE* monitorScale, false, FONTFAMILY, bufferSize.x - 2, (weight));
    m_texActive         = RENDER_TEXT(COLORACTIVE, FONTWEIGHTACTIVE->m_value);
    m_texInactive       = RENDER_TEXT(COLORINACTIVE, FONTWEIGHTINACTIVE->m_value);
    m_texLockedActive   = RENDER_TEXT(COLORLOCKEDACTIVE, FONTWEIGHTACTIVE->m_value);
    m_texLockedInactive = RENDER_TEXT(COLORLOCKEDINACTIVE, FONTWEIGHTINACTIVE->m_value);
#undef RENDER_TEXT
}

static void renderGradientTo(SP<CTexture> tex, CGradientValueData* grad) {

    if (!g_pCompositor->m_lastMonitor)
        return;

    const Vector2D& bufferSize = g_pCompositor->m_lastMonitor->m_pixelSize;

    const auto      CAIROSURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, bufferSize.x, bufferSize.y);
    const auto      CAIRO        = cairo_create(CAIROSURFACE);

    // clear the pixmap
    cairo_save(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_restore(CAIRO);

    cairo_pattern_t* pattern;
    pattern = cairo_pattern_create_linear(0, 0, 0, bufferSize.y);

    for (unsigned long i = 0; i < grad->m_colors.size(); i++) {
        cairo_pattern_add_color_stop_rgba(pattern, 1 - sc<double>(i + 1) / (grad->m_colors.size() + 1), grad->m_colors[i].r, grad->m_colors[i].g, grad->m_colors[i].b,
                                          grad->m_colors[i].a);
    }

    cairo_rectangle(CAIRO, 0, 0, bufferSize.x, bufferSize.y);
    cairo_set_source(CAIRO, pattern);
    cairo_fill(CAIRO);
    cairo_pattern_destroy(pattern);

    cairo_surface_flush(CAIROSURFACE);

    // copy the data to an OpenGL texture we have
    const auto DATA = cairo_image_surface_get_data(CAIROSURFACE);
    tex->allocate();
    tex->bind();
    tex->setTexParameter(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    tex->setTexParameter(GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_R, GL_BLUE);
    tex->setTexParameter(GL_TEXTURE_SWIZZLE_B, GL_RED);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bufferSize.x, bufferSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, DATA);

    // delete cairo
    cairo_destroy(CAIRO);
    cairo_surface_destroy(CAIROSURFACE);
}

void refreshGroupBarGradients() {
    static auto PGRADIENTS = CConfigValue<Hyprlang::INT>("group:groupbar:enabled");
    static auto PENABLED   = CConfigValue<Hyprlang::INT>("group:groupbar:gradients");

    static auto PGROUPCOLACTIVE         = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:col.active");
    static auto PGROUPCOLINACTIVE       = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:col.inactive");
    static auto PGROUPCOLACTIVELOCKED   = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:col.locked_active");
    static auto PGROUPCOLINACTIVELOCKED = CConfigValue<Hyprlang::CUSTOMTYPE>("group:groupbar:col.locked_inactive");
    auto* const GROUPCOLACTIVE          = sc<CGradientValueData*>((PGROUPCOLACTIVE.ptr())->getData());
    auto* const GROUPCOLINACTIVE        = sc<CGradientValueData*>((PGROUPCOLINACTIVE.ptr())->getData());
    auto* const GROUPCOLACTIVELOCKED    = sc<CGradientValueData*>((PGROUPCOLACTIVELOCKED.ptr())->getData());
    auto* const GROUPCOLINACTIVELOCKED  = sc<CGradientValueData*>((PGROUPCOLINACTIVELOCKED.ptr())->getData());

    g_pHyprRenderer->makeEGLCurrent();

    if (m_tGradientActive->m_texID != 0) {
        m_tGradientActive->destroyTexture();
        m_tGradientInactive->destroyTexture();
        m_tGradientLockedActive->destroyTexture();
        m_tGradientLockedInactive->destroyTexture();
    }

    if (!*PENABLED || !*PGRADIENTS)
        return;

    renderGradientTo(m_tGradientActive, GROUPCOLACTIVE);
    renderGradientTo(m_tGradientInactive, GROUPCOLINACTIVE);
    renderGradientTo(m_tGradientLockedActive, GROUPCOLACTIVELOCKED);
    renderGradientTo(m_tGradientLockedInactive, GROUPCOLINACTIVELOCKED);
}

bool CHyprGroupBarDecoration::onBeginWindowDragOnDeco(const Vector2D& pos) {
    static auto PSTACKED  = CConfigValue<Hyprlang::INT>("group:groupbar:stacked");
    static auto POUTERGAP = CConfigValue<Hyprlang::INT>("group:groupbar:gaps_out");
    static auto PINNERGAP = CConfigValue<Hyprlang::INT>("group:groupbar:gaps_in");
    if (m_window.lock() == m_window->m_groupData.pNextWindow.lock())
        return false;

    const float BARRELATIVEX = pos.x - assignedBoxGlobal().x;
    const float BARRELATIVEY = pos.y - assignedBoxGlobal().y;
    const int   WINDOWINDEX  = *PSTACKED ? (BARRELATIVEY / (m_barHeight + *POUTERGAP)) : (BARRELATIVEX) / (m_barWidth + *PINNERGAP);

    if (!*PSTACKED && (BARRELATIVEX - (m_barWidth + *PINNERGAP) * WINDOWINDEX > m_barWidth))
        return false;

    if (*PSTACKED && (BARRELATIVEY - (m_barHeight + *POUTERGAP) * WINDOWINDEX < *POUTERGAP))
        return false;

    PHLWINDOW pWindow = m_window->getGroupWindowByIndex(WINDOWINDEX);

    // hack
    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(pWindow);
    if (!pWindow->m_isFloating) {
        const bool GROUPSLOCKEDPREV       = g_pKeybindManager->m_groupsLocked;
        g_pKeybindManager->m_groupsLocked = true;
        g_pLayoutManager->getCurrentLayout()->onWindowCreated(pWindow);
        g_pKeybindManager->m_groupsLocked = GROUPSLOCKEDPREV;
    }

    g_pInputManager->m_currentlyDraggedWindow = pWindow;

    if (!g_pCompositor->isWindowActive(pWindow))
        g_pCompositor->focusWindow(pWindow);

    return true;
}

bool CHyprGroupBarDecoration::onEndWindowDragOnDeco(const Vector2D& pos, PHLWINDOW pDraggedWindow) {
    static auto PSTACKED                         = CConfigValue<Hyprlang::INT>("group:groupbar:stacked");
    static auto PDRAGINTOGROUP                   = CConfigValue<Hyprlang::INT>("group:drag_into_group");
    static auto PMERGEFLOATEDINTOTILEDONGROUPBAR = CConfigValue<Hyprlang::INT>("group:merge_floated_into_tiled_on_groupbar");
    static auto PMERGEGROUPSONGROUPBAR           = CConfigValue<Hyprlang::INT>("group:merge_groups_on_groupbar");
    static auto POUTERGAP                        = CConfigValue<Hyprlang::INT>("group:groupbar:gaps_out");
    static auto PINNERGAP                        = CConfigValue<Hyprlang::INT>("group:groupbar:gaps_in");
    const bool  FLOATEDINTOTILED                 = !m_window->m_isFloating && !pDraggedWindow->m_draggingTiled;

    g_pInputManager->m_wasDraggingWindow = false;

    if (!pDraggedWindow->canBeGroupedInto(m_window.lock()) || (*PDRAGINTOGROUP != 1 && *PDRAGINTOGROUP != 2) || (FLOATEDINTOTILED && !*PMERGEFLOATEDINTOTILEDONGROUPBAR) ||
        (!*PMERGEGROUPSONGROUPBAR && pDraggedWindow->m_groupData.pNextWindow.lock() && m_window->m_groupData.pNextWindow.lock())) {
        g_pInputManager->m_wasDraggingWindow = true;
        return false;
    }

    const float BARRELATIVE = *PSTACKED ? pos.y - assignedBoxGlobal().y - (m_barHeight + *POUTERGAP) / 2 : pos.x - assignedBoxGlobal().x - m_barWidth / 2;
    const float BARSIZE     = *PSTACKED ? m_barHeight + *POUTERGAP : m_barWidth + *PINNERGAP;
    const int   WINDOWINDEX = BARRELATIVE < 0 ? -1 : BARRELATIVE / BARSIZE;

    PHLWINDOW   pWindowInsertAfter = m_window->getGroupWindowByIndex(WINDOWINDEX);
    PHLWINDOW   pWindowInsertEnd   = pWindowInsertAfter->m_groupData.pNextWindow.lock();
    PHLWINDOW   pDraggedHead       = pDraggedWindow->m_groupData.pNextWindow.lock() ? pDraggedWindow->getGroupHead() : pDraggedWindow;

    if (!pDraggedWindow->m_groupData.pNextWindow.expired()) {

        // stores group data
        std::vector<PHLWINDOW> members;
        PHLWINDOW              curr      = pDraggedHead;
        const bool             WASLOCKED = pDraggedHead->m_groupData.locked;
        do {
            members.push_back(curr);
            curr = curr->m_groupData.pNextWindow.lock();
        } while (curr != members[0]);

        // removes all windows
        for (const PHLWINDOW& w : members) {
            w->m_groupData.pNextWindow.reset();
            w->m_groupData.head   = false;
            w->m_groupData.locked = false;
            g_pLayoutManager->getCurrentLayout()->onWindowRemoved(w);
        }

        // restores the group
        for (auto it = members.begin(); it != members.end(); ++it) {
            (*it)->m_isFloating    = pWindowInsertAfter->m_isFloating;           // match the floating state of group members
            *(*it)->m_realSize     = pWindowInsertAfter->m_realSize->goal();     // match the size of group members
            *(*it)->m_realPosition = pWindowInsertAfter->m_realPosition->goal(); // match the position of group members
            if (std::next(it) != members.end())
                (*it)->m_groupData.pNextWindow = *std::next(it);
            else
                (*it)->m_groupData.pNextWindow = members[0];
        }
        members[0]->m_groupData.head   = true;
        members[0]->m_groupData.locked = WASLOCKED;
    } else
        g_pLayoutManager->getCurrentLayout()->onWindowRemoved(pDraggedWindow);

    pDraggedWindow->m_isFloating = pWindowInsertAfter->m_isFloating; // match the floating state of the window

    pWindowInsertAfter->insertWindowToGroup(pDraggedWindow);

    if (WINDOWINDEX == -1)
        std::swap(pDraggedHead->m_groupData.head, pWindowInsertEnd->m_groupData.head);

    m_window->setGroupCurrent(pDraggedWindow);
    pDraggedWindow->applyGroupRules();
    pDraggedWindow->updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(pDraggedWindow);

    if (!pDraggedWindow->getDecorationByType(DECORATION_GROUPBAR))
        pDraggedWindow->addWindowDeco(makeUnique<CHyprGroupBarDecoration>(pDraggedWindow));

    return true;
}

bool CHyprGroupBarDecoration::onMouseButtonOnDeco(const Vector2D& pos, const IPointer::SButtonEvent& e) {
    static auto PSTACKED  = CConfigValue<Hyprlang::INT>("group:groupbar:stacked");
    static auto POUTERGAP = CConfigValue<Hyprlang::INT>("group:groupbar:gaps_out");
    static auto PINNERGAP = CConfigValue<Hyprlang::INT>("group:groupbar:gaps_in");
    if (m_window->isEffectiveInternalFSMode(FSMODE_FULLSCREEN))
        return true;

    const float BARRELATIVEX = pos.x - assignedBoxGlobal().x;
    const float BARRELATIVEY = pos.y - assignedBoxGlobal().y;
    const int   WINDOWINDEX  = *PSTACKED ? (BARRELATIVEY / (m_barHeight + *POUTERGAP)) : (BARRELATIVEX) / (m_barWidth + *PINNERGAP);
    static auto PFOLLOWMOUSE = CConfigValue<Hyprlang::INT>("input:follow_mouse");

    // close window on middle click
    if (e.button == 274) {
        static Vector2D pressedCursorPos;

        if (e.state == WL_POINTER_BUTTON_STATE_PRESSED)
            pressedCursorPos = pos;
        else if (e.state == WL_POINTER_BUTTON_STATE_RELEASED && pressedCursorPos == pos)
            g_pXWaylandManager->sendCloseWindow(m_window->getGroupWindowByIndex(WINDOWINDEX));

        return true;
    }

    if (e.state != WL_POINTER_BUTTON_STATE_PRESSED)
        return true;

    // click on padding
    const auto TABPAD   = !*PSTACKED && (BARRELATIVEX - (m_barWidth + *PINNERGAP) * WINDOWINDEX > m_barWidth);
    const auto STACKPAD = *PSTACKED && (BARRELATIVEY - (m_barHeight + *POUTERGAP) * WINDOWINDEX < *POUTERGAP);
    if (TABPAD || STACKPAD) {
        if (!g_pCompositor->isWindowActive(m_window.lock()))
            g_pCompositor->focusWindow(m_window.lock());
        return true;
    }

    PHLWINDOW pWindow = m_window->getGroupWindowByIndex(WINDOWINDEX);

    if (pWindow != m_window)
        pWindow->setGroupCurrent(pWindow);

    if (!g_pCompositor->isWindowActive(pWindow) && *PFOLLOWMOUSE != 3)
        g_pCompositor->focusWindow(pWindow);

    if (pWindow->m_isFloating)
        g_pCompositor->changeWindowZOrder(pWindow, true);

    return true;
}

bool CHyprGroupBarDecoration::onScrollOnDeco(const Vector2D& pos, const IPointer::SAxisEvent e) {
    static auto PGROUPBARSCROLLING = CConfigValue<Hyprlang::INT>("group:groupbar:scrolling");

    if (!*PGROUPBARSCROLLING || m_window->m_groupData.pNextWindow.expired())
        return false;

    if (e.delta > 0)
        m_window->setGroupCurrent(m_window->m_groupData.pNextWindow.lock());
    else
        m_window->setGroupCurrent(m_window->getGroupPrevious());

    return true;
}

bool CHyprGroupBarDecoration::onInputOnDeco(const eInputType type, const Vector2D& mouseCoords, std::any data) {
    switch (type) {
        case INPUT_TYPE_AXIS: return onScrollOnDeco(mouseCoords, std::any_cast<const IPointer::SAxisEvent>(data));
        case INPUT_TYPE_BUTTON: return onMouseButtonOnDeco(mouseCoords, std::any_cast<const IPointer::SButtonEvent&>(data));
        case INPUT_TYPE_DRAG_START: return onBeginWindowDragOnDeco(mouseCoords);
        case INPUT_TYPE_DRAG_END: return onEndWindowDragOnDeco(mouseCoords, std::any_cast<PHLWINDOW>(data));
        default: return false;
    }
}

eDecorationLayer CHyprGroupBarDecoration::getDecorationLayer() {
    return DECORATION_LAYER_OVER;
}

uint64_t CHyprGroupBarDecoration::getDecorationFlags() {
    return DECORATION_ALLOWS_MOUSE_INPUT;
}

std::string CHyprGroupBarDecoration::getDisplayName() {
    return "GroupBar";
}

CBox CHyprGroupBarDecoration::assignedBoxGlobal() {
    CBox box = m_assignedBox;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_TOP, m_window));

    const auto PWORKSPACE = m_window->m_workspace;

    if (PWORKSPACE && !m_window->m_pinned)
        box.translate(PWORKSPACE->m_renderOffset->value());

    return box.round();
}

bool CHyprGroupBarDecoration::visible() {
    static auto PENABLED = CConfigValue<Hyprlang::INT>("group:groupbar:enabled");
    return *PENABLED && m_window->m_ruleApplicator->decorate().valueOrDefault();
}
