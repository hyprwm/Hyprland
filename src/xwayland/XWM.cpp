#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <xcb/xfixes.h>
#include <xcb/xproto.h>
#ifndef NO_XWAYLAND

#include <fcntl.h>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <xcb/xcb_icccm.h>

#include "XWayland.hpp"
#include "../Compositor.hpp"
#include "../protocols/core/Seat.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/SeatManager.hpp"
#include "../managers/ANRManager.hpp"
#include "../protocols/XWaylandShell.hpp"
#include "../protocols/core/Compositor.hpp"
using Hyprutils::Memory::CUniquePointer;

using namespace Hyprutils::OS;

#define XCB_EVENT_RESPONSE_TYPE_MASK 0x7f
constexpr size_t INCR_CHUNK_SIZE = 64ul * 1024;

static int       onX11Event(int fd, uint32_t mask, void* data) {
    return g_pXWayland->m_wm->onEvent(fd, mask);
}

struct SFreeDeleter {
    void operator()(void* ptr) const {
        std::free(ptr); // NOLINT(cppcoreguidelines-no-malloc)
    }
};

template <typename T>
using XCBReplyPtr = std::unique_ptr<T, SFreeDeleter>;

SP<CXWaylandSurface> CXWM::windowForXID(xcb_window_t wid) {
    for (auto const& s : m_surfaces) {
        if (s->m_xID == wid)
            return s;
    }

    return nullptr;
}

void CXWM::handleCreate(xcb_create_notify_event_t* e) {
    if (isWMWindow(e->window))
        return;

    const auto XSURF = m_surfaces.emplace_back(SP<CXWaylandSurface>(new CXWaylandSurface(e->window, CBox{e->x, e->y, e->width, e->height}, e->override_redirect)));
    XSURF->m_self    = XSURF;
    Debug::log(LOG, "[xwm] New XSurface at {:x} with xid of {}", rc<uintptr_t>(XSURF.get()), e->window);

    const auto WINDOW = CWindow::create(XSURF);
    g_pCompositor->m_windows.emplace_back(WINDOW);
    WINDOW->m_self = WINDOW;
    Debug::log(LOG, "[xwm] New XWayland window at {:x} for surf {:x}", rc<uintptr_t>(WINDOW.get()), rc<uintptr_t>(XSURF.get()));
}

void CXWM::handleDestroy(xcb_destroy_notify_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    XSURF->m_events.destroy.emit();
    std::erase_if(m_surfaces, [XSURF](const auto& other) { return XSURF == other; });
}

void CXWM::handleConfigureRequest(xcb_configure_request_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    const uint16_t     MASK     = e->value_mask;
    constexpr uint16_t GEOMETRY = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    if (!(MASK & GEOMETRY))
        return;

    XSURF->m_events.configureRequest.emit(CBox{MASK & XCB_CONFIG_WINDOW_X ? e->x : XSURF->m_geometry.x, MASK & XCB_CONFIG_WINDOW_Y ? e->y : XSURF->m_geometry.y,
                                               MASK & XCB_CONFIG_WINDOW_WIDTH ? e->width : XSURF->m_geometry.width,
                                               MASK & XCB_CONFIG_WINDOW_HEIGHT ? e->height : XSURF->m_geometry.height});
}

void CXWM::handleConfigureNotify(xcb_configure_notify_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    if (XSURF->m_geometry == CBox{e->x, e->y, e->width, e->height})
        return;

    XSURF->m_geometry = {e->x, e->y, e->width, e->height};
    updateOverrideRedirect(XSURF, e->override_redirect);
    XSURF->m_events.setGeometry.emit();
}

void CXWM::handleMapRequest(xcb_map_request_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    xcb_map_window(getConnection(), e->window);

    XSURF->restackToTop();

    const bool SMALL =
        XSURF->m_geometry.size() < Vector2D{2, 2} || (XSURF->m_sizeHints && XSURF->m_geometry.size() < Vector2D{XSURF->m_sizeHints->min_width, XSURF->m_sizeHints->min_height});
    const bool HAS_HINTS   = XSURF->m_sizeHints && Vector2D{XSURF->m_sizeHints->base_width, XSURF->m_sizeHints->base_height} > Vector2D{5, 5};
    const auto DESIREDSIZE = HAS_HINTS ? Vector2D{XSURF->m_sizeHints->base_width, XSURF->m_sizeHints->base_height} : Vector2D{800, 800};

    // if it's too small, configure it.
    if (SMALL && !XSURF->m_overrideRedirect) // default to 800 x 800
        XSURF->configure({XSURF->m_geometry.pos(), DESIREDSIZE});

    Debug::log(LOG, "[xwm] Mapping window {} in X (geometry {}x{} at {}x{}))", e->window, XSURF->m_geometry.width, XSURF->m_geometry.height, XSURF->m_geometry.x,
               XSURF->m_geometry.y);

    // read data again. Some apps for some reason fail to send WINDOW_TYPE
    // this shouldn't happen but does, I prolly fucked up somewhere, this is a band-aid
    readWindowData(XSURF);

    XSURF->considerMap();
}

void CXWM::handleMapNotify(xcb_map_notify_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    updateOverrideRedirect(XSURF, e->override_redirect);

    if (XSURF->m_overrideRedirect)
        return;

    XSURF->setWithdrawn(false);
    sendState(XSURF);
    xcb_flush(getConnection());

    XSURF->considerMap();
}

void CXWM::handleUnmapNotify(xcb_unmap_notify_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    XSURF->unmap();
    dissociate(XSURF);

    if (XSURF->m_overrideRedirect)
        return;

    XSURF->setWithdrawn(true);
    sendState(XSURF);
    xcb_flush(getConnection());
}

static bool lookupParentExists(SP<CXWaylandSurface> XSURF, SP<CXWaylandSurface> prospectiveChild) {
    std::vector<SP<CXWaylandSurface>> visited;

    while (XSURF->m_parent) {
        if (XSURF->m_parent == prospectiveChild)
            return true;
        visited.emplace_back(XSURF);

        XSURF = XSURF->m_parent.lock();

        if (std::ranges::find(visited, XSURF) != visited.end())
            return false;
    }

    return false;
}

std::string CXWM::getAtomName(uint32_t atom) {
    for (auto const& ha : HYPRATOMS) {
        if (ha.second != atom)
            continue;

        return ha.first;
    }

    // Get the name of the atom
    const auto                             cookie = xcb_get_atom_name(getConnection(), atom);
    XCBReplyPtr<xcb_get_atom_name_reply_t> reply(xcb_get_atom_name_reply(getConnection(), cookie, nullptr));

    if (!reply)
        return "Unknown";

    auto const name_len = xcb_get_atom_name_name_length(reply.get());
    auto*      name     = xcb_get_atom_name_name(reply.get());

    return {name, name_len};
}

