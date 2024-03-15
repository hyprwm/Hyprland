#include "AnimationManager.hpp"
#include "../Compositor.hpp"
#include "HookSystemManager.hpp"
#include "macros.hpp"
#include "../config/ConfigValue.hpp"

int wlTick(void* data) {
    if (g_pAnimationManager)
        g_pAnimationManager->onTicked();

    if (g_pCompositor->m_bSessionActive && g_pAnimationManager && g_pHookSystem && !g_pCompositor->m_bUnsafeState &&
        std::ranges::any_of(g_pCompositor->m_vMonitors, [](const auto& mon) { return mon->m_bEnabled && mon->output; })) {
        g_pAnimationManager->tick();
        EMIT_HOOK_EVENT("tick", nullptr);
    }

    if (g_pAnimationManager && g_pAnimationManager->shouldTickForNext())
        g_pAnimationManager->scheduleTick();

    return 0;
}

CAnimationManager::CAnimationManager() {
    std::vector<Vector2D> points = {Vector2D(0, 0.75f), Vector2D(0.15f, 1.f)};
    m_mBezierCurves["default"].setup(&points);

    m_pAnimationTick = wl_event_loop_add_timer(g_pCompositor->m_sWLEventLoop, &wlTick, nullptr);
    wl_event_source_timer_update(m_pAnimationTick, 1);
}

void CAnimationManager::removeAllBeziers() {
    m_mBezierCurves.clear();

    // add the default one
    std::vector<Vector2D> points = {Vector2D(0, 0.75f), Vector2D(0.15f, 1.f)};
    m_mBezierCurves["default"].setup(&points);
}

void CAnimationManager::addBezierWithName(std::string name, const Vector2D& p1, const Vector2D& p2) {
    std::vector points = {p1, p2};
    m_mBezierCurves[name].setup(&points);
}

void CAnimationManager::onTicked() {
    m_bTickScheduled = false;
}

