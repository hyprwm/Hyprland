#include "ViewQuery.hpp"
#include "ViewStateTracker.hpp"
#include "../state/FocusState.hpp"
#include "../view/LayerSurface.hpp"
#include "../view/WLSurface.hpp"
#include "../view/Window.hpp"
#include "../../helpers/MiscFunctions.hpp"
#include "../../managers/KeybindManager.hpp"
#include "../../protocols/LayerShell.hpp"
#include "../../protocols/core/Compositor.hpp"

#include <hyprutils/string/String.hpp>
#include <re2/re2.h>
#include <utility>

using namespace Desktop;
using namespace Hyprutils::String;

CViewQuery::CViewQuery(const IViewStateTracker& tracker) : m_tracker(tracker) {
    ;
}

CViewQuery&& CViewQuery::type(View::eViewType type) && {
    m_type = type;
    return std::move(*this);
}

CViewQuery&& CViewQuery::surface(SP<CWLSurfaceResource> surface) && {
    m_surface = surface;
    return std::move(*this);
}

CViewQuery&& CViewQuery::handle(uint32_t handle) && {
    m_handle = handle;
    return std::move(*this);
}

CViewQuery&& CViewQuery::selector(std::string_view selector) && {
    m_selector = std::string{selector};
    return std::move(*this);
}

CViewQuery&& CViewQuery::urgent() && {
    m_urgent = true;
    return std::move(*this);
}

CViewQuery&& CViewQuery::forceFocus() && {
    m_forceFocus = true;
    return std::move(*this);
}

CViewQuery&& CViewQuery::mappedOnly(bool mappedOnly) && {
    m_mappedOnly = mappedOnly;
    return std::move(*this);
}

PHLVIEW CViewQuery::run() && {
    if (m_handle)
        return byHandle();

    if (m_selector)
        return bySelector();

    if (m_urgent)
        return urgentWindow();

    if (m_forceFocus)
        return forceFocusWindow();

    if (m_surface)
        return bySurface();

    return nullptr;
}

PHLWINDOW CViewQuery::runWindow() && {
    return View::CWindow::fromView(std::move(*this).run());
}

PHLLS CViewQuery::runLayer() && {
    return View::CLayerSurface::fromView(std::move(*this).run());
}

PHLWINDOW CViewQuery::byHandle() const {
    for (auto const& w : m_tracker.windows()) {
        if (!windowMatchesCommon(w))
            continue;

        if (sc<uint32_t>(rc<uint64_t>(w.get()) & 0xFFFFFFFF) == *m_handle)
            return w;
    }

    return nullptr;
}