void CXWM::readProp(SP<CXWaylandSurface> XSURF, uint32_t atom, xcb_get_property_reply_t* reply) {
    std::string propName;
    if (Debug::m_trace)
        propName = getAtomName(atom);

    const auto  valueLen = xcb_get_property_value_length(reply);
    const auto* value    = sc<const char*>(xcb_get_property_value(reply));

    auto        handleWMClass = [&]() {
        XSURF->m_state.appid = std::string{value, valueLen};
        if (std::count(XSURF->m_state.appid.begin(), XSURF->m_state.appid.end(), '\000') == 2)
            XSURF->m_state.appid = XSURF->m_state.appid.substr(XSURF->m_state.appid.find_first_of('\000') + 1);

        if (!XSURF->m_state.appid.empty())
            XSURF->m_state.appid.pop_back();
        XSURF->m_events.metadataChanged.emit();
    };

    auto handleWMName = [&]() {
        if (reply->type != HYPRATOMS["UTF8_STRING"] && reply->type != HYPRATOMS["TEXT"] && reply->type != XCB_ATOM_STRING)
            return;
        XSURF->m_state.title = std::string{value, valueLen};
        XSURF->m_events.metadataChanged.emit();
    };

    auto handleWindowType = [&]() {
        auto* atomsArr = rc<const xcb_atom_t*>(value);
        XSURF->m_atoms.assign(atomsArr, atomsArr + reply->value_len);
    };

    auto handleWMState = [&]() {
        auto* atoms = rc<const xcb_atom_t*>(value);
        for (uint32_t i = 0; i < reply->value_len; i++) {
            if (atoms[i] == HYPRATOMS["_NET_WM_STATE_MODAL"])
                XSURF->m_modal = true;
        }
    };

    auto handleWMHints = [&]() {
        if (reply->value_len == 0)
            return;
        XSURF->m_hints = makeUnique<xcb_icccm_wm_hints_t>();
        xcb_icccm_get_wm_hints_from_reply(XSURF->m_hints.get(), reply);
        if (!(XSURF->m_hints->flags & XCB_ICCCM_WM_HINT_INPUT))
            XSURF->m_hints->input = true;
    };

    auto handleWMRole = [&]() {
        if (valueLen <= 0)
            XSURF->m_role = "";
        else {
            XSURF->m_role = std::string{value, valueLen};
            XSURF->m_role = XSURF->m_role.substr(0, XSURF->m_role.find_first_of('\000'));
        }
    };

    auto handleTransientFor = [&]() {
        if (reply->type != XCB_ATOM_WINDOW)
            return;
        const auto XID     = rc<const xcb_window_t*>(value);
        XSURF->m_transient = XID;
        if (!XID)
            return;

        if (const auto NEWXSURF = windowForXID(*XID); NEWXSURF && !lookupParentExists(XSURF, NEWXSURF)) {
            XSURF->m_parent = NEWXSURF;
            NEWXSURF->m_children.emplace_back(XSURF);
        } else
            Debug::log(LOG, "[xwm] Denying transient because it would create a loop");
    };

    auto handleSizeHints = [&]() {
        if (reply->type != HYPRATOMS["WM_SIZE_HINTS"] || reply->value_len == 0)
            return;

        XSURF->m_sizeHints = makeUnique<xcb_size_hints_t>();
        std::memset(XSURF->m_sizeHints.get(), 0, sizeof(xcb_size_hints_t));
        xcb_icccm_get_wm_size_hints_from_reply(XSURF->m_sizeHints.get(), reply);

        const int32_t FLAGS   = XSURF->m_sizeHints->flags;
        const bool    HASMIN  = FLAGS & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE;
        const bool    HASBASE = FLAGS & XCB_ICCCM_SIZE_HINT_BASE_SIZE;

        if (!HASMIN && !HASBASE) {
            XSURF->m_sizeHints->min_width = XSURF->m_sizeHints->min_height = -1;
            XSURF->m_sizeHints->base_width = XSURF->m_sizeHints->base_height = -1;
        } else if (!HASBASE) {
            XSURF->m_sizeHints->base_width  = XSURF->m_sizeHints->min_width;
            XSURF->m_sizeHints->base_height = XSURF->m_sizeHints->min_height;
        } else if (!HASMIN) {
            XSURF->m_sizeHints->min_width  = XSURF->m_sizeHints->base_width;
            XSURF->m_sizeHints->min_height = XSURF->m_sizeHints->base_height;
        }

        if (!(FLAGS & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE))
            XSURF->m_sizeHints->max_width = XSURF->m_sizeHints->max_height = -1;
    };

    auto handleWMProtocols = [&]() {
        if (reply->type != XCB_ATOM_ATOM)
            return;
        auto* atoms = rc<const xcb_atom_t*>(value);
        XSURF->m_protocols.assign(atoms, atoms + reply->value_len);
    };

    if (atom == XCB_ATOM_WM_CLASS)
        handleWMClass();
    else if (atom == XCB_ATOM_WM_NAME || atom == HYPRATOMS["_NET_WM_NAME"])
        handleWMName();
    else if (atom == HYPRATOMS["_NET_WM_WINDOW_TYPE"])
        handleWindowType();
    else if (atom == HYPRATOMS["_NET_WM_STATE"])
        handleWMState();
    else if (atom == HYPRATOMS["WM_HINTS"])
        handleWMHints();
    else if (atom == HYPRATOMS["WM_WINDOW_ROLE"])
        handleWMRole();
    else if (atom == XCB_ATOM_WM_TRANSIENT_FOR)
        handleTransientFor();
    else if (atom == HYPRATOMS["WM_NORMAL_HINTS"])
        handleSizeHints();
    else if (atom == HYPRATOMS["WM_PROTOCOLS"])
        handleWMProtocols();
    else {
        Debug::log(TRACE, "[xwm] Unhandled prop {} -> {}", atom, propName);
        return;
    }

    Debug::log(TRACE, "[xwm] Handled prop {} -> {}", atom, propName);
}

void CXWM::handlePropertyNotify(xcb_property_notify_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    xcb_get_property_cookie_t             cookie = xcb_get_property(getConnection(), 0, XSURF->m_xID, e->atom, XCB_ATOM_ANY, 0, 2048);
    XCBReplyPtr<xcb_get_property_reply_t> reply(xcb_get_property_reply(getConnection(), cookie, nullptr));

    if (!reply) {
        Debug::log(ERR, "[xwm] Failed to read property notify cookie");
        return;
    }

    readProp(XSURF, e->atom, reply.get());
}

void CXWM::handleClientMessage(xcb_client_message_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    std::string propName = getAtomName(e->type);

    if (e->type == HYPRATOMS["WM_PROTOCOLS"]) {
        if (e->data.data32[1] == XSURF->m_lastPingSeq && e->data.data32[0] == HYPRATOMS["_NET_WM_PING"]) {
            g_pANRManager->onResponse(XSURF);
            return;
        }
    } else if (e->type == HYPRATOMS["WL_SURFACE_ID"]) {
        if (XSURF->m_surface) {
            Debug::log(WARN, "[xwm] Re-assignment of WL_SURFACE_ID");
            dissociate(XSURF);
        }

        auto id       = e->data.data32[0];
        auto resource = wl_client_get_object(g_pXWayland->m_server->m_xwaylandClient, id);
        if (resource) {
            auto surf = CWLSurfaceResource::fromResource(resource);
            associate(XSURF, surf);
        }
    } else if (e->type == HYPRATOMS["WL_SURFACE_SERIAL"]) {
        if (XSURF->m_wlSerial) {
            Debug::log(WARN, "[xwm] Re-assignment of WL_SURFACE_SERIAL");
            dissociate(XSURF);
        }

        uint32_t serialLow  = e->data.data32[0];
        uint32_t serialHigh = e->data.data32[1];
        XSURF->m_wlSerial   = (sc<uint64_t>(serialHigh) << 32) | serialLow;

        Debug::log(LOG, "[xwm] surface {:x} requests serial {:x}", rc<uintptr_t>(XSURF.get()), XSURF->m_wlSerial);

        for (auto const& res : m_shellResources) {
            if (!res)
                continue;

            if (res->m_serial != XSURF->m_wlSerial || !XSURF->m_wlSerial)
                continue;

            associate(XSURF, res->m_surface.lock());
            break;
        }

    } else if (e->type == HYPRATOMS["_NET_WM_STATE"]) {
        if (e->format == 32) {
            uint32_t action = e->data.data32[0];
            for (size_t i = 0; i < 2; ++i) {
                xcb_atom_t prop = e->data.data32[1 + i];

                auto       updateState = [XSURF](int action, bool current) -> bool {
                    switch (action) {
                        case 0:
                            /* remove */
                            return false;
                        case 1:
                            /* add */
                            return true;
                        case 2:
                            /* toggle */
                            return !current;
                        default: return false;
                    }
                    return false;
                };

                if (prop == HYPRATOMS["_NET_WM_STATE_FULLSCREEN"])
                    XSURF->m_state.requestsFullscreen = updateState(action, XSURF->m_fullscreen);
                if (prop == HYPRATOMS["_NET_WM_STATE_HIDDEN"])
                    XSURF->m_state.requestsMinimize = updateState(action, XSURF->m_minimized);
                if (prop == HYPRATOMS["_NET_WM_STATE_MAXIMIZED_VERT"] || prop == HYPRATOMS["_NET_WM_STATE_MAXIMIZED_HORZ"])
                    XSURF->m_state.requestsMaximize = updateState(action, XSURF->m_maximized);
            }

            XSURF->m_events.stateChanged.emit();
        }
    } else if (e->type == HYPRATOMS["WM_CHANGE_STATE"]) {
        int state = e->data.data32[0];
        if (state == XCB_ICCCM_WM_STATE_ICONIC || state == XCB_ICCCM_WM_STATE_WITHDRAWN)
            XSURF->m_state.requestsMinimize = true;
        else if (state == XCB_ICCCM_WM_STATE_NORMAL)
            XSURF->m_state.requestsMinimize = false;
        XSURF->m_events.stateChanged.emit();
    } else if (e->type == HYPRATOMS["_NET_ACTIVE_WINDOW"]) {
        XSURF->m_events.activate.emit();
    } else if (e->type == HYPRATOMS["XdndStatus"]) {
        if (m_dndDataOffers.empty() || !m_dndDataOffers.at(0)->getSource()) {
            Debug::log(TRACE, "[xwm] Rejecting XdndStatus message: nothing to get");
            return;
        }

        xcb_client_message_data_t* data     = &e->data;
        const bool                 ACCEPTED = data->data32[1] & 1;

        if (ACCEPTED)
            m_dndDataOffers.at(0)->getSource()->accepted("");

        Debug::log(LOG, "[xwm] XdndStatus: accepted: {}");
    } else if (e->type == HYPRATOMS["XdndFinished"]) {
        if (m_dndDataOffers.empty() || !m_dndDataOffers.at(0)->getSource()) {
            Debug::log(TRACE, "[xwm] Rejecting XdndFinished message: nothing to get");
            return;
        }

        m_dndDataOffers.at(0)->getSource()->sendDndFinished();

        Debug::log(LOG, "[xwm] XdndFinished");
    } else {
        Debug::log(TRACE, "[xwm] Unhandled message prop {} -> {}", e->type, propName);
        return;
    }

    Debug::log(TRACE, "[xwm] Handled message prop {} -> {}", e->type, propName);
}