void CAnimationManager::tick() {
    static std::chrono::time_point lastTick = std::chrono::high_resolution_clock::now();
    m_fLastTickTime                         = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - lastTick).count() / 1000.0;
    lastTick                                = std::chrono::high_resolution_clock::now();

    if (m_vActiveAnimatedVariables.empty())
        return;

    bool        animGlobalDisabled = false;

    static auto PANIMENABLED = CConfigValue<Hyprlang::INT>("animations:enabled");

    if (!*PANIMENABLED)
        animGlobalDisabled = true;

    static auto* const                  PSHADOWSENABLED = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("decoration:drop_shadow");

    const auto                          DEFAULTBEZIER = m_mBezierCurves.find("default");

    std::vector<CBaseAnimatedVariable*> animationEndedVars;

    for (auto& av : m_vActiveAnimatedVariables) {

        if (av->m_eDamagePolicy == AVARDAMAGE_SHADOW && !*PSHADOWSENABLED) {
            av->warp(false);
            animationEndedVars.push_back(av);
            continue;
        }

        // get the spent % (0 - 1)
        const float SPENT = av->getPercent();

        // window stuff
        const auto PWINDOW            = (CWindow*)av->m_pWindow;
        const auto PWORKSPACE         = (CWorkspace*)av->m_pWorkspace;
        const auto PLAYER             = (SLayerSurface*)av->m_pLayer;
        CMonitor*  PMONITOR           = nullptr;
        bool       animationsDisabled = animGlobalDisabled;

        CBox       WLRBOXPREV = {0, 0, 0, 0};
        if (PWINDOW) {
            CBox       bb               = PWINDOW->getFullWindowBoundingBox();
            const auto PWINDOWWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);
            if (PWINDOWWORKSPACE)
                bb.translate(PWINDOWWORKSPACE->m_vRenderOffset.value());
            WLRBOXPREV = bb;

            PMONITOR = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);
            if (!PMONITOR)
                continue;
            animationsDisabled = animationsDisabled || PWINDOW->m_sAdditionalConfigData.forceNoAnims;
        } else if (PWORKSPACE) {
            PMONITOR = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);
            if (!PMONITOR)
                continue;
            WLRBOXPREV = {PMONITOR->vecPosition, PMONITOR->vecSize};

            // TODO: just make this into a damn callback already vax...
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w->isHidden() && w->m_bIsMapped && w->m_bIsFloating)
                    g_pHyprRenderer->damageWindow(w.get());
            }

            // if a workspace window is on any monitor, damage it
            for (auto& w : g_pCompositor->m_vWindows) {
                for (auto& m : g_pCompositor->m_vMonitors) {
                    if (w->m_iWorkspaceID == PWORKSPACE->m_iID && g_pCompositor->windowValidMapped(w.get()) && g_pHyprRenderer->shouldRenderWindow(w.get(), m.get(), PWORKSPACE)) {
                        CBox bb = w->getFullWindowBoundingBox();
                        bb.translate(PWORKSPACE->m_vRenderOffset.value());
                        if (PWORKSPACE->m_bIsSpecialWorkspace)
                            bb.scaleFromCenter(1.1); // for some reason special ws windows getting border artifacts if you close it too quickly...
                        bb.intersection({m->vecPosition, m->vecSize});
                        g_pHyprRenderer->damageBox(&bb);
                    }
                }
            }
        } else if (PLAYER) {
            WLRBOXPREV = CBox{PLAYER->realPosition.value(), PLAYER->realSize.value()};
            PMONITOR   = g_pCompositor->getMonitorFromVector(Vector2D(PLAYER->geometry.x, PLAYER->geometry.y) + Vector2D(PLAYER->geometry.width, PLAYER->geometry.height) / 2.f);
            if (!PMONITOR)
                continue;
            animationsDisabled = animationsDisabled || PLAYER->noAnimations;
        }

        const bool VISIBLE = PWINDOW ? g_pCompositor->isWorkspaceVisible(PWINDOW->m_iWorkspaceID) : true;

        // beziers are with a switch unforto
        // TODO: maybe do something cleaner

        auto updateVariable = [&]<Animable T>(CAnimatedVariable<T>& av) {
            // for disabled anims just warp
            if (av.m_pConfig->pValues->internalEnabled == 0 || animationsDisabled) {
                av.warp(false);
                return;
            }

            if (SPENT >= 1.f || av.m_Begun == av.m_Goal) {
                av.warp(false);
                return;
            }

            const auto DELTA  = av.m_Goal - av.m_Begun;
            const auto BEZIER = m_mBezierCurves.find(av.m_pConfig->pValues->internalBezier);

            if (BEZIER != m_mBezierCurves.end())
                av.m_Value = av.m_Begun + DELTA * BEZIER->second.getYForPoint(SPENT);
            else
                av.m_Value = av.m_Begun + DELTA * DEFAULTBEZIER->second.getYForPoint(SPENT);
        };

        switch (av->m_Type) {
            case AVARTYPE_FLOAT: {
                auto typedAv = static_cast<CAnimatedVariable<float>*>(av);
                updateVariable(*typedAv);
                break;
            }
            case AVARTYPE_VECTOR: {
                auto typedAv = static_cast<CAnimatedVariable<Vector2D>*>(av);
                updateVariable(*typedAv);
                break;
            }
            case AVARTYPE_COLOR: {
                auto typedAv = static_cast<CAnimatedVariable<CColor>*>(av);
                updateVariable(*typedAv);
                break;
            }
            default: UNREACHABLE();
        }
        // set size and pos if valid, but only if damage policy entire (dont if border for example)
        if (g_pCompositor->windowValidMapped(PWINDOW) && av->m_eDamagePolicy == AVARDAMAGE_ENTIRE && PWINDOW->m_iX11Type != 2)
            g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goal());

        // check if we did not finish animating. If so, trigger onAnimationEnd.
        if (!av->isBeingAnimated())
            animationEndedVars.push_back(av);

        // lastly, handle damage, but only if whatever we are animating is visible.
        if (!VISIBLE)
            continue;

        if (av->m_fUpdateCallback)
            av->m_fUpdateCallback(av);

        switch (av->m_eDamagePolicy) {
            case AVARDAMAGE_ENTIRE: {
                g_pHyprRenderer->damageBox(&WLRBOXPREV);

                if (PWINDOW) {
                    PWINDOW->updateWindowDecos();
                    auto       bb               = PWINDOW->getFullWindowBoundingBox();
                    const auto PWINDOWWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);
                    bb.translate(PWINDOWWORKSPACE->m_vRenderOffset.value());
                    g_pHyprRenderer->damageBox(&bb);
                } else if (PWORKSPACE) {
                    for (auto& w : g_pCompositor->m_vWindows) {
                        if (!w->m_bIsMapped || w->isHidden())
                            continue;

                        if (w->m_iWorkspaceID != PWORKSPACE->m_iID)
                            continue;

                        w->updateWindowDecos();

                        if (w->m_bIsFloating) {
                            auto bb = w->getFullWindowBoundingBox();
                            bb.translate(PWORKSPACE->m_vRenderOffset.value());
                            g_pHyprRenderer->damageBox(&bb);
                        }
                    }
                } else if (PLAYER) {
                    if (PLAYER->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || PLAYER->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
                        g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR);

                    // some fucking layers miss 1 pixel???
                    CBox expandBox = WLRBOXPREV;
                    expandBox.expand(5);
                    g_pHyprRenderer->damageBox(&expandBox);
                }
                break;
            }
            case AVARDAMAGE_BORDER: {
                RASSERT(PWINDOW, "Tried to AVARDAMAGE_BORDER a non-window AVAR!");

                // TODO: move this to the border class

                // damage only the border.
                const auto ROUNDING     = PWINDOW->rounding();
                const auto ROUNDINGSIZE = ROUNDING + 1;
                const auto BORDERSIZE   = PWINDOW->getRealBorderSize();

                // damage for old box
                g_pHyprRenderer->damageBox(WLRBOXPREV.x - BORDERSIZE, WLRBOXPREV.y - BORDERSIZE, WLRBOXPREV.width + 2 * BORDERSIZE, BORDERSIZE + ROUNDINGSIZE);  // top
                g_pHyprRenderer->damageBox(WLRBOXPREV.x - BORDERSIZE, WLRBOXPREV.y - BORDERSIZE, BORDERSIZE + ROUNDINGSIZE, WLRBOXPREV.height + 2 * BORDERSIZE); // left
                g_pHyprRenderer->damageBox(WLRBOXPREV.x + WLRBOXPREV.width - ROUNDINGSIZE, WLRBOXPREV.y - BORDERSIZE, BORDERSIZE + ROUNDINGSIZE,
                                           WLRBOXPREV.height + 2 * BORDERSIZE); // right
                g_pHyprRenderer->damageBox(WLRBOXPREV.x, WLRBOXPREV.y + WLRBOXPREV.height - ROUNDINGSIZE, WLRBOXPREV.width + 2 * BORDERSIZE,
                                           BORDERSIZE + ROUNDINGSIZE); // bottom

                // damage for new box
                CBox       WLRBOXNEW        = {PWINDOW->m_vRealPosition.value(), PWINDOW->m_vRealSize.value()};
                const auto PWINDOWWORKSPACE = g_pCompositor->getWorkspaceByID(PWINDOW->m_iWorkspaceID);
                if (PWINDOWWORKSPACE)
                    WLRBOXNEW.translate(PWINDOWWORKSPACE->m_vRenderOffset.value());
                g_pHyprRenderer->damageBox(WLRBOXNEW.x - BORDERSIZE, WLRBOXNEW.y - BORDERSIZE, WLRBOXNEW.width + 2 * BORDERSIZE, BORDERSIZE + ROUNDINGSIZE);  // top
                g_pHyprRenderer->damageBox(WLRBOXNEW.x - BORDERSIZE, WLRBOXNEW.y - BORDERSIZE, BORDERSIZE + ROUNDINGSIZE, WLRBOXNEW.height + 2 * BORDERSIZE); // left
                g_pHyprRenderer->damageBox(WLRBOXNEW.x + WLRBOXNEW.width - ROUNDINGSIZE, WLRBOXNEW.y - BORDERSIZE, BORDERSIZE + ROUNDINGSIZE,
                                           WLRBOXNEW.height + 2 * BORDERSIZE);                                                                                       // right
                g_pHyprRenderer->damageBox(WLRBOXNEW.x, WLRBOXNEW.y + WLRBOXNEW.height - ROUNDINGSIZE, WLRBOXNEW.width + 2 * BORDERSIZE, BORDERSIZE + ROUNDINGSIZE); // bottom

                break;
            }
            case AVARDAMAGE_SHADOW: {
                RASSERT(PWINDOW, "Tried to AVARDAMAGE_SHADOW a non-window AVAR!");

                const auto PDECO = PWINDOW->getDecorationByType(DECORATION_SHADOW);

                PDECO->damageEntire();

                break;
            }
            default: {
                break;
            }
        }

        // manually schedule a frame
        if (PMONITOR)
            g_pCompositor->scheduleFrameForMonitor(PMONITOR);
    }

    // do it here, because if this alters the animation vars deque we would be in trouble above.
    for (auto& ave : animationEndedVars) {
        ave->onAnimationEnd();
    }
}

