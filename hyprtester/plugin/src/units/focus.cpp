#include "../private.hpp"
#include "../globals.hpp"

static SDispatchResult nullfocus(lua_State* L) {
    g_pSeatManager->setKeyboardFocus(nullptr);
    return {};
}

static SDispatchResult clearSurfaceFocus(lua_State* L) {
    Desktop::focusState()->m_focusSurface.reset();
    return {};
}

static SDispatchResult checkKeyboardFocusWindow(lua_State* L) {
    const auto CLS = std::string{luaL_checkstring(L, 1)};

    const auto KBSURF = g_pSeatManager->m_state.keyboardFocus.lock();
    if (!KBSURF)
        return {.success = false, .error = "No keyboard focus"};

    const auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW)
        return {.success = false, .error = "Keyboard focus surface is not a window"};

    if (PWINDOW->metadata().appID() != CLS)
        return {.success = false, .error = std::format("Keyboard focus window class is '{}', expected '{}'", PWINDOW->metadata().appID(), CLS)};

    return {};
}

static SDispatchResult checkPointerFocusLayer(lua_State* L) {
    const auto NS = std::string{luaL_checkstring(L, 1)};

    const auto POINTERSURF = g_pSeatManager->m_state.pointerFocus.lock();

    if (!POINTERSURF)
        return {.success = false, .error = "No pointer focus"};

    const auto HLSURF = Desktop::View::CWLSurface::fromResource(POINTERSURF);
    const auto VIEW   = HLSURF ? HLSURF->view() : nullptr;
    const auto LAYER  = Desktop::View::CLayerSurface::fromView(VIEW);

    if (!LAYER) {
        const auto WINDOW = Desktop::viewState()->query().type(Desktop::View::VIEW_TYPE_WINDOW).surface(POINTERSURF).runWindow();
        if (WINDOW)
            return {.success = false, .error = std::format("Pointer focus is a window surface with class '{}'", WINDOW->metadata().appID())};

        return {.success = false, .error = std::format("Pointer focus is not a layer surface, view type is {}", VIEW ? sc<int>(VIEW->type()) : -1)};
    }

    if (LAYER->m_namespace != NS)
        return {.success = false, .error = std::format("Pointer focus layer namespace is '{}', expected '{}'", LAYER->m_namespace, NS)};

    return {};
}

static SDispatchResult setPointerFocusLayer(lua_State* L) {
    const auto NS = std::string{luaL_checkstring(L, 1)};

    for (const auto& layer : Desktop::layerState()->layers()) {
        if (layer->m_namespace != NS)
            continue;

        const auto SURFACE = layer->wlSurface() ? layer->wlSurface()->resource() : nullptr;
        if (!SURFACE)
            return {.success = false, .error = std::format("Layer '{}' has no surface", NS)};

        const auto LOCAL = layer->m_geometry.size() / 2.0;

        g_pSeatManager->setPointerFocus(SURFACE, LOCAL);
        g_pSeatManager->sendPointerMotion(Time::millis(Time::steadyNow()), LOCAL);
        return {};
    }

    return {.success = false, .error = std::format("No layer with namespace '{}'", NS)};
}

REGISTER_UNIT(focus) {
    registerLuaFn<nullfocus>("nullfocus");
    registerLuaFn<clearSurfaceFocus>("clear_surface_focus");
    registerLuaFn<checkKeyboardFocusWindow>("check_keyboard_focus_window");
    registerLuaFn<checkPointerFocusLayer>("check_pointer_focus_layer");
    registerLuaFn<setPointerFocusLayer>("set_pointer_focus_layer");
}