void CXWM::handleFocusIn(xcb_focus_in_event_t* e) {
    if (e->mode == XCB_NOTIFY_MODE_GRAB || e->mode == XCB_NOTIFY_MODE_UNGRAB || e->detail == XCB_NOTIFY_DETAIL_POINTER)
        return;

    const auto XSURF = windowForXID(e->event);

    if (!XSURF)
        return;

    if (m_focusedSurface && m_focusedSurface->m_pid == XSURF->m_pid && e->sequence - m_lastFocusSeq <= 255)
        focusWindow(XSURF);
    else
        focusWindow(m_focusedSurface.lock());
}

void CXWM::handleFocusOut(xcb_focus_out_event_t* e) {
    Debug::log(TRACE, "[xwm] focusOut mode={}, detail={}, event={}", e->mode, e->detail, e->event);

    const auto XSURF = windowForXID(e->event);

    if (!XSURF)
        return;

    Debug::log(TRACE, "[xwm] focusOut for {} {} {} surface {}", XSURF->m_mapped ? "mapped" : "unmapped", XSURF->m_fullscreen ? "fullscreen" : "windowed",
               XSURF == m_focusedSurface ? "focused" : "unfocused", XSURF->m_state.title);

    // do something?
}

void CXWM::sendWMMessage(SP<CXWaylandSurface> surf, xcb_client_message_data_t* data, uint32_t mask) {
    xcb_client_message_event_t event = {
        .response_type = XCB_CLIENT_MESSAGE,
        .format        = 32,
        .sequence      = 0,
        .window        = surf->m_xID,
        .type          = HYPRATOMS["WM_PROTOCOLS"],
        .data          = *data,
    };

    xcb_send_event(getConnection(), 0, surf->m_xID, mask, rc<const char*>(&event));
    xcb_flush(getConnection());
}

void CXWM::focusWindow(SP<CXWaylandSurface> surf) {
    if (surf == m_focusedSurface)
        return;

    m_focusedSurface = surf;

    // send state to all toplevel surfaces, sometimes we might lose some
    // that could still stick with the focused atom
    for (auto const& s : m_mappedSurfaces) {
        if (!s || s->m_overrideRedirect)
            continue;

        sendState(s.lock());
    }

    if (!surf) {
        xcb_set_input_focus_checked(getConnection(), XCB_INPUT_FOCUS_POINTER_ROOT, XCB_NONE, XCB_CURRENT_TIME);
        return;
    }

    if (surf->m_overrideRedirect)
        return;

    xcb_client_message_data_t msg = {{0}};
    msg.data32[0]                 = HYPRATOMS["WM_TAKE_FOCUS"];
    msg.data32[1]                 = XCB_TIME_CURRENT_TIME;

    if (surf->m_hints && !surf->m_hints->input)
        sendWMMessage(surf, &msg, XCB_EVENT_MASK_NO_EVENT);
    else {
        sendWMMessage(surf, &msg, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT);

        xcb_void_cookie_t cookie = xcb_set_input_focus(getConnection(), XCB_INPUT_FOCUS_POINTER_ROOT, surf->m_xID, XCB_CURRENT_TIME);
        m_lastFocusSeq           = cookie.sequence;
    }
}

void CXWM::handleError(xcb_value_error_t* e) {
    const char* major_name = xcb_errors_get_name_for_major_code(m_errors, e->major_opcode);
    if (!major_name) {
        Debug::log(ERR, "xcb error happened, but could not get major name");
        return;
    }

    const char* minor_name = xcb_errors_get_name_for_minor_code(m_errors, e->major_opcode, e->minor_opcode);

    const char* extension;
    const char* error_name = xcb_errors_get_name_for_error(m_errors, e->error_code, &extension);
    if (!error_name) {
        Debug::log(ERR, "xcb error happened, but could not get error name");
        return;
    }

    Debug::log(ERR, "[xwm] xcb error: {} ({}), code {} ({}), seq {}, val {}", major_name, minor_name ? minor_name : "no minor", error_name, extension ? extension : "no extension",
               e->sequence, e->bad_value);
}

void CXWM::selectionSendNotify(xcb_selection_request_event_t* e, bool success) {
    xcb_selection_notify_event_t selection_notify = {
        .response_type = XCB_SELECTION_NOTIFY,
        .sequence      = 0,
        .time          = e->time,
        .requestor     = e->requestor,
        .selection     = e->selection,
        .target        = e->target,
        .property      = success ? e->property : sc<uint32_t>(XCB_ATOM_NONE),
    };

    xcb_send_event(getConnection(), 0, e->requestor, XCB_EVENT_MASK_NO_EVENT, rc<const char*>(&selection_notify));
    xcb_flush(getConnection());
}

xcb_atom_t CXWM::mimeToAtom(const std::string& mime) {
    if (mime == "text/plain;charset=utf-8")
        return HYPRATOMS["UTF8_STRING"];
    if (mime == "text/plain")
        return HYPRATOMS["TEXT"];

    xcb_intern_atom_cookie_t             cookie = xcb_intern_atom(getConnection(), 0, mime.length(), mime.c_str());
    XCBReplyPtr<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(getConnection(), cookie, nullptr));
    if (!reply.get())
        return XCB_ATOM_NONE;
    xcb_atom_t atom = reply->atom;

    return atom;
}

std::string CXWM::mimeFromAtom(xcb_atom_t atom) {
    if (atom == HYPRATOMS["UTF8_STRING"])
        return "text/plain;charset=utf-8";
    if (atom == HYPRATOMS["TEXT"])
        return "text/plain";

    xcb_get_atom_name_cookie_t             cookie = xcb_get_atom_name(getConnection(), atom);
    XCBReplyPtr<xcb_get_atom_name_reply_t> reply(xcb_get_atom_name_reply(getConnection(), cookie, nullptr));
    if (!reply)
        return "INVALID";
    size_t      len = xcb_get_atom_name_name_length(reply.get());
    char*       buf = xcb_get_atom_name_name(reply.get()); // not a C string
    std::string SZNAME{buf, len};
    return SZNAME;
}

void CXWM::handleSelectionNotify(xcb_selection_notify_event_t* e) {
    Debug::log(TRACE, "[xwm] Selection notify for {} prop {} target {}", e->selection, e->property, e->target);

    SXSelection* sel = getSelection(e->selection);

    if (e->property == XCB_ATOM_NONE) {
        auto it = std::ranges::find_if(sel->transfers, [](const auto& t) { return !t->propertyReply; });
        if (it != sel->transfers.end()) {
            Debug::log(TRACE, "[xwm] converting selection failed");
            sel->transfers.erase(it);
        }
    } else if (e->target == HYPRATOMS["TARGETS"]) {
        if (!m_focusedSurface) {
            Debug::log(TRACE, "[xwm] denying access to write to clipboard because no X client is in focus");
            return;
        }

        setClipboardToWayland(*sel);
    } else if (!sel->transfers.empty())
        getTransferData(*sel);
}