bool CAnimationManager::deltaSmallToFlip(const Vector2D& a, const Vector2D& b) {
    return std::abs(a.x - b.x) < 0.5f && std::abs(a.y - b.y) < 0.5f;
}

bool CAnimationManager::deltaSmallToFlip(const CColor& a, const CColor& b) {
    return std::abs(a.r - b.r) < 0.5f && std::abs(a.g - b.g) < 0.5f && std::abs(a.b - b.b) < 0.5f && std::abs(a.a - b.a) < 0.5f;
}

bool CAnimationManager::deltaSmallToFlip(const float& a, const float& b) {
    return std::abs(a - b) < 0.5f;
}

bool CAnimationManager::deltazero(const Vector2D& a, const Vector2D& b) {
    return a.x == b.x && a.y == b.y;
}

bool CAnimationManager::deltazero(const float& a, const float& b) {
    return a == b;
}

bool CAnimationManager::deltazero(const CColor& a, const CColor& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool CAnimationManager::bezierExists(const std::string& bezier) {
    for (auto& [bc, bz] : m_mBezierCurves) {
        if (bc == bezier)
            return true;
    }

    return false;
}

//
// Anims
//
//

void CAnimationManager::animationPopin(CWindow* pWindow, bool close, float minPerc) {
    const auto GOALPOS  = pWindow->m_vRealPosition.goal();
    const auto GOALSIZE = pWindow->m_vRealSize.goal();

    if (!close) {
        pWindow->m_vRealSize.setValue((GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y}));
        pWindow->m_vRealPosition.setValue(GOALPOS + GOALSIZE / 2.f - pWindow->m_vRealSize.m_Value / 2.f);
    } else {
        pWindow->m_vRealSize     = (GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y});
        pWindow->m_vRealPosition = GOALPOS + GOALSIZE / 2.f - pWindow->m_vRealSize.m_Goal / 2.f;
    }
}

