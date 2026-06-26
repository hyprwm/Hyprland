#include "FadingOutState.hpp"
#include "LayerState.hpp"
#include "WindowState.hpp"
#include "../view/LayerSurface.hpp"
#include "../view/Window.hpp"
#include "../../debug/log/Logger.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../state/MonitorState.hpp"

#include <algorithm>

using namespace Desktop;

const std::vector<PHLWINDOWREF>& CFadingOutState::windows() const {
    return m_windows;
}

const std::vector<PHLLSREF>& CFadingOutState::layers() const {
    return m_layers;
}

void CFadingOutState::add(PHLWINDOW w) {
    const auto FOUND = std::ranges::find_if(m_windows, [&](PHLWINDOWREF& other) { return other.lock() == w; });

    if (FOUND != m_windows.end())
        return;

    m_windows.emplace_back(w);
}

void CFadingOutState::add(PHLLS ls) {
    const auto FOUND = std::ranges::find_if(m_layers, [&](auto& other) { return other.lock() == ls; });

    if (FOUND != m_layers.end())
        return;

    m_layers.emplace_back(ls);
}

void CFadingOutState::remove(PHLWINDOW w) {
    std::erase_if(m_windows, [&w](const auto& el) { return el.lock() == w; });
}

void CFadingOutState::remove(PHLLS ls) {
    std::erase_if(m_layers, [&ls](const auto& el) { return el.lock() == ls; });
}

void CFadingOutState::cleanupForMonitor(const MONITORID& monid) {
    for (auto const& ww : m_windows) {
        auto w = ww.lock();

        if (!w)
            continue;

        if (w->monitorID() != monid && w->m_monitor)
            continue;

        if (!w->m_fadingOut || w->alphaValue(View::WINDOW_ALPHA_FADE) == 0.f) {
            w->m_fadingOut = false;

            if (!w->m_readyToDelete)
                continue;

            windowState()->removeSafe(w);
            remove(w);

            w.reset();

            Log::logger->log(Log::DEBUG, "Cleanup: destroyed a window");
            return;
        }
    }

    bool layersDirty = false;

    for (auto const& lsr : m_layers) {
        auto ls = lsr.lock();

        if (!ls) {
            layersDirty = true;
            continue;
        }

        if (ls->monitorID() != monid && ls->m_monitor)
            continue;

        // mark blur for recalc
        if (ls->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND || ls->m_layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM) {
            auto mon = State::monitorState()->query().id(monid).run();
            if (mon)
                mon->m_blurFBDirty = true;
        }

        if (ls->m_fadingOut && ls->m_readyToDelete && ls->isFadedOut()) {
            for (auto const& m : State::monitorState()->monitors()) {
                for (auto& lsl : m->m_layerSurfaceLayers) {
                    if (!lsl.empty() && std::ranges::find_if(lsl, [&](auto& other) { return other == ls; }) != lsl.end())
                        std::erase_if(lsl, [&](auto& other) { return other == ls || !other; });
                }
            }

            remove(ls);
            layerState()->removeSafe(ls);

            ls.reset();

            Log::logger->log(Log::DEBUG, "Cleanup: destroyed a layersurface");

            return;
        }
    }

    if (layersDirty)
        removeExpiredLayers();
}

void CFadingOutState::removeExpiredLayers() {
    std::erase_if(m_layers, [](const auto& el) { return el.expired(); });
}

void CFadingOutState::clear() {
    m_windows.clear();
    m_layers.clear();
}

UP<CFadingOutState>& Desktop::fadingOutState() {
    static UP<CFadingOutState> state = makeUnique<CFadingOutState>();
    return state;
}