bool CXWM::handleSelectionPropertyNotify(xcb_property_notify_event_t* e) {
    if (e->state != XCB_PROPERTY_DELETE)
        return false;

    for (auto* sel : {&m_clipboard, &m_primarySelection}) {
        auto it = std::ranges::find_if(sel->transfers, [e](const auto& t) { return t->incomingWindow == e->window; });
        if (it != sel->transfers.end()) {
            if (!(*it)->getIncomingSelectionProp(true)) {
                sel->transfers.erase(it);
                return false;
            }
            getTransferData(*sel);
            return true;
        }
    }

    return false;
}

SXSelection* CXWM::getSelection(xcb_atom_t atom) {
    if (atom == HYPRATOMS["CLIPBOARD"])
        return &m_clipboard;
    else if (atom == HYPRATOMS["PRIMARY"])
        return &m_primarySelection;
    else if (atom == HYPRATOMS["XdndSelection"])
        return &m_dndSelection;

    return nullptr;
}

void CXWM::handleSelectionRequest(xcb_selection_request_event_t* e) {
    Debug::log(TRACE, "[xwm] Selection request for {} prop {} target {} time {} requestor {} selection {}", e->selection, e->property, e->target, e->time, e->requestor,
               e->selection);

    SXSelection* sel = getSelection(e->selection);

    if (!sel) {
        Debug::log(ERR, "[xwm] No selection");
        selectionSendNotify(e, false);
        return;
    }

    if (e->selection == HYPRATOMS["CLIPBOARD_MANAGER"]) {
        selectionSendNotify(e, true);
        return;
    }

    if (sel->window != e->owner && e->time != XCB_CURRENT_TIME && e->time < sel->timestamp) {
        Debug::log(ERR, "[xwm] outdated selection request. Time {} < {}", e->time, sel->timestamp);
        selectionSendNotify(e, false);
        return;
    }

    if (!g_pSeatManager->m_state.keyboardFocusResource || g_pSeatManager->m_state.keyboardFocusResource->client() != g_pXWayland->m_server->m_xwaylandClient) {
        Debug::log(TRACE, "[xwm] Ignoring clipboard access: xwayland not in focus");
        selectionSendNotify(e, false);
        return;
    }

    if (e->target == HYPRATOMS["TARGETS"]) {
        // send mime types
        std::vector<std::string> mimes;
        if (sel == &m_clipboard && g_pSeatManager->m_selection.currentSelection)
            mimes = g_pSeatManager->m_selection.currentSelection->mimes();
        else if (sel == &m_dndSelection && !m_dndDataOffers.empty() && m_dndDataOffers.at(0)->m_source)
            mimes = m_dndDataOffers.at(0)->m_source->mimes();

        if (mimes.empty())
            Debug::log(WARN, "[xwm] WARNING: No mimes in TARGETS?");

        std::vector<xcb_atom_t> atoms;
        // reserve to avoid reallocations
        atoms.reserve(mimes.size() + 2);
        atoms.push_back(HYPRATOMS["TIMESTAMP"]);
        atoms.push_back(HYPRATOMS["TARGETS"]);

        for (auto const& m : mimes) {
            atoms.push_back(mimeToAtom(m));
        }

        xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, e->requestor, e->property, XCB_ATOM_ATOM, 32, atoms.size(), atoms.data());
        selectionSendNotify(e, true);
    } else if (e->target == HYPRATOMS["TIMESTAMP"]) {
        xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, e->requestor, e->property, XCB_ATOM_INTEGER, 32, 1, &sel->timestamp);
        selectionSendNotify(e, true);
    } else if (e->target == HYPRATOMS["DELETE"]) {
        selectionSendNotify(e, true);
    } else {
        std::string mime = mimeFromAtom(e->target);

        if (mime == "INVALID") {
            Debug::log(LOG, "[xwm] Ignoring clipboard access: invalid mime atom {}", e->target);
            selectionSendNotify(e, false);
            return;
        }

        if (!sel->sendData(e, mime)) {
            Debug::log(LOG, "[xwm] Failed to send selection :(");
            selectionSendNotify(e, false);
            return;
        }
    }
}

bool CXWM::handleSelectionXFixesNotify(xcb_xfixes_selection_notify_event_t* e) {
    Debug::log(TRACE, "[xwm] Selection xfixes notify for {}", e->selection);

    // IMPORTANT: mind the g_pSeatManager below
    SXSelection* sel = getSelection(e->selection);

    if (sel == &m_dndSelection)
        return true;

    if (e->owner == XCB_WINDOW_NONE) {
        if (sel->owner != sel->window) {
            if (sel == &m_clipboard)
                g_pSeatManager->setCurrentSelection(nullptr);
            else if (sel == &m_primarySelection)
                g_pSeatManager->setCurrentPrimarySelection(nullptr);
        }

        sel->owner = 0;
        return true;
    }

    sel->owner = e->owner;

    if (sel->owner == sel->window) {
        sel->timestamp = e->timestamp;
        return true;
    }

    if (sel == &m_clipboard)
        xcb_convert_selection(getConnection(), sel->window, HYPRATOMS["CLIPBOARD"], HYPRATOMS["TARGETS"], HYPRATOMS["_WL_SELECTION"], e->timestamp);
    else if (sel == &m_primarySelection)
        xcb_convert_selection(getConnection(), sel->window, HYPRATOMS["PRIMARY"], HYPRATOMS["TARGETS"], HYPRATOMS["_WL_SELECTION"], e->timestamp);
    xcb_flush(getConnection());

    return true;
}

bool CXWM::handleSelectionEvent(xcb_generic_event_t* e) {
    switch (e->response_type & XCB_EVENT_RESPONSE_TYPE_MASK) {
        case XCB_SELECTION_NOTIFY: {
            handleSelectionNotify(rc<xcb_selection_notify_event_t*>(e));
            return true;
        }
        case XCB_PROPERTY_NOTIFY: {
            return handleSelectionPropertyNotify(rc<xcb_property_notify_event_t*>(e));
        }
        case XCB_SELECTION_REQUEST: {
            handleSelectionRequest(rc<xcb_selection_request_event_t*>(e));
            return true;
        }
    }

    if (e->response_type - m_xfixes->first_event == XCB_XFIXES_SELECTION_NOTIFY)
        return handleSelectionXFixesNotify(rc<xcb_xfixes_selection_notify_event_t*>(e));

    return false;
}

int CXWM::onEvent(int fd, uint32_t mask) {

    if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
        Debug::log(ERR, "XWayland has yeeten the xwm off?!");
        Debug::log(CRIT, "XWayland has yeeten the xwm off?!");
        // Attempt to create fresh instance
        g_pEventLoopManager->doLater([]() {
            g_pXWayland->m_wm.reset();
            g_pXWayland->m_server.reset();
            g_pXWayland = makeUnique<CXWayland>(true);
        });
        return 0;
    }

    int processedEventCount = 0;
    using XCBEventPtr       = std::unique_ptr<xcb_generic_event_t, decltype(&free)>;
    while (true) {
        XCBEventPtr event(xcb_poll_for_event(getConnection()), &free);
        if (!event)
            break;

        processedEventCount++;

        if (handleSelectionEvent(event.get()))
            continue;

        switch (event->response_type & XCB_EVENT_RESPONSE_TYPE_MASK) {
            case XCB_CREATE_NOTIFY: handleCreate(rc<xcb_create_notify_event_t*>(event.get())); break;
            case XCB_DESTROY_NOTIFY: handleDestroy(rc<xcb_destroy_notify_event_t*>(event.get())); break;
            case XCB_CONFIGURE_REQUEST: handleConfigureRequest(rc<xcb_configure_request_event_t*>(event.get())); break;
            case XCB_CONFIGURE_NOTIFY: handleConfigureNotify(rc<xcb_configure_notify_event_t*>(event.get())); break;
            case XCB_MAP_REQUEST: handleMapRequest(rc<xcb_map_request_event_t*>(event.get())); break;
            case XCB_MAP_NOTIFY: handleMapNotify(rc<xcb_map_notify_event_t*>(event.get())); break;
            case XCB_UNMAP_NOTIFY: handleUnmapNotify(rc<xcb_unmap_notify_event_t*>(event.get())); break;
            case XCB_PROPERTY_NOTIFY: handlePropertyNotify(rc<xcb_property_notify_event_t*>(event.get())); break;
            case XCB_CLIENT_MESSAGE: handleClientMessage(rc<xcb_client_message_event_t*>(event.get())); break;
            case XCB_FOCUS_IN: handleFocusIn(rc<xcb_focus_in_event_t*>(event.get())); break;
            case XCB_FOCUS_OUT: handleFocusOut(rc<xcb_focus_out_event_t*>(event.get())); break;
            case 0: handleError(rc<xcb_value_error_t*>(event.get())); break;
            default: {
                Debug::log(TRACE, "[xwm] unhandled event {}", event->response_type & XCB_EVENT_RESPONSE_TYPE_MASK);
            }
        }
    }

    if (processedEventCount)
        xcb_flush(getConnection());

    return processedEventCount;
}