void CAnimationManager::animationSlide(CWindow* pWindow, std::string force, bool close) {
    pWindow->m_vRealSize.warp(false); // size we preserve in slide

    const auto GOALPOS  = pWindow->m_vRealPosition.goal();
    const auto GOALSIZE = pWindow->m_vRealSize.goal();

    const auto PMONITOR = g_pCompositor->getMonitorFromID(pWindow->m_iMonitorID);

    if (!PMONITOR)
        return; // unsafe state most likely

    Vector2D posOffset;

    if (force != "") {
        if (force == "bottom")
            posOffset = Vector2D(GOALPOS.x, PMONITOR->vecPosition.y + PMONITOR->vecSize.y);
        else if (force == "left")
            posOffset = GOALPOS - Vector2D(GOALSIZE.x, 0);
        else if (force == "right")
            posOffset = GOALPOS + Vector2D(GOALSIZE.x, 0);
        else
            posOffset = Vector2D(GOALPOS.x, PMONITOR->vecPosition.y - GOALSIZE.y);

        if (!close)
            pWindow->m_vRealPosition.setValue(posOffset);
        else
            pWindow->m_vRealPosition = posOffset;

        return;
    }

    const auto MIDPOINT = GOALPOS + GOALSIZE / 2.f;

    // check sides it touches
    const bool DISPLAYLEFT   = STICKS(pWindow->m_vPosition.x, PMONITOR->vecPosition.x + PMONITOR->vecReservedTopLeft.x);
    const bool DISPLAYRIGHT  = STICKS(pWindow->m_vPosition.x + pWindow->m_vSize.x, PMONITOR->vecPosition.x + PMONITOR->vecSize.x - PMONITOR->vecReservedBottomRight.x);
    const bool DISPLAYTOP    = STICKS(pWindow->m_vPosition.y, PMONITOR->vecPosition.y + PMONITOR->vecReservedTopLeft.y);
    const bool DISPLAYBOTTOM = STICKS(pWindow->m_vPosition.y + pWindow->m_vSize.y, PMONITOR->vecPosition.y + PMONITOR->vecSize.y - PMONITOR->vecReservedBottomRight.y);

    if (DISPLAYBOTTOM && DISPLAYTOP) {
        if (DISPLAYLEFT && DISPLAYRIGHT) {
            posOffset = GOALPOS + Vector2D(0, GOALSIZE.y);
        } else if (DISPLAYLEFT) {
            posOffset = GOALPOS - Vector2D(GOALSIZE.x, 0);
        } else {
            posOffset = GOALPOS + Vector2D(GOALSIZE.x, 0);
        }
    } else if (DISPLAYTOP) {
        posOffset = GOALPOS - Vector2D(0, GOALSIZE.y);
    } else if (DISPLAYBOTTOM) {
        posOffset = GOALPOS + Vector2D(0, GOALSIZE.y);
    } else {
        if (MIDPOINT.y > PMONITOR->vecPosition.y + PMONITOR->vecSize.y / 2.f)
            posOffset = Vector2D(GOALPOS.x, PMONITOR->vecPosition.y + PMONITOR->vecSize.y);
        else
            posOffset = Vector2D(GOALPOS.x, PMONITOR->vecPosition.y - GOALSIZE.y);
    }

    if (!close)
        pWindow->m_vRealPosition.setValue(posOffset);
    else
        pWindow->m_vRealPosition = posOffset;
}

