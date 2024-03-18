#include "WLClasses.hpp"
#include "../config/ConfigManager.hpp"
#include "../Compositor.hpp"

SLayerSurface::SLayerSurface() {
    alpha.create(g_pConfigManager->getAnimationPropertyConfig("fadeLayers"), nullptr, AVARDAMAGE_ENTIRE);
    realPosition.create(g_pConfigManager->getAnimationPropertyConfig("layers"), nullptr, AVARDAMAGE_ENTIRE);
    realSize.create(g_pConfigManager->getAnimationPropertyConfig("layers"), nullptr, AVARDAMAGE_ENTIRE);
    alpha.m_pLayer        = this;
    realPosition.m_pLayer = this;
    realSize.m_pLayer     = this;
    alpha.registerVar();
    realPosition.registerVar();
    realSize.registerVar();

    alpha.setValueAndWarp(0.f);
}

SLayerSurface::~SLayerSurface() {
    if (!g_pHyprOpenGL)
        return;

    g_pHyprRenderer->makeEGLCurrent();
    std::erase_if(g_pHyprOpenGL->m_mLayerFramebuffers, [&](const auto& other) { return other.first == this; });
}

void SLayerSurface::applyRules() {
    noAnimations     = false;
    forceBlur        = false;
    ignoreAlpha      = false;
    ignoreAlphaValue = 0.f;
    xray             = -1;
    animationStyle.reset();

    for (auto& rule : g_pConfigManager->getMatchingRules(this)) {
        if (rule.rule == "noanim")
            noAnimations = true;
        else if (rule.rule == "blur")
            forceBlur = true;
        else if (rule.rule.starts_with("ignorealpha") || rule.rule.starts_with("ignorezero")) {
            const auto  FIRST_SPACE_POS = rule.rule.find_first_of(' ');
            std::string alphaValue      = "";
            if (FIRST_SPACE_POS != std::string::npos)
                alphaValue = rule.rule.substr(FIRST_SPACE_POS + 1);

            try {
                ignoreAlpha = true;
                if (!alphaValue.empty())
                    ignoreAlphaValue = std::stof(alphaValue);
            } catch (...) { Debug::log(ERR, "Invalid value passed to ignoreAlpha"); }
        } else if (rule.rule.starts_with("xray")) {
            CVarList vars{rule.rule, 0, ' '};
            try {
                xray = configStringToInt(vars[1]);
            } catch (...) {}
        } else if (rule.rule.starts_with("animation")) {
            CVarList vars{rule.rule, 2, 's'};
            animationStyle = vars[1];
        }
    }
}

void SLayerSurface::startAnimation(bool in, bool instant) {
    const auto ANIMSTYLE = animationStyle.value_or(realPosition.m_pConfig->pValues->internalStyle);

    if (ANIMSTYLE == "slide") {
        // get closest edge
        const auto                    MIDDLE = geometry.middle();

        const auto                    PMONITOR = g_pCompositor->getMonitorFromVector(MIDDLE);

        const std::array<Vector2D, 4> edgePoints = {
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x / 2, 0},
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x / 2, PMONITOR->vecSize.y},
            PMONITOR->vecPosition + Vector2D{0, PMONITOR->vecSize.y},
            PMONITOR->vecPosition + Vector2D{PMONITOR->vecSize.x, PMONITOR->vecSize.y / 2},
        };

        float  closest = std::numeric_limits<float>::max();
        size_t leader  = 0;
        for (size_t i = 0; i < 4; ++i) {
            float dist = MIDDLE.distance(edgePoints[i]);
            if (dist < closest) {
                leader  = i;
                closest = dist;
            }
        }

        realSize.setValueAndWarp(geometry.size());
        alpha.setValueAndWarp(in ? 0.f : 1.f);
        alpha = in ? 1.f : 0.f;

        Vector2D prePos;

        switch (leader) {
            case 0:
                // TOP
                prePos = {geometry.x, PMONITOR->vecPosition.y - geometry.h};
                break;
            case 1:
                // BOTTOM
                prePos = {geometry.x, PMONITOR->vecPosition.y + PMONITOR->vecPosition.y};
                break;
            case 2:
                // LEFT
                prePos = {PMONITOR->vecPosition.x - geometry.w, geometry.y};
                break;
            case 3:
                // RIGHT
                prePos = {PMONITOR->vecPosition.x + PMONITOR->vecSize.x, geometry.y};
                break;
            default: UNREACHABLE();
        }

        if (in) {
            realPosition.setValueAndWarp(prePos);
            realPosition = geometry.pos();
        } else {
            realPosition.setValueAndWarp(geometry.pos());
            realPosition = prePos;
        }

    } else if (ANIMSTYLE.starts_with("popin")) {
        float minPerc = 0.f;
        if (ANIMSTYLE.find("%") != std::string::npos) {
            try {
                auto percstr = ANIMSTYLE.substr(ANIMSTYLE.find_last_of(' '));
                minPerc      = std::stoi(percstr.substr(0, percstr.length() - 1));
            } catch (std::exception& e) {
                ; // oops
            }
        }

        minPerc *= 0.01;

        const auto GOALSIZE = (geometry.size() * minPerc).clamp({5, 5});
        const auto GOALPOS  = geometry.pos() + (geometry.size() - GOALSIZE) / 2.f;

        alpha.setValueAndWarp(in ? 0.f : 1.f);
        alpha = in ? 1.f : 0.f;

        if (in) {
            realSize.setValueAndWarp(GOALSIZE);
            realPosition.setValueAndWarp(GOALPOS);
            realSize     = geometry.size();
            realPosition = geometry.pos();
        } else {
            realSize.setValueAndWarp(geometry.size());
            realPosition.setValueAndWarp(geometry.pos());
            realSize     = GOALSIZE;
            realPosition = GOALPOS;
        }
    } else {
        // fade
        realPosition.setValueAndWarp(geometry.pos());
        realSize.setValueAndWarp(geometry.size());
        alpha = in ? 1.f : 0.f;
    }

    if (!in)
        fadingOut = true;
}