PHLWINDOW CViewQuery::bySelector() const {
    auto regexp = trim(*m_selector);

    if (regexp.starts_with("active"))
        return focusState()->window();
    else if (regexp.starts_with("floating") || regexp.starts_with("tiled")) {
        if (!focusState()->window())
            return nullptr;

        const bool FLOAT = regexp.starts_with("floating");

        for (auto const& w : m_tracker.windows()) {
            if (!w->m_isMapped || w->m_isFloating != FLOAT || w->m_workspace != focusState()->window()->m_workspace ||
                w->hasInputBlockedReasonsBesides(Desktop::View::INPUT_BLOCK_BELOW_FULLSCREEN))
                continue;

            return w;
        }

        return nullptr;
    }

    eFocusWindowMode mode = MODE_CLASS_REGEX;

    std::string      regexCheck;
    std::string      matchCheck;
    if (regexp.starts_with("class:")) {
        regexCheck = regexp.substr(6);
    } else if (regexp.starts_with("initialclass:")) {
        mode       = MODE_INITIAL_CLASS_REGEX;
        regexCheck = regexp.substr(13);
    } else if (regexp.starts_with("title:")) {
        mode       = MODE_TITLE_REGEX;
        regexCheck = regexp.substr(6);
    } else if (regexp.starts_with("initialtitle:")) {
        mode       = MODE_INITIAL_TITLE_REGEX;
        regexCheck = regexp.substr(13);
    } else if (regexp.starts_with("tag:")) {
        mode       = MODE_TAG_REGEX;
        regexCheck = regexp.substr(4);
    } else if (regexp.starts_with("stableid:")) {
        mode       = MODE_STABLE_ID;
        matchCheck = regexp.substr(9);
    } else if (regexp.starts_with("address:")) {
        mode       = MODE_ADDRESS;
        matchCheck = regexp.substr(8);
    } else if (regexp.starts_with("pid:")) {
        mode       = MODE_PID;
        matchCheck = regexp.substr(4);
    }

    for (auto const& w : m_tracker.windows()) {
        if (!w->m_isMapped)
            continue;

        if (!windowMatchesCommon(w))
            continue;

        switch (mode) {
            case MODE_CLASS_REGEX: {
                const auto windowClass = w->m_class;
                if (!RE2::FullMatch(windowClass, regexCheck))
                    continue;
                break;
            }
            case MODE_INITIAL_CLASS_REGEX: {
                const auto initialWindowClass = w->m_initialClass;
                if (!RE2::FullMatch(initialWindowClass, regexCheck))
                    continue;
                break;
            }
            case MODE_TITLE_REGEX: {
                const auto windowTitle = w->m_title;
                if (!RE2::FullMatch(windowTitle, regexCheck))
                    continue;
                break;
            }
            case MODE_INITIAL_TITLE_REGEX: {
                const auto initialWindowTitle = w->m_initialTitle;
                if (!RE2::FullMatch(initialWindowTitle, regexCheck))
                    continue;
                break;
            }
            case MODE_TAG_REGEX: {
                bool tagMatched = false;
                for (auto const& t : w->m_ruleApplicator->m_tagKeeper.getTags()) {
                    if (RE2::FullMatch(t, regexCheck)) {
                        tagMatched = true;
                        break;
                    }
                }
                if (!tagMatched)
                    continue;
                break;
            }
            case MODE_STABLE_ID: {
                std::string stableID = std::format("{:x}", w->m_stableID);
                if (matchCheck != stableID)
                    continue;
                break;
            }
            case MODE_ADDRESS: {
                std::string addr = std::format("0x{:x}", rc<uintptr_t>(w.get()));
                if (matchCheck != addr)
                    continue;
                break;
            }
            case MODE_PID: {
                std::string pid = std::format("{}", w->getPID());
                if (matchCheck != pid)
                    continue;
                break;
            }
            default: break;
        }

        return w;
    }

    return nullptr;
}

PHLWINDOW CViewQuery::urgentWindow() const {
    for (auto const& w : m_tracker.windows()) {
        if (!windowMatchesCommon(w))
            continue;

        if (w->m_isMapped && w->m_isUrgent)
            return w;
    }

    return nullptr;
}

PHLWINDOW CViewQuery::forceFocusWindow() const {
    for (auto const& w : m_tracker.windows()) {
        if (!windowMatchesCommon(w))
            continue;

        if (!w->m_isMapped || !w->acceptsInput() || !w->m_workspace || !w->m_workspace->isVisible())
            continue;

        if (!w->m_ruleApplicator->stayFocused().valueOrDefault())
            continue;

        return w;
    }

    return nullptr;
}

PHLVIEW CViewQuery::bySurface() const {
    if (!*m_surface || !(*m_surface)->m_hlSurface)
        return nullptr;

    auto view = (*m_surface)->m_hlSurface->view();
    if (view && typeMatches(view))
        return view;

    if (!m_type || *m_type == View::VIEW_TYPE_LAYER_SURFACE)
        return layerBySurface();

    return nullptr;
}

PHLLS CViewQuery::layerBySurface() const {
    std::pair<SP<CWLSurfaceResource>, bool> result = {*m_surface, false};

    for (auto const& ls : m_tracker.layers()) {
        if (!ls->aliveAndVisible())
            continue;

        if (ls->m_layerSurface->m_surface == *m_surface)
            return ls;

        ls->m_layerSurface->m_surface->breadthfirst(
            [&result](SP<CWLSurfaceResource> surf, const Vector2D& offset, void* data) {
                if (surf == result.first) {
                    result.second = true;
                    return;
                }
            },
            nullptr);

        if (result.second)
            return ls;
    }

    return nullptr;
}

bool CViewQuery::typeMatches(PHLVIEW view) const {
    return view && (!m_type || view->type() == *m_type);
}

bool CViewQuery::windowMatchesCommon(PHLWINDOW window) const {
    if (!window)
        return false;

    if (m_type && *m_type != View::VIEW_TYPE_WINDOW)
        return false;

    if (m_mappedOnly && !window->m_isMapped)
        return false;

    return true;
}