void CXWM::gatherResources() {
    xcb_prefetch_extension_data(getConnection(), &xcb_xfixes_id);
    xcb_prefetch_extension_data(getConnection(), &xcb_composite_id);
    xcb_prefetch_extension_data(getConnection(), &xcb_res_id);

    for (auto& ATOM : HYPRATOMS) {
        xcb_intern_atom_cookie_t             cookie = xcb_intern_atom(getConnection(), 0, ATOM.first.length(), ATOM.first.c_str());
        XCBReplyPtr<xcb_intern_atom_reply_t> reply(xcb_intern_atom_reply(getConnection(), cookie, nullptr));

        if (!reply) {
            Debug::log(ERR, "[xwm] Atom failed: {}", ATOM.first);
            continue;
        }

        ATOM.second = reply->atom;
    }

    m_xfixes = xcb_get_extension_data(getConnection(), &xcb_xfixes_id);

    if (!m_xfixes || !m_xfixes->present)
        Debug::log(WARN, "XFixes not available");

    auto                                          xfixes_cookie = xcb_xfixes_query_version(getConnection(), XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
    XCBReplyPtr<xcb_xfixes_query_version_reply_t> xfixes_reply(xcb_xfixes_query_version_reply(getConnection(), xfixes_cookie, nullptr));

    if (xfixes_reply) {
        Debug::log(LOG, "xfixes version: {}.{}", xfixes_reply->major_version, xfixes_reply->minor_version);
        m_xfixesMajor = xfixes_reply->major_version;
    }

    const auto* xresReply1 = xcb_get_extension_data(getConnection(), &xcb_res_id);
    if (!xresReply1 || !xresReply1->present)
        return;

    auto                                       xres_cookie = xcb_res_query_version(getConnection(), XCB_RES_MAJOR_VERSION, XCB_RES_MINOR_VERSION);
    XCBReplyPtr<xcb_res_query_version_reply_t> xres_reply(xcb_res_query_version_reply(getConnection(), xres_cookie, nullptr));
    if (!xres_reply)
        return;

    Debug::log(LOG, "xres version: {}.{}", xres_reply->server_major, xres_reply->server_minor);
    if (xres_reply->server_major > 1 || (xres_reply->server_major == 1 && xres_reply->server_minor >= 2)) {
        m_xres = xresReply1;
    }
}

void CXWM::getVisual() {
    xcb_depth_iterator_t      d_iter;
    xcb_visualtype_iterator_t vt_iter;
    xcb_visualtype_t*         visualtype;

    d_iter     = xcb_screen_allowed_depths_iterator(m_screen);
    visualtype = nullptr;
    while (d_iter.rem > 0) {
        if (d_iter.data->depth == 32) {
            vt_iter    = xcb_depth_visuals_iterator(d_iter.data);
            visualtype = vt_iter.data;
            break;
        }

        xcb_depth_next(&d_iter);
    }

    if (visualtype == nullptr) {
        Debug::log(LOG, "xwm: No 32-bit visualtype");
        return;
    }

    m_visualID = visualtype->visual_id;
    m_colormap = xcb_generate_id(getConnection());
    xcb_create_colormap(getConnection(), XCB_COLORMAP_ALLOC_NONE, m_colormap, m_screen->root, m_visualID);
}

void CXWM::getRenderFormat() {
    auto                                               cookie = xcb_render_query_pict_formats(getConnection());
    XCBReplyPtr<xcb_render_query_pict_formats_reply_t> reply(xcb_render_query_pict_formats_reply(getConnection(), cookie, nullptr));

    if (!reply) {
        Debug::log(LOG, "xwm: No xcb_render_query_pict_formats_reply_t reply");
        return;
    }

    auto                       iter   = xcb_render_query_pict_formats_formats_iterator(reply.get());
    xcb_render_pictforminfo_t* format = nullptr;
    while (iter.rem > 0) {
        if (iter.data->depth == 32) {
            format = iter.data;
            break;
        }

        xcb_render_pictforminfo_next(&iter);
    }

    if (format == nullptr) {
        Debug::log(LOG, "xwm: No 32-bit render format");
        return;
    }

    m_renderFormatID = format->id;
}

CXWM::CXWM() : m_connection(makeUnique<CXCBConnection>(g_pXWayland->m_server->m_xwmFDs[0].get())) {

    if (m_connection->hasError()) {
        Debug::log(ERR, "[xwm] Couldn't start, error {}", m_connection->hasError());
        return;
    }

    CXCBErrorContext xcbErrCtx(getConnection());
    if (!xcbErrCtx.isValid()) {
        Debug::log(ERR, "[xwm] Couldn't allocate errors context");
        return;
    }

    m_dndDataDevice->m_self = m_dndDataDevice;

    xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(xcb_get_setup(getConnection()));
    m_screen                              = screen_iterator.data;

    m_eventSource = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, g_pXWayland->m_server->m_xwmFDs[0].get(), WL_EVENT_READABLE, ::onX11Event, nullptr);

    gatherResources();
    getVisual();
    getRenderFormat();

    uint32_t values[] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_PROPERTY_CHANGE,
    };
    xcb_change_window_attributes(getConnection(), m_screen->root, XCB_CW_EVENT_MASK, values);

    xcb_composite_redirect_subwindows(getConnection(), m_screen->root, XCB_COMPOSITE_REDIRECT_MANUAL);

    xcb_atom_t supported[] = {
        HYPRATOMS["_NET_WM_STATE"],        HYPRATOMS["_NET_ACTIVE_WINDOW"],       HYPRATOMS["_NET_WM_MOVERESIZE"],           HYPRATOMS["_NET_WM_STATE_FOCUSED"],
        HYPRATOMS["_NET_WM_STATE_MODAL"],  HYPRATOMS["_NET_WM_STATE_FULLSCREEN"], HYPRATOMS["_NET_WM_STATE_MAXIMIZED_VERT"], HYPRATOMS["_NET_WM_STATE_MAXIMIZED_HORZ"],
        HYPRATOMS["_NET_WM_STATE_HIDDEN"], HYPRATOMS["_NET_CLIENT_LIST"],         HYPRATOMS["_NET_CLIENT_LIST_STACKING"],
    };
    xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, m_screen->root, HYPRATOMS["_NET_SUPPORTED"], XCB_ATOM_ATOM, 32, sizeof(supported) / sizeof(*supported), supported);

    setActiveWindow(XCB_WINDOW_NONE);
    initSelection();

    m_listeners.newWLSurface     = PROTO::compositor->m_events.newSurface.listen([this](const auto& surface) { onNewSurface(surface); });
    m_listeners.newXShellSurface = PROTO::xwaylandShell->m_events.newSurface.listen([this](const auto& surface) { onNewResource(surface); });

    createWMWindow();

    xcb_flush(getConnection());
}

CXWM::~CXWM() {

    if (m_eventSource)
        wl_event_source_remove(m_eventSource);

    for (auto const& sr : m_surfaces) {
        sr->m_events.destroy.emit();
    }
}

void CXWM::setActiveWindow(xcb_window_t window) {
    xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, m_screen->root, HYPRATOMS["_NET_ACTIVE_WINDOW"], HYPRATOMS["WINDOW"], 32, 1, &window);
}