void CAnimationManager::onWindowPostCreateClose(CWindow* pWindow, bool close) {
    if (!close) {
        pWindow->m_vRealPosition.m_pConfig = g_pConfigManager->getAnimationPropertyConfig("windowsIn");
        pWindow->m_vRealSize.m_pConfig     = g_pConfigManager->getAnimationPropertyConfig("windowsIn");
        pWindow->m_fAlpha.m_pConfig        = g_pConfigManager->getAnimationPropertyConfig("fadeIn");
    } else {
        pWindow->m_vRealPosition.m_pConfig = g_pConfigManager->getAnimationPropertyConfig("windowsOut");
        pWindow->m_vRealSize.m_pConfig     = g_pConfigManager->getAnimationPropertyConfig("windowsOut");
        pWindow->m_fAlpha.m_pConfig        = g_pConfigManager->getAnimationPropertyConfig("fadeOut");
    }

    auto ANIMSTYLE = pWindow->m_vRealPosition.m_pConfig->pValues->internalStyle;
    transform(ANIMSTYLE.begin(), ANIMSTYLE.end(), ANIMSTYLE.begin(), ::tolower);

    // if the window is not being animated, that means the layout set a fixed size for it, don't animate.
    if (!pWindow->m_vRealPosition.isBeingAnimated() && !pWindow->m_vRealSize.isBeingAnimated())
        return;

    // if the animation is disabled and we are leaving, ignore the anim to prevent the snapshot being fucked
    if (!pWindow->m_vRealPosition.m_pConfig->pValues->internalEnabled)
        return;

    if (pWindow->m_sAdditionalConfigData.animationStyle != "") {
        // the window has config'd special anim
        if (pWindow->m_sAdditionalConfigData.animationStyle.starts_with("slide")) {
            if (pWindow->m_sAdditionalConfigData.animationStyle.contains(' ')) {
                // has a direction
                animationSlide(pWindow, pWindow->m_sAdditionalConfigData.animationStyle.substr(pWindow->m_sAdditionalConfigData.animationStyle.find(' ') + 1), close);
            } else {
                animationSlide(pWindow, "", close);
            }
        } else {
            // anim popin, fallback

            float minPerc = 0.f;
            if (pWindow->m_sAdditionalConfigData.animationStyle.find("%") != std::string::npos) {
                try {
                    auto percstr = pWindow->m_sAdditionalConfigData.animationStyle.substr(pWindow->m_sAdditionalConfigData.animationStyle.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) {
                    ; // oops
                }
            }

            animationPopin(pWindow, close, minPerc / 100.f);
        }
    } else {
        if (ANIMSTYLE == "slide") {
            animationSlide(pWindow, "", close);
        } else {
            // anim popin, fallback

            float minPerc = 0.f;
            if (!ANIMSTYLE.starts_with("%")) {
                try {
                    auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) {
                    ; // oops
                }
            }

            animationPopin(pWindow, close, minPerc / 100.f);
        }
    }
}

std::string CAnimationManager::styleValidInConfigVar(const std::string& config, const std::string& style) {
    if (config.starts_with("window")) {
        if (style == "slide") {
            return "";
        } else if (style.starts_with("popin")) {
            // try parsing
            float minPerc = 0.f;
            if (style.find("%") != std::string::npos) {
                try {
                    auto percstr = style.substr(style.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) { return "invalid minperc"; }

                return "";
            }

            minPerc; // fix warning

            return "";
        }

        return "unknown style";
    } else if (config == "workspaces" || config == "specialWorkspace") {
        if (style == "slide" || style == "slidevert" || style == "fade")
            return "";
        else if (style.starts_with("slidefade")) {
            // try parsing
            float movePerc = 0.f;
            if (style.find("%") != std::string::npos) {
                try {
                    auto percstr = style.substr(style.find_last_of(' ') + 1);
                    movePerc     = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) { return "invalid movePerc"; }

                return "";
            }

            movePerc; // fix warning

            return "";
        }

        return "unknown style";
    } else if (config == "borderangle") {
        if (style == "loop" || style == "once")
            return "";
        return "unknown style";
    } else if (config.starts_with("layers")) {
        if (style == "fade" || style == "" || style == "slide")
            return "";
        else if (style.starts_with("popin")) {
            // try parsing
            float minPerc = 0.f;
            if (style.find("%") != std::string::npos) {
                try {
                    auto percstr = style.substr(style.find_last_of(' '));
                    minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
                } catch (std::exception& e) { return "invalid minperc"; }

                return "";
            }

            minPerc; // fix warning

            return "";
        }
        return "";
        return "unknown style";
    } else {
        return "animation has no styles";
    }

    return "";
}

CBezierCurve* CAnimationManager::getBezier(const std::string& name) {
    const auto BEZIER = std::find_if(m_mBezierCurves.begin(), m_mBezierCurves.end(), [&](const auto& other) { return other.first == name; });

    return BEZIER == m_mBezierCurves.end() ? &m_mBezierCurves["default"] : &BEZIER->second;
}

std::unordered_map<std::string, CBezierCurve> CAnimationManager::getAllBeziers() {
    return m_mBezierCurves;
}

bool CAnimationManager::shouldTickForNext() {
    return !m_vActiveAnimatedVariables.empty();
}

void CAnimationManager::scheduleTick() {
    if (m_bTickScheduled)
        return;

    m_bTickScheduled = true;

    const auto PMOSTHZ = g_pHyprRenderer->m_pMostHzMonitor;

    if (!PMOSTHZ) {
        wl_event_source_timer_update(m_pAnimationTick, 16);
        return;
    }

    float       refreshDelayMs = std::floor(1000.f / PMOSTHZ->refreshRate);

    const float SINCEPRES = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - PMOSTHZ->lastPresentationTimer.chrono()).count() / 1000.f;

    const auto  TOPRES = std::clamp(refreshDelayMs - SINCEPRES, 1.1f, 1000.f); // we can't send 0, that will disarm it

    wl_event_source_timer_update(m_pAnimationTick, std::floor(TOPRES));
}
