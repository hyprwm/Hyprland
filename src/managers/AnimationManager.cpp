#include "AnimationManager.hpp"
#include "../Compositor.hpp"
#include "HookSystemManager.hpp"

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

    bool               animGlobalDisabled = false;

    static auto* const PANIMENABLED = &g_pConfigManager->getConfigValuePtr("animations:enabled")->intValue;

    if (!*PANIMENABLED)
        animGlobalDisabled = true;

    static auto* const              PSHADOWSENABLED = &g_pConfigManager->getConfigValuePtr("decoration:drop_shadow")->intValue;

    const auto                      DEFAULTBEZIER = m_mBezierCurves.find("default");

    std::vector<CAnimatedVariable*> animationEndedVars;

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
            WLRBOXPREV = PWINDOW->getFullWindowBoundingBox();
            PMONITOR   = g_pCompositor->getMonitorFromID(PWINDOW->m_iMonitorID);
            if (!PMONITOR)
                continue;
            animationsDisabled = animationsDisabled || PWINDOW->m_sAdditionalConfigData.forceNoAnims;
        } else if (PWORKSPACE) {
            PMONITOR = g_pCompositor->getMonitorFromID(PWORKSPACE->m_iMonitorID);
            if (!PMONITOR)
                continue;
            WLRBOXPREV = {(int)PMONITOR->vecPosition.x, (int)PMONITOR->vecPosition.y, (int)PMONITOR->vecSize.x, (int)PMONITOR->vecSize.y};

            // TODO: just make this into a damn callback already vax...
            for (auto& w : g_pCompositor->m_vWindows) {
                if (!w->isHidden() && w->m_bIsMapped && w->m_bIsFloating)
                    g_pHyprRenderer->damageWindow(w.get());
            }
        } else if (PLAYER) {
            WLRBOXPREV = PLAYER->geometry;
            PMONITOR   = g_pCompositor->getMonitorFromVector(Vector2D(PLAYER->geometry.x, PLAYER->geometry.y) + Vector2D(PLAYER->geometry.width, PLAYER->geometry.height) / 2.f);
            if (!PMONITOR)
                continue;
            animationsDisabled = animationsDisabled || PLAYER->noAnimations;
        }

        const bool VISIBLE = PWINDOW ? g_pCompositor->isWorkspaceVisible(PWINDOW->m_iWorkspaceID) : true;

        // beziers are with a switch unforto
        // TODO: maybe do something cleaner

        switch (av->m_eVarType) {
            case AVARTYPE_FLOAT: {
                // for disabled anims just warp
                if (av->m_pConfig->pValues->internalEnabled == 0 || animationsDisabled) {
                    av->warp(false);
                    break;
                }

                if (SPENT >= 1.f || av->m_fBegun == av->m_fGoal) {
                    av->warp(false);
                    break;
                }

                const auto DELTA  = av->m_fGoal - av->m_fBegun;
                const auto BEZIER = m_mBezierCurves.find(av->m_pConfig->pValues->internalBezier);

                if (BEZIER != m_mBezierCurves.end())
                    av->m_fValue = av->m_fBegun + BEZIER->second.getYForPoint(SPENT) * DELTA;
                else
                    av->m_fValue = av->m_fBegun + DEFAULTBEZIER->second.getYForPoint(SPENT) * DELTA;
                break;
            }
            case AVARTYPE_VECTOR: {
                // for disabled anims just warp
                if (av->m_pConfig->pValues->internalEnabled == 0 || animationsDisabled) {
                    av->warp(false);
                    break;
                }

                if (SPENT >= 1.f || av->m_vBegun == av->m_vGoal) {
                    av->warp(false);
                    break;
                }

                const auto DELTA  = av->m_vGoal - av->m_vBegun;
                const auto BEZIER = m_mBezierCurves.find(av->m_pConfig->pValues->internalBezier);

                if (BEZIER != m_mBezierCurves.end())
                    av->m_vValue = av->m_vBegun + DELTA * BEZIER->second.getYForPoint(SPENT);
                else
                    av->m_vValue = av->m_vBegun + DELTA * DEFAULTBEZIER->second.getYForPoint(SPENT);
                break;
            }
            case AVARTYPE_COLOR: {
                // for disabled anims just warp
                if (av->m_pConfig->pValues->internalEnabled == 0 || animationsDisabled) {
                    av->warp(false);
                    break;
                }

                if (SPENT >= 1.f || av->m_cBegun == av->m_cGoal) {
                    av->warp(false);
                    break;
                }

                const auto DELTA  = av->m_cGoal - av->m_cBegun;
                const auto BEZIER = m_mBezierCurves.find(av->m_pConfig->pValues->internalBezier);

                if (BEZIER != m_mBezierCurves.end())
                    av->m_cValue = av->m_cBegun + DELTA * BEZIER->second.getYForPoint(SPENT);
                else
                    av->m_cValue = av->m_cBegun + DELTA * DEFAULTBEZIER->second.getYForPoint(SPENT);
                break;
            }
            default: {
                ;
            }
        }

        // set size and pos if valid, but only if damage policy entire (dont if border for example)
        if (g_pCompositor->windowValidMapped(PWINDOW) && av->m_eDamagePolicy == AVARDAMAGE_ENTIRE && PWINDOW->m_iX11Type != 2)
            g_pXWaylandManager->setWindowSize(PWINDOW, PWINDOW->m_vRealSize.goalv());

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
                    g_pHyprRenderer->damageWindow(PWINDOW);
                } else if (PWORKSPACE) {
                    for (auto& w : g_pCompositor->m_vWindows) {
                        if (!w->m_bIsMapped || w->isHidden())
                            continue;

                        if (w->m_iWorkspaceID != PWORKSPACE->m_iID)
                            continue;

                        w->updateWindowDecos();

                        if (w->m_bIsFloating) {
                            auto bb = w->getFullWindowBoundingBox();
                            bb.translate(PWORKSPACE->m_vRenderOffset.vec());
                            g_pHyprRenderer->damageBox(&bb);
                        }
                    }
                } else if (PLAYER) {
                    if (PLAYER->layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || PLAYER->layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM)
                        g_pHyprOpenGL->markBlurDirtyForMonitor(PMONITOR);
                }
                break;
            }
            case AVARDAMAGE_BORDER: {
                RASSERT(PWINDOW, "Tried to AVARDAMAGE_BORDER a non-window AVAR!");

                // damage only the border.
                static auto* const PROUNDING    = &g_pConfigManager->getConfigValuePtr("decoration:rounding")->intValue;
                const auto         ROUNDINGSIZE = *PROUNDING + 1;
                const auto         BORDERSIZE   = PWINDOW->getRealBorderSize();

                // damage for old box
                g_pHyprRenderer->damageBox(WLRBOXPREV.x - BORDERSIZE, WLRBOXPREV.y - BORDERSIZE, WLRBOXPREV.width + 2 * BORDERSIZE, BORDERSIZE + ROUNDINGSIZE);  // top
                g_pHyprRenderer->damageBox(WLRBOXPREV.x - BORDERSIZE, WLRBOXPREV.y - BORDERSIZE, BORDERSIZE + ROUNDINGSIZE, WLRBOXPREV.height + 2 * BORDERSIZE); // left
                g_pHyprRenderer->damageBox(WLRBOXPREV.x + WLRBOXPREV.width - ROUNDINGSIZE, WLRBOXPREV.y - BORDERSIZE, BORDERSIZE + ROUNDINGSIZE,
                                           WLRBOXPREV.height + 2 * BORDERSIZE); // right
                g_pHyprRenderer->damageBox(WLRBOXPREV.x, WLRBOXPREV.y + WLRBOXPREV.height - ROUNDINGSIZE, WLRBOXPREV.width + 2 * BORDERSIZE,
                                           BORDERSIZE + ROUNDINGSIZE); // bottom

                // damage for new box
                const CBox WLRBOXNEW = {PWINDOW->m_vRealPosition.vec().x, PWINDOW->m_vRealPosition.vec().y, PWINDOW->m_vRealSize.vec().x, PWINDOW->m_vRealSize.vec().y};
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
    const auto GOALPOS  = pWindow->m_vRealPosition.goalv();
    const auto GOALSIZE = pWindow->m_vRealSize.goalv();

    if (!close) {
        pWindow->m_vRealSize.setValue((GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y}));
        pWindow->m_vRealPosition.setValue(GOALPOS + GOALSIZE / 2.f - pWindow->m_vRealSize.m_vValue / 2.f);
    } else {
        pWindow->m_vRealSize     = (GOALSIZE * minPerc).clamp({5, 5}, {GOALSIZE.x, GOALSIZE.y});
        pWindow->m_vRealPosition = GOALPOS + GOALSIZE / 2.f - pWindow->m_vRealSize.m_vGoal / 2.f;
    }
}

void CAnimationManager::animationSlide(CWindow* pWindow, std::string force, bool close) {
    pWindow->m_vRealSize.warp(false); // size we preserve in slide

    const auto GOALPOS  = pWindow->m_vRealPosition.goalv();
    const auto GOALSIZE = pWindow->m_vRealSize.goalv();

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