bool SLayerSurface::isFadedOut() {
    if (!fadingOut)
        return false;

    return !realPosition.isBeingAnimated() && !realSize.isBeingAnimated() && !alpha.isBeingAnimated();
}

void SKeyboard::updateXKBTranslationState(xkb_keymap* const keymap) {
    xkb_state_unref(xkbTranslationState);

    if (keymap) {
        Debug::log(LOG, "Updating keyboard {:x}'s translation state from a provided keymap", (uintptr_t)this);
        xkbTranslationState = xkb_state_new(keymap);
        return;
    }

    const auto WLRKB      = wlr_keyboard_from_input_device(keyboard);
    const auto KEYMAP     = WLRKB->keymap;
    const auto STATE      = WLRKB->xkb_state;
    const auto LAYOUTSNUM = xkb_keymap_num_layouts(KEYMAP);

    const auto PCONTEXT = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

    for (uint32_t i = 0; i < LAYOUTSNUM; ++i) {
        if (xkb_state_layout_index_is_active(STATE, i, XKB_STATE_LAYOUT_EFFECTIVE)) {
            Debug::log(LOG, "Updating keyboard {:x}'s translation state from an active index {}", (uintptr_t)this, i);

            CVarList       keyboardLayouts(currentRules.layout, 0, ',');
            CVarList       keyboardModels(currentRules.model, 0, ',');
            CVarList       keyboardVariants(currentRules.variant, 0, ',');

            xkb_rule_names rules = {.rules = "", .model = "", .layout = "", .variant = "", .options = ""};

            std::string    layout, model, variant;
            layout  = keyboardLayouts[i % keyboardLayouts.size()];
            model   = keyboardModels[i % keyboardModels.size()];
            variant = keyboardVariants[i % keyboardVariants.size()];

            rules.layout  = layout.c_str();
            rules.model   = model.c_str();
            rules.variant = variant.c_str();

            auto KEYMAP = xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

            if (!KEYMAP) {
                Debug::log(ERR, "updateXKBTranslationState: keymap failed 1, fallback without model/variant");
                rules.model   = "";
                rules.variant = "";
                KEYMAP        = xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
            }

            if (!KEYMAP) {
                Debug::log(ERR, "updateXKBTranslationState: keymap failed 2, fallback to us");
                rules.layout = "us";
                KEYMAP       = xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
            }

            xkbTranslationState = xkb_state_new(KEYMAP);

            xkb_keymap_unref(KEYMAP);
            xkb_context_unref(PCONTEXT);

            return;
        }
    }

    Debug::log(LOG, "Updating keyboard {:x}'s translation state from an unknown index", (uintptr_t)this);

    xkb_rule_names rules = {
        .rules   = currentRules.rules.c_str(),
        .model   = currentRules.model.c_str(),
        .layout  = currentRules.layout.c_str(),
        .variant = currentRules.variant.c_str(),
        .options = currentRules.options.c_str(),
    };

    const auto NEWKEYMAP = xkb_keymap_new_from_names(PCONTEXT, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

    xkbTranslationState = xkb_state_new(NEWKEYMAP);

    xkb_keymap_unref(NEWKEYMAP);
    xkb_context_unref(PCONTEXT);
}