void CXWM::createWMWindow() {
    constexpr const char* wmName = "Hyprland :D";
    m_wmWindow                   = xcb_generate_id(getConnection());
    xcb_create_window(getConnection(), XCB_COPY_FROM_PARENT, m_wmWindow, m_screen->root, 0, 0, 10, 10, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, m_screen->root_visual, 0, nullptr);
    xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, m_wmWindow, HYPRATOMS["_NET_WM_NAME"], HYPRATOMS["UTF8_STRING"],
                        8, // format
                        strlen(wmName), wmName);
    xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, m_screen->root, HYPRATOMS["_NET_SUPPORTING_WM_CHECK"], XCB_ATOM_WINDOW,
                        32, // format
                        1, &m_wmWindow);
    xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, m_wmWindow, HYPRATOMS["_NET_SUPPORTING_WM_CHECK"], XCB_ATOM_WINDOW,
                        32, // format
                        1, &m_wmWindow);
    xcb_set_selection_owner(getConnection(), m_wmWindow, HYPRATOMS["WM_S0"], XCB_CURRENT_TIME);
    xcb_set_selection_owner(getConnection(), m_wmWindow, HYPRATOMS["_NET_WM_CM_S0"], XCB_CURRENT_TIME);
}

void CXWM::activateSurface(SP<CXWaylandSurface> surf, bool activate) {
    if ((surf == m_focusedSurface && activate) || (surf && surf->m_overrideRedirect))
        return;

    if (!surf || (!activate && g_pCompositor->m_lastWindow && !g_pCompositor->m_lastWindow->m_isX11)) {
        setActiveWindow(XCB_WINDOW_NONE);
        focusWindow(nullptr);
    } else {
        setActiveWindow(surf ? surf->m_xID : sc<uint32_t>(XCB_WINDOW_NONE));
        focusWindow(surf);
    }

    xcb_flush(getConnection());
}

void CXWM::sendState(SP<CXWaylandSurface> surf) {
    Debug::log(TRACE, "[xwm] sendState for {} {} {} surface {}", surf->m_mapped ? "mapped" : "unmapped", surf->m_fullscreen ? "fullscreen" : "windowed",
               surf == m_focusedSurface ? "focused" : "unfocused", surf->m_state.title);
    if (surf->m_fullscreen && surf->m_mapped && surf == m_focusedSurface)
        surf->setWithdrawn(false); // resend normal state

    if (surf->m_withdrawn) {
        xcb_delete_property(getConnection(), surf->m_xID, HYPRATOMS["_NET_WM_STATE"]);
        return;
    }

    std::vector<uint32_t> props;
    // reserve to avoid reallocations
    props.reserve(6); // props below
    if (surf->m_modal)
        props.push_back(HYPRATOMS["_NET_WM_STATE_MODAL"]);
    if (surf->m_fullscreen)
        props.push_back(HYPRATOMS["_NET_WM_STATE_FULLSCREEN"]);
    if (surf->m_maximized) {
        props.push_back(HYPRATOMS["NET_WM_STATE_MAXIMIZED_VERT"]);
        props.push_back(HYPRATOMS["NET_WM_STATE_MAXIMIZED_HORZ"]);
    }
    if (surf->m_minimized)
        props.push_back(HYPRATOMS["_NET_WM_STATE_HIDDEN"]);
    if (surf == m_focusedSurface)
        props.push_back(HYPRATOMS["_NET_WM_STATE_FOCUSED"]);

    xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, surf->m_xID, HYPRATOMS["_NET_WM_STATE"], XCB_ATOM_ATOM, 32, props.size(), props.data());
}

void CXWM::onNewSurface(SP<CWLSurfaceResource> surf) {
    if (surf->client() != g_pXWayland->m_server->m_xwaylandClient)
        return;

    Debug::log(LOG, "[xwm] New XWayland surface at {:x}", rc<uintptr_t>(surf.get()));

    const auto WLID = surf->id();

    for (auto const& sr : m_surfaces) {
        if (sr->m_surface || sr->m_wlID != WLID)
            continue;

        associate(sr, surf);
        return;
    }

    Debug::log(WARN, "[xwm] CXWM::onNewSurface: no matching xwaylandSurface");
}

void CXWM::onNewResource(SP<CXWaylandSurfaceResource> resource) {
    Debug::log(LOG, "[xwm] New XWayland resource at {:x}", rc<uintptr_t>(resource.get()));

    std::erase_if(m_shellResources, [](const auto& e) { return e.expired(); });
    m_shellResources.emplace_back(resource);

    for (auto const& surf : m_surfaces) {
        if (surf->m_resource || surf->m_wlSerial != resource->m_serial)
            continue;

        associate(surf, resource->m_surface.lock());
        break;
    }
}

void CXWM::readWindowData(SP<CXWaylandSurface> surf) {
    const std::array<xcb_atom_t, 9> interestingProps = {
        XCB_ATOM_WM_CLASS,          XCB_ATOM_WM_NAME,          XCB_ATOM_WM_TRANSIENT_FOR,        HYPRATOMS["WM_HINTS"],
        HYPRATOMS["_NET_WM_STATE"], HYPRATOMS["_NET_WM_NAME"], HYPRATOMS["_NET_WM_WINDOW_TYPE"], HYPRATOMS["WM_NORMAL_HINTS"],
        HYPRATOMS["WM_PROTOCOLS"],
    };

    for (size_t i = 0; i < interestingProps.size(); i++) {
        xcb_get_property_cookie_t             cookie = xcb_get_property(getConnection(), 0, surf->m_xID, interestingProps[i], XCB_ATOM_ANY, 0, 2048);
        XCBReplyPtr<xcb_get_property_reply_t> reply(xcb_get_property_reply(getConnection(), cookie, nullptr));
        if (!reply) {
            Debug::log(ERR, "[xwm] Failed to get window property");
            continue;
        }
        readProp(surf, interestingProps[i], reply.get());
    }
}

SP<CXWaylandSurface> CXWM::windowForWayland(SP<CWLSurfaceResource> surf) {
    for (auto& s : m_surfaces) {
        if (s->m_surface == surf)
            return s;
    }

    return nullptr;
}

void CXWM::associate(SP<CXWaylandSurface> surf, SP<CWLSurfaceResource> wlSurf) {
    if (surf->m_surface)
        return;

    auto existing = std::ranges::find_if(m_surfaces, [wlSurf](const auto& e) { return e->m_surface == wlSurf; });

    if (existing != m_surfaces.end()) {
        Debug::log(WARN, "[xwm] associate() called but surface is already associated to {:x}, ignoring...", rc<uintptr_t>(surf.get()));
        return;
    }

    surf->m_surface = wlSurf;
    surf->ensureListeners();

    readWindowData(surf);

    surf->m_events.resourceChange.emit();
}

void CXWM::dissociate(SP<CXWaylandSurface> surf) {
    if (!surf->m_surface)
        return;

    if (surf->m_mapped)
        surf->unmap();

    surf->m_surface.reset();
    surf->m_events.resourceChange.emit();

    Debug::log(LOG, "Dissociate for {:x}", rc<uintptr_t>(surf.get()));
}

void CXWM::updateClientList() {
    std::vector<xcb_window_t> windows;
    windows.reserve(m_mappedSurfaces.size());

    for (auto const& s : m_mappedSurfaces) {
        if (auto surf = s.lock(); surf)
            windows.push_back(surf->m_xID);
    }

    xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, m_screen->root, HYPRATOMS["_NET_CLIENT_LIST"], XCB_ATOM_WINDOW, 32, windows.size(), windows.data());

    windows.clear();
    windows.reserve(m_mappedSurfacesStacking.size());

    for (auto const& s : m_mappedSurfacesStacking) {
        if (auto surf = s.lock(); surf)
            windows.push_back(surf->m_xID);
    }

    xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, m_screen->root, HYPRATOMS["_NET_CLIENT_LIST_STACKING"], XCB_ATOM_WINDOW, 32, windows.size(), windows.data());
}

bool CXWM::isWMWindow(xcb_window_t w) {
    return w == m_wmWindow || w == m_clipboard.window || w == m_dndSelection.window;
}

void CXWM::updateOverrideRedirect(SP<CXWaylandSurface> surf, bool overrideRedirect) {
    if (!surf || surf->m_overrideRedirect == overrideRedirect)
        return;

    surf->m_overrideRedirect = overrideRedirect;
}

void CXWM::initSelection() {
    const uint32_t windowMask = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    const uint32_t xfixesMask =
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE;

    auto createSelectionWindow = [&](xcb_window_t& window, const std::string& atomName, bool inputOnly = false) {
        window                = xcb_generate_id(getConnection());
        const uint16_t width  = inputOnly ? 8192 : 10;
        const uint16_t height = inputOnly ? 8192 : 10;

        xcb_create_window(getConnection(), XCB_COPY_FROM_PARENT, window, m_screen->root, 0, 0, width, height, 0,
                          inputOnly ? XCB_WINDOW_CLASS_INPUT_ONLY : XCB_WINDOW_CLASS_INPUT_OUTPUT, m_screen->root_visual, XCB_CW_EVENT_MASK, &windowMask);

        if (!inputOnly) {
            xcb_set_selection_owner(getConnection(), window, HYPRATOMS[atomName], XCB_TIME_CURRENT_TIME);
            xcb_xfixes_select_selection_input(getConnection(), window, HYPRATOMS[atomName], xfixesMask);
        }

        return window;
    };

    createSelectionWindow(m_clipboard.window, "CLIPBOARD_MANAGER");
    createSelectionWindow(m_clipboard.window, "CLIPBOARD");
    m_clipboard.listeners.setSelection        = g_pSeatManager->m_events.setSelection.listen([this] { m_clipboard.onSelection(); });
    m_clipboard.listeners.keyboardFocusChange = g_pSeatManager->m_events.keyboardFocusChange.listen([this] { m_clipboard.onKeyboardFocus(); });

    createSelectionWindow(m_primarySelection.window, "PRIMARY");
    m_primarySelection.listeners.setSelection        = g_pSeatManager->m_events.setPrimarySelection.listen([this] { m_primarySelection.onSelection(); });
    m_primarySelection.listeners.keyboardFocusChange = g_pSeatManager->m_events.keyboardFocusChange.listen([this] { m_primarySelection.onKeyboardFocus(); });

    createSelectionWindow(m_dndSelection.window, "XdndAware", true);
    const uint32_t xdndVersion = XDND_VERSION;
    xcb_change_property(getConnection(), XCB_PROP_MODE_REPLACE, m_dndSelection.window, HYPRATOMS["XdndAware"], XCB_ATOM_ATOM, 32, 1, &xdndVersion);
}

void CXWM::setClipboardToWayland(SXSelection& sel) {
    auto source = makeShared<CXDataSource>(sel);
    if (source->mimes().empty()) {
        Debug::log(ERR, "[xwm] can't set selection: no MIMEs");
        return;
    }

    sel.dataSource = source;

    Debug::log(LOG, "[xwm] X selection at {:x} takes {}", rc<uintptr_t>(sel.dataSource.get()), (&sel == &m_clipboard) ? "clipboard" : "primary selection");

    if (&sel == &m_clipboard)
        g_pSeatManager->setCurrentSelection(sel.dataSource);
    else if (&sel == &m_primarySelection)
        g_pSeatManager->setCurrentPrimarySelection(sel.dataSource);
}

static int writeDataSource(int fd, uint32_t mask, void* data) {
    auto selection = sc<SXSelection*>(data);
    return selection->onWrite();
}

void CXWM::getTransferData(SXSelection& sel) {
    Debug::log(LOG, "[xwm] getTransferData");

    auto it = std::ranges::find_if(sel.transfers, [](const auto& t) { return !t->propertyReply; });
    if (it == sel.transfers.end()) {
        Debug::log(ERR, "[xwm] No pending transfer found");
        return;
    }

    auto& transfer = *it;
    if (!transfer || !transfer->incomingWindow) {
        Debug::log(ERR, "[xwm] Invalid transfer state");
        sel.transfers.erase(it);
        return;
    }

    if (!transfer->getIncomingSelectionProp(true)) {
        Debug::log(ERR, "[xwm] Failed to get property data");
        sel.transfers.erase(it);
        return;
    }

    if (!transfer->propertyReply) {
        Debug::log(ERR, "[xwm] No property reply");
        sel.transfers.erase(it);
        return;
    }

    if (transfer->propertyReply->type == HYPRATOMS["INCR"]) {
        transfer->incremental   = true;
        transfer->propertyStart = 0;
        free(transfer->propertyReply); // NOLINT(cppcoreguidelines-no-malloc)
        transfer->propertyReply = nullptr;
        return;
    }

    const size_t transferIndex = std::distance(sel.transfers.begin(), it);
    int          writeResult   = sel.onWrite();

    if (writeResult != 1)
        return;

    if (transferIndex >= sel.transfers.size())
        return;

    Hyprutils::Memory::CUniquePointer<SXTransfer>& updatedTransfer = sel.transfers[transferIndex];
    if (!updatedTransfer)
        return;

    if (updatedTransfer->eventSource && updatedTransfer->wlFD.get() == -1)
        return;

    updatedTransfer->eventSource = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, updatedTransfer->wlFD.get(), WL_EVENT_WRITABLE, ::writeDataSource, &sel);
}

void CXWM::setCursor(unsigned char* pixData, uint32_t stride, const Vector2D& size, const Vector2D& hotspot) {
    if (!m_renderFormatID) {
        Debug::log(ERR, "[xwm] can't set cursor: no render format");
        return;
    }

    if (m_cursorXID)
        xcb_free_cursor(getConnection(), m_cursorXID);

    constexpr int CURSOR_DEPTH = 32;

    xcb_pixmap_t  pix = xcb_generate_id(getConnection());
    xcb_create_pixmap(getConnection(), CURSOR_DEPTH, pix, m_screen->root, size.x, size.y);

    xcb_render_picture_t pic = xcb_generate_id(getConnection());
    xcb_render_create_picture(getConnection(), pic, pix, m_renderFormatID, 0, nullptr);

    xcb_gcontext_t gc = xcb_generate_id(getConnection());
    xcb_create_gc(getConnection(), gc, pix, 0, nullptr);

    xcb_put_image(getConnection(), XCB_IMAGE_FORMAT_Z_PIXMAP, pix, gc, size.x, size.y, 0, 0, 0, CURSOR_DEPTH, stride * size.y * sizeof(uint8_t), pixData);
    xcb_free_gc(getConnection(), gc);

    m_cursorXID = xcb_generate_id(getConnection());
    xcb_render_create_cursor(getConnection(), m_cursorXID, pic, hotspot.x, hotspot.y);
    xcb_free_pixmap(getConnection(), pix);
    xcb_render_free_picture(getConnection(), pic);

    uint32_t values[] = {m_cursorXID};
    xcb_change_window_attributes(getConnection(), m_screen->root, XCB_CW_CURSOR, values);
    xcb_flush(getConnection());
}

SP<CX11DataDevice> CXWM::getDataDevice() {
    return m_dndDataDevice;
}

SP<IDataOffer> CXWM::createX11DataOffer(SP<CWLSurfaceResource> surf, SP<IDataSource> source) {
    auto XSURF = windowForWayland(surf);

    if (!XSURF) {
        Debug::log(ERR, "[xwm] No xwayland surface for destination in createX11DataOffer");
        return nullptr;
    }

    // invalidate old
    g_pXWayland->m_wm->m_dndDataOffers.clear();

    auto offer               = m_dndDataOffers.emplace_back(makeShared<CX11DataOffer>());
    offer->m_self            = offer;
    offer->m_xwaylandSurface = XSURF;
    offer->m_source          = source;

    return offer;
}

void SXSelection::onSelection() {
    const bool isClipboard = this == &g_pXWayland->m_wm->m_clipboard;
    const bool isPrimary   = this == &g_pXWayland->m_wm->m_primarySelection;

    auto       currentSel     = g_pSeatManager->m_selection.currentSelection;
    auto       currentPrimSel = g_pSeatManager->m_selection.currentPrimarySelection;

    const bool isX11Clipboard = isClipboard && currentSel && currentSel->type() == DATA_SOURCE_TYPE_X11;
    const bool isX11Primary   = isPrimary && currentPrimSel && currentPrimSel->type() == DATA_SOURCE_TYPE_X11;

    if (isX11Clipboard || isX11Primary)
        return;

    auto conn = g_pXWayland->m_wm->getConnection();

    if (isClipboard && currentSel) {
        xcb_set_selection_owner(conn, g_pXWayland->m_wm->m_clipboard.window, HYPRATOMS["CLIPBOARD"], XCB_TIME_CURRENT_TIME);
        xcb_flush(conn);
        g_pXWayland->m_wm->m_clipboard.notifyOnFocus = true;
    } else if (isPrimary && currentPrimSel) {
        xcb_set_selection_owner(conn, g_pXWayland->m_wm->m_primarySelection.window, HYPRATOMS["PRIMARY"], XCB_TIME_CURRENT_TIME);
        xcb_flush(conn);
        g_pXWayland->m_wm->m_primarySelection.notifyOnFocus = true;
    }
}

void SXSelection::onKeyboardFocus() {
    if (!g_pSeatManager->m_state.keyboardFocusResource || g_pSeatManager->m_state.keyboardFocusResource->client() != g_pXWayland->m_server->m_xwaylandClient)
        return;

    if (this == &g_pXWayland->m_wm->m_clipboard && g_pXWayland->m_wm->m_clipboard.notifyOnFocus) {
        onSelection();
        g_pXWayland->m_wm->m_clipboard.notifyOnFocus = false;
    } else if (this == &g_pXWayland->m_wm->m_primarySelection && g_pXWayland->m_wm->m_primarySelection.notifyOnFocus) {
        onSelection();
        g_pXWayland->m_wm->m_primarySelection.notifyOnFocus = false;
    }
}

int SXSelection::onRead(int fd, uint32_t mask) {
    auto it = std::ranges::find_if(transfers, [fd](const auto& t) { return t->wlFD.get() == fd; });

    if (it == transfers.end()) {
        Debug::log(ERR, "[xwm] No transfer found for fd {}", fd);
        return 0;
    }

    auto&        transfer = *it;
    const size_t oldSize  = transfer->data.size();
    transfer->data.resize(oldSize + INCR_CHUNK_SIZE);

    ssize_t bytesRead = read(fd, transfer->data.data() + oldSize, INCR_CHUNK_SIZE - 1);

    if (bytesRead < 0) {
        Debug::log(ERR, "[xwm] readDataSource died");
        g_pXWayland->m_wm->selectionSendNotify(&transfer->request, false);
        transfers.erase(it);
        return 0;
    }

    transfer->data.resize(oldSize + bytesRead);

    if (bytesRead == 0) {
        if (transfer->data.empty()) {
            Debug::log(WARN, "[xwm] Transfer ended with zero bytes — rejecting");
            g_pXWayland->m_wm->selectionSendNotify(&transfer->request, false);
            transfers.erase(it);
            return 0;
        }

        Debug::log(LOG, "[xwm] Transfer complete, total size: {}", transfer->data.size());
        auto conn = g_pXWayland->m_wm->getConnection();
        xcb_change_property(conn, XCB_PROP_MODE_REPLACE, transfer->request.requestor, transfer->request.property, transfer->request.target, 8, transfer->data.size(),
                            transfer->data.data());

        xcb_flush(conn);
        g_pXWayland->m_wm->selectionSendNotify(&transfer->request, true);
        transfers.erase(it);
    } else
        Debug::log(LOG, "[xwm] Received {} bytes, awaiting more...", bytesRead);

    return 1;
}

static int readDataSource(int fd, uint32_t mask, void* data) {
    Debug::log(LOG, "[xwm] readDataSource on fd {}", fd);

    auto selection = sc<SXSelection*>(data);

    return selection->onRead(fd, mask);
}

bool SXSelection::sendData(xcb_selection_request_event_t* e, std::string mime) {
    WP<IDataSource> selection;
    if (this == &g_pXWayland->m_wm->m_clipboard)
        selection = g_pSeatManager->m_selection.currentSelection;
    else if (this == &g_pXWayland->m_wm->m_primarySelection)
        selection = g_pSeatManager->m_selection.currentPrimarySelection;
    else if (!g_pXWayland->m_wm->m_dndDataOffers.empty())
        selection = g_pXWayland->m_wm->m_dndDataOffers.at(0)->getSource();

    if (!selection) {
        Debug::log(ERR, "[xwm] sendData: no selection source available");
        return false;
    }

    const auto MIMES = selection->mimes();

    if (MIMES.empty()) {
        Debug::log(ERR, "[xwm] sendData: selection source has no mimes");
        return false;
    }

    if (std::ranges::find(MIMES, mime) == MIMES.end()) {
        // try to guess mime, don't just blindly send random-ass shit that the app will have no fucking
        // clue what to do with
        Debug::log(ERR, "[xwm] X client asked for MIME '{}' that this selection doesn't support, guessing.", mime);

        auto needle       = mime;
        auto selectedMime = *MIMES.begin();
        if (mime.contains('/'))
            needle = mime.substr(0, mime.find('/'));

        Debug::log(TRACE, "[xwm] X MIME needle '{}'", needle);

        if (Debug::m_trace) {
            std::string mimeList = "";
            for (const auto& m : MIMES) {
                mimeList += "'" + m + "', ";
            }

            if (!MIMES.empty())
                mimeList = mimeList.substr(0, mimeList.size() - 2);

            Debug::log(TRACE, "[xwm] X MIME supported: {}", mimeList);
        }

        bool found = false;

        for (const auto& m : MIMES) {
            if (m.starts_with(needle)) {
                selectedMime = m;
                Debug::log(TRACE, "[xwm] X MIME needle found type '{}'", m);
                found = true;
                break;
            }
        }

        if (!found) {
            for (const auto& m : MIMES) {
                if (m.contains(needle)) {
                    selectedMime = m;
                    Debug::log(TRACE, "[xwm] X MIME needle found type '{}'", m);
                    found = true;
                    break;
                }
            }
        }

        Debug::log(ERR, "[xwm] Guessed mime: '{}'. Hopefully we're right enough.", selectedMime);

        mime = selectedMime;
    }

    auto transfer     = makeUnique<SXTransfer>(*this);
    transfer->request = *e;

    int p[2];
    if (pipe(p) == -1) {
        Debug::log(ERR, "[xwm] sendData: pipe() failed");
        return false;
    }

    fcntl(p[0], F_SETFD, FD_CLOEXEC);
    fcntl(p[0], F_SETFL, O_NONBLOCK);
    fcntl(p[1], F_SETFD, FD_CLOEXEC);
    // Do NOT set O_NONBLOCK on p[1] (wayland clients may block)

    transfer->wlFD = CFileDescriptor{p[0]};

    Debug::log(LOG, "[xwm] sending wayland selection to xwayland with mime {}, target {}, fds {} {}", mime, e->target, p[0], p[1]);

    selection->send(mime, CFileDescriptor{p[1]});

    transfer->eventSource = wl_event_loop_add_fd(g_pCompositor->m_wlEventLoop, transfer->wlFD.get(), WL_EVENT_READABLE, ::readDataSource, this);

    transfers.emplace_back(std::move(transfer));
    return true;
}

int SXSelection::onWrite() {
    auto it = std::ranges::find_if(transfers, [](const auto& t) { return t->propertyReply; });
    if (it == transfers.end()) {
        Debug::log(ERR, "[xwm] No transfer with property data found");
        return 0;
    }

    auto&   transfer  = *it;
    char*   property  = sc<char*>(xcb_get_property_value(transfer->propertyReply));
    int     remainder = xcb_get_property_value_length(transfer->propertyReply) - transfer->propertyStart;

    ssize_t len = write(transfer->wlFD.get(), property + transfer->propertyStart, remainder);
    if (len == -1) {
        if (errno == EAGAIN)
            return 1;
        Debug::log(ERR, "[xwm] write died in transfer get");
        transfers.erase(it);
        return 0;
    }

    if (len < remainder) {
        transfer->propertyStart += len;
        Debug::log(LOG, "[xwm] wl client read partially: len {}", len);
    } else {
        Debug::log(LOG, "[xwm] cb transfer to wl client complete, read {} bytes", len);
        if (!transfer->incremental) {
            transfers.erase(it);
        } else {
            free(transfer->propertyReply); // NOLINT(cppcoreguidelines-no-malloc)
            transfer->propertyReply = nullptr;
            transfer->propertyStart = 0;
        }
    }

    return 1;
}

SXTransfer::~SXTransfer() {
    if (eventSource)
        wl_event_source_remove(eventSource);
    if (incomingWindow)
        xcb_destroy_window(*g_pXWayland->m_wm->m_connection, incomingWindow);
    if (propertyReply)
        free(propertyReply); // NOLINT(cppcoreguidelines-no-malloc)
}

bool SXTransfer::getIncomingSelectionProp(bool erase) {
    xcb_get_property_cookie_t cookie =
        xcb_get_property(*g_pXWayland->m_wm->m_connection, erase, incomingWindow, HYPRATOMS["_WL_SELECTION"], XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff);

    propertyStart = 0;
    propertyReply = xcb_get_property_reply(*g_pXWayland->m_wm->m_connection, cookie, nullptr);

    if (!propertyReply) {
        Debug::log(ERR, "[SXTransfer] couldn't get a prop reply");
        return false;
    }

    return true;
}

#endif
