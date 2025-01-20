#include "helpers/math/Math.hpp"
#include <cstdint>
#ifndef NO_XWAYLAND

#include <ranges>
#include <fcntl.h>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <xcb/xcb_icccm.h>

#include "XWayland.hpp"
#include "../defines.hpp"
#include "../Compositor.hpp"
#include "../protocols/core/Seat.hpp"
#include "../managers/eventLoop/EventLoopManager.hpp"
#include "../managers/SeatManager.hpp"
#include "../protocols/XWaylandShell.hpp"
#include "../protocols/core/Compositor.hpp"

#define XCB_EVENT_RESPONSE_TYPE_MASK 0x7f
#define INCR_CHUNK_SIZE              (64 * 1024)

static int onX11Event(int fd, uint32_t mask, void* data) {
    return g_pXWayland->pWM->onEvent(fd, mask);
}

SP<CXWaylandSurface> CXWM::windowForXID(xcb_window_t wid) {
    for (auto const& s : surfaces) {
        if (s->xID == wid)
            return s;
    }

    return nullptr;
}

void CXWM::handleCreate(xcb_create_notify_event_t* e) {
    if (isWMWindow(e->window))
        return;

    const auto XSURF = surfaces.emplace_back(SP<CXWaylandSurface>(new CXWaylandSurface(e->window, CBox{e->x, e->y, e->width, e->height}, e->override_redirect)));
    XSURF->self      = XSURF;
    Debug::log(LOG, "[xwm] New XSurface at {:x} with xid of {}", (uintptr_t)XSURF.get(), e->window);

    const auto WINDOW = CWindow::create(XSURF);
    g_pCompositor->m_vWindows.emplace_back(WINDOW);
    WINDOW->m_pSelf = WINDOW;
    Debug::log(LOG, "[xwm] New XWayland window at {:x} for surf {:x}", (uintptr_t)WINDOW.get(), (uintptr_t)XSURF.get());
}

void CXWM::handleDestroy(xcb_destroy_notify_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    XSURF->events.destroy.emit();
    std::erase_if(surfaces, [XSURF](const auto& other) { return XSURF == other; });
}

void CXWM::handleConfigure(xcb_configure_request_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    const uint16_t     MASK     = e->value_mask;
    constexpr uint16_t GEOMETRY = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
    if (!(MASK & GEOMETRY))
        return;

    XSURF->events.configure.emit(CBox{MASK & XCB_CONFIG_WINDOW_X ? e->x : XSURF->geometry.x, MASK & XCB_CONFIG_WINDOW_Y ? e->y : XSURF->geometry.y,
                                      MASK & XCB_CONFIG_WINDOW_WIDTH ? e->width : XSURF->geometry.width, MASK & XCB_CONFIG_WINDOW_HEIGHT ? e->height : XSURF->geometry.height});
}

void CXWM::handleConfigureNotify(xcb_configure_notify_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    if (XSURF->geometry == CBox{e->x, e->y, e->width, e->height})
        return;

    XSURF->geometry = {e->x, e->y, e->width, e->height};
    updateOverrideRedirect(XSURF, e->override_redirect);
    XSURF->events.setGeometry.emit();
}

void CXWM::handleMapRequest(xcb_map_request_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    xcb_map_window(connection, e->window);

    XSURF->restackToTop();

    const bool SMALL =
        XSURF->geometry.size() < Vector2D{2, 2} || (XSURF->sizeHints && XSURF->geometry.size() < Vector2D{XSURF->sizeHints->min_width, XSURF->sizeHints->min_height});
    const bool HAS_HINTS   = XSURF->sizeHints && Vector2D{XSURF->sizeHints->base_width, XSURF->sizeHints->base_height} > Vector2D{5, 5};
    const auto DESIREDSIZE = HAS_HINTS ? Vector2D{XSURF->sizeHints->base_width, XSURF->sizeHints->base_height} : Vector2D{800, 800};

    // if it's too small, or its base size is set, configure it.
    if ((SMALL || HAS_HINTS) && !XSURF->overrideRedirect) // default to 800 x 800
        XSURF->configure({XSURF->geometry.pos(), DESIREDSIZE});

    Debug::log(LOG, "[xwm] Mapping window {} in X (geometry {}x{} at {}x{}))", e->window, XSURF->geometry.width, XSURF->geometry.height, XSURF->geometry.x, XSURF->geometry.y);

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

    if (XSURF->overrideRedirect)
        return;

    XSURF->setWithdrawn(false);
    sendState(XSURF);
    xcb_flush(connection);

    XSURF->considerMap();
}

void CXWM::handleUnmapNotify(xcb_unmap_notify_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    XSURF->unmap();
    dissociate(XSURF);

    if (XSURF->overrideRedirect)
        return;

    XSURF->setWithdrawn(true);
    sendState(XSURF);
    xcb_flush(connection);
}

static bool lookupParentExists(SP<CXWaylandSurface> XSURF, SP<CXWaylandSurface> prospectiveChild) {
    std::vector<SP<CXWaylandSurface>> visited;

    while (XSURF->parent) {
        if (XSURF->parent == prospectiveChild)
            return true;
        visited.emplace_back(XSURF);

        XSURF = XSURF->parent.lock();

        if (std::find(visited.begin(), visited.end(), XSURF) != visited.end())
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
    auto const atom_name_cookie = xcb_get_atom_name(connection, atom);
    auto*      atom_name_reply  = xcb_get_atom_name_reply(connection, atom_name_cookie, nullptr);

    if (!atom_name_reply)
        return "Unknown";

    auto const name_len = xcb_get_atom_name_name_length(atom_name_reply);
    auto*      name     = xcb_get_atom_name_name(atom_name_reply);
    free(atom_name_reply);

    return {name, name_len};
}

void CXWM::readProp(SP<CXWaylandSurface> XSURF, uint32_t atom, xcb_get_property_reply_t* reply) {
    std::string propName;
    if (Debug::trace)
        propName = getAtomName(atom);

    if (atom == XCB_ATOM_WM_CLASS) {
        size_t len         = xcb_get_property_value_length(reply);
        char*  string      = (char*)xcb_get_property_value(reply);
        XSURF->state.appid = std::string{string, len};
        if (std::count(XSURF->state.appid.begin(), XSURF->state.appid.end(), '\000') == 2)
            XSURF->state.appid = XSURF->state.appid.substr(XSURF->state.appid.find_first_of('\000') + 1); // fuck you X
        if (!XSURF->state.appid.empty())
            XSURF->state.appid.pop_back();
        XSURF->events.metadataChanged.emit();
    } else if (atom == XCB_ATOM_WM_NAME || atom == HYPRATOMS["_NET_WM_NAME"]) {
        size_t len    = xcb_get_property_value_length(reply);
        char*  string = (char*)xcb_get_property_value(reply);
        if (reply->type != HYPRATOMS["UTF8_STRING"] && reply->type != HYPRATOMS["TEXT"] && reply->type != XCB_ATOM_STRING)
            return;
        XSURF->state.title = std::string{string, len};
        XSURF->events.metadataChanged.emit();
    } else if (atom == HYPRATOMS["_NET_WM_WINDOW_TYPE"]) {
        xcb_atom_t* atomsArr = (xcb_atom_t*)xcb_get_property_value(reply);
        size_t      atomsNo  = reply->value_len;
        XSURF->atoms.clear();
        for (size_t i = 0; i < atomsNo; ++i) {
            XSURF->atoms.push_back(atomsArr[i]);
        }
    } else if (atom == HYPRATOMS["_NET_WM_STATE"]) {
        xcb_atom_t* atoms = (xcb_atom_t*)xcb_get_property_value(reply);
        for (uint32_t i = 0; i < reply->value_len; i++) {
            if (atoms[i] == HYPRATOMS["_NET_WM_STATE_MODAL"])
                XSURF->modal = true;
        }
    } else if (atom == HYPRATOMS["WM_HINTS"]) {
        if (reply->value_len != 0) {
            XSURF->hints = std::make_unique<xcb_icccm_wm_hints_t>();
            xcb_icccm_get_wm_hints_from_reply(XSURF->hints.get(), reply);

            if (!(XSURF->hints->flags & XCB_ICCCM_WM_HINT_INPUT))
                XSURF->hints->input = true;
        }
    } else if (atom == HYPRATOMS["WM_WINDOW_ROLE"]) {
        size_t len = xcb_get_property_value_length(reply);

        if (len <= 0)
            XSURF->role = "";
        else {
            XSURF->role = std::string{(char*)xcb_get_property_value(reply), len};
            XSURF->role = XSURF->role.substr(0, XSURF->role.find_first_of('\000'));
        }
    } else if (atom == XCB_ATOM_WM_TRANSIENT_FOR) {
        if (reply->type == XCB_ATOM_WINDOW) {
            const auto XID   = (xcb_window_t*)xcb_get_property_value(reply);
            XSURF->transient = XID;
            if (XID) {
                if (const auto NEWXSURF = windowForXID(*XID); NEWXSURF && !lookupParentExists(XSURF, NEWXSURF)) {
                    XSURF->parent = NEWXSURF;
                    NEWXSURF->children.emplace_back(XSURF);
                } else
                    Debug::log(LOG, "[xwm] Denying transient because it would create a loop");
            }
        }
    } else if (atom == HYPRATOMS["WM_NORMAL_HINTS"]) {
        if (reply->type == HYPRATOMS["WM_SIZE_HINTS"] && reply->value_len > 0) {
            XSURF->sizeHints = std::make_unique<xcb_size_hints_t>();
            std::memset(XSURF->sizeHints.get(), 0, sizeof(xcb_size_hints_t));

            xcb_icccm_get_wm_size_hints_from_reply(XSURF->sizeHints.get(), reply);

            const int32_t FLAGS   = XSURF->sizeHints->flags;
            const bool    HASMIN  = (FLAGS & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE);
            const bool    HASBASE = (FLAGS & XCB_ICCCM_SIZE_HINT_BASE_SIZE);

            if (!HASMIN && !HASBASE) {
                XSURF->sizeHints->min_width   = -1;
                XSURF->sizeHints->min_height  = -1;
                XSURF->sizeHints->base_width  = -1;
                XSURF->sizeHints->base_height = -1;
            } else if (!HASBASE) {
                XSURF->sizeHints->base_width  = XSURF->sizeHints->min_width;
                XSURF->sizeHints->base_height = XSURF->sizeHints->min_height;
            } else if (!HASMIN) {
                XSURF->sizeHints->min_width  = XSURF->sizeHints->base_width;
                XSURF->sizeHints->min_height = XSURF->sizeHints->base_height;
            }

            if (!(FLAGS & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE)) {
                XSURF->sizeHints->max_width  = -1;
                XSURF->sizeHints->max_height = -1;
            }
        }
    } else {
        Debug::log(TRACE, "[xwm] Unhandled prop {} -> {}", atom, propName);
        return;
    }

    Debug::log(TRACE, "[xwm] Handled prop {} -> {}", atom, propName);
}

void CXWM::handlePropertyNotify(xcb_property_notify_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    xcb_get_property_cookie_t cookie = xcb_get_property(connection, 0, XSURF->xID, e->atom, XCB_ATOM_ANY, 0, 2048);
    xcb_get_property_reply_t* reply  = xcb_get_property_reply(connection, cookie, nullptr);
    if (!reply) {
        Debug::log(ERR, "[xwm] Failed to read property notify cookie");
        return;
    }

    readProp(XSURF, e->atom, reply);

    free(reply);
}

void CXWM::handleClientMessage(xcb_client_message_event_t* e) {
    const auto XSURF = windowForXID(e->window);

    if (!XSURF)
        return;

    std::string propName = "?";
    for (auto const& ha : HYPRATOMS) {
        if (ha.second != e->type)
            continue;

        propName = ha.first;
        break;
    }

    if (e->type == HYPRATOMS["WL_SURFACE_ID"]) {
        if (XSURF->surface) {
            Debug::log(WARN, "[xwm] Re-assignment of WL_SURFACE_ID");
            dissociate(XSURF);
        }

        auto id       = e->data.data32[0];
        auto resource = wl_client_get_object(g_pXWayland->pServer->xwaylandClient, id);
        if (resource) {
            auto surf = CWLSurfaceResource::fromResource(resource);
            associate(XSURF, surf);
        }
    } else if (e->type == HYPRATOMS["WL_SURFACE_SERIAL"]) {
        if (XSURF->wlSerial) {
            Debug::log(WARN, "[xwm] Re-assignment of WL_SURFACE_SERIAL");
            dissociate(XSURF);
        }

        uint32_t serialLow  = e->data.data32[0];
        uint32_t serialHigh = e->data.data32[1];
        XSURF->wlSerial     = ((uint64_t)serialHigh << 32) | serialLow;

        Debug::log(LOG, "[xwm] surface {:x} requests serial {:x}", (uintptr_t)XSURF.get(), XSURF->wlSerial);

        for (auto const& res : shellResources) {
            if (!res)
                continue;

            if (res->serial != XSURF->wlSerial || !XSURF->wlSerial)
                continue;

            associate(XSURF, res->surface.lock());
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
                    XSURF->state.requestsFullscreen = updateState(action, XSURF->fullscreen);
            }

            XSURF->events.stateChanged.emit();
        }
    } else if (e->type == HYPRATOMS["_NET_ACTIVE_WINDOW"]) {
        XSURF->events.activate.emit();
    } else if (e->type == HYPRATOMS["XdndStatus"]) {
        if (dndDataOffers.empty() || !dndDataOffers.at(0)->getSource()) {
            Debug::log(TRACE, "[xwm] Rejecting XdndStatus message: nothing to get");
            return;
        }

        xcb_client_message_data_t* data     = &e->data;
        const bool                 ACCEPTED = data->data32[1] & 1;

        if (ACCEPTED)
            dndDataOffers.at(0)->getSource()->accepted("");

        Debug::log(LOG, "[xwm] XdndStatus: accepted: {}");
    } else if (e->type == HYPRATOMS["XdndFinished"]) {
        if (dndDataOffers.empty() || !dndDataOffers.at(0)->getSource()) {
            Debug::log(TRACE, "[xwm] Rejecting XdndFinished message: nothing to get");
            return;
        }

        dndDataOffers.at(0)->getSource()->sendDndFinished();

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

    if (focusedSurface && focusedSurface->pid == XSURF->pid && e->sequence - lastFocusSeq <= 255)
        focusWindow(XSURF);
    else
        focusWindow(focusedSurface.lock());
}

void CXWM::handleFocusOut(xcb_focus_out_event_t* e) {
    Debug::log(TRACE, "[xwm] focusOut mode={}, detail={}, event={}", e->mode, e->detail, e->event);

    const auto XSURF = windowForXID(e->event);

    if (!XSURF)
        return;

    Debug::log(TRACE, "[xwm] focusOut for {} {} {} surface {}", XSURF->mapped ? "mapped" : "unmapped", XSURF->fullscreen ? "fullscreen" : "windowed",
               XSURF == focusedSurface ? "focused" : "unfocused", XSURF->state.title);

    // do something?
}

void CXWM::sendWMMessage(SP<CXWaylandSurface> surf, xcb_client_message_data_t* data, uint32_t mask) {
    xcb_client_message_event_t event = {
        .response_type = XCB_CLIENT_MESSAGE,
        .format        = 32,
        .sequence      = 0,
        .window        = surf->xID,
        .type          = HYPRATOMS["WM_PROTOCOLS"],
        .data          = *data,
    };

    xcb_send_event(connection, 0, surf->xID, mask, (const char*)&event);
    xcb_flush(connection);
}

void CXWM::focusWindow(SP<CXWaylandSurface> surf) {
    if (surf == focusedSurface)
        return;

    focusedSurface = surf;

    // send state to all toplevel surfaces, sometimes we might lose some
    // that could still stick with the focused atom
    for (auto const& s : mappedSurfaces) {
        if (!s || s->overrideRedirect)
            continue;

        sendState(s.lock());
    }

    if (!surf) {
        xcb_set_input_focus_checked(connection, XCB_INPUT_FOCUS_POINTER_ROOT, XCB_NONE, XCB_CURRENT_TIME);
        return;
    }

    if (surf->overrideRedirect)
        return;

    xcb_client_message_data_t msg = {0};
    msg.data32[0]                 = HYPRATOMS["WM_TAKE_FOCUS"];
    msg.data32[1]                 = XCB_TIME_CURRENT_TIME;

    if (surf->hints && !surf->hints->input)
        sendWMMessage(surf, &msg, XCB_EVENT_MASK_NO_EVENT);
    else {
        sendWMMessage(surf, &msg, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT);

        xcb_void_cookie_t cookie = xcb_set_input_focus(connection, XCB_INPUT_FOCUS_POINTER_ROOT, surf->xID, XCB_CURRENT_TIME);
        lastFocusSeq             = cookie.sequence;
    }
}

void CXWM::handleError(xcb_value_error_t* e) {
    const char* major_name = xcb_errors_get_name_for_major_code(errors, e->major_opcode);
    if (!major_name) {
        Debug::log(ERR, "xcb error happened, but could not get major name");
        return;
    }

    const char* minor_name = xcb_errors_get_name_for_minor_code(errors, e->major_opcode, e->minor_opcode);

    const char* extension;
    const char* error_name = xcb_errors_get_name_for_error(errors, e->error_code, &extension);
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
        .property      = success ? e->property : (uint32_t)XCB_ATOM_NONE,
    };

    xcb_send_event(connection, 0, e->requestor, XCB_EVENT_MASK_NO_EVENT, (const char*)&selection_notify);
    xcb_flush(connection);
}

xcb_atom_t CXWM::mimeToAtom(const std::string& mime) {
    if (mime == "text/plain;charset=utf-8")
        return HYPRATOMS["UTF8_STRING"];
    if (mime == "text/plain")
        return HYPRATOMS["TEXT"];

    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, mime.length(), mime.c_str());
    xcb_intern_atom_reply_t* reply  = xcb_intern_atom_reply(connection, cookie, nullptr);
    if (!reply)
        return XCB_ATOM_NONE;
    xcb_atom_t atom = reply->atom;
    free(reply);
    return atom;
}

std::string CXWM::mimeFromAtom(xcb_atom_t atom) {
    if (atom == HYPRATOMS["UTF8_STRING"])
        return "text/plain;charset=utf-8";
    if (atom == HYPRATOMS["TEXT"])
        return "text/plain";

    xcb_get_atom_name_cookie_t cookie = xcb_get_atom_name(connection, atom);
    xcb_get_atom_name_reply_t* reply  = xcb_get_atom_name_reply(connection, cookie, nullptr);
    if (!reply)
        return "INVALID";
    size_t      len = xcb_get_atom_name_name_length(reply);
    char*       buf = xcb_get_atom_name_name(reply); // not a C string
    std::string SZNAME{buf, len};
    free(reply);
    return SZNAME;
}

void CXWM::handleSelectionNotify(xcb_selection_notify_event_t* e) {
    Debug::log(TRACE, "[xwm] Selection notify for {} prop {} target {}", e->selection, e->property, e->target);

    SXSelection* sel = getSelection(e->selection);

    if (e->property == XCB_ATOM_NONE) {
        if (sel->transfer) {
            Debug::log(TRACE, "[xwm] converting selection failed");
            sel->transfer.reset();
        }
    } else if (e->target == HYPRATOMS["TARGETS"] && sel == &clipboard) {
        if (!focusedSurface) {
            Debug::log(TRACE, "[xwm] denying access to write to clipboard because no X client is in focus");
            return;
        }

        setClipboardToWayland(*sel);
    } else if (sel->transfer)
        getTransferData(*sel);
}

bool CXWM::handleSelectionPropertyNotify(xcb_property_notify_event_t* e) {
    // Debug::log(LOG, "[xwm] Selection property notify for {} target {}", e->atom, e->window);

    // Debug::log(ERR, "[xwm] FIXME: CXWM::handleSelectionPropertyNotify stub");

    return false;
}

SXSelection* CXWM::getSelection(xcb_atom_t atom) {
    if (atom == HYPRATOMS["CLIPBOARD"])
        return &clipboard;
    else if (atom == HYPRATOMS["XdndSelection"])
        return &dndSelection;

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

    if (!g_pSeatManager->state.keyboardFocusResource || g_pSeatManager->state.keyboardFocusResource->client() != g_pXWayland->pServer->xwaylandClient) {
        Debug::log(TRACE, "[xwm] Ignoring clipboard access: xwayland not in focus");
        selectionSendNotify(e, false);
        return;
    }

    if (e->target == HYPRATOMS["TARGETS"]) {
        // send mime types
        std::vector<std::string> mimes;
        if (sel == &clipboard && g_pSeatManager->selection.currentSelection)
            mimes = g_pSeatManager->selection.currentSelection->mimes();
        else if (sel == &dndSelection && !dndDataOffers.empty() && dndDataOffers.at(0)->source)
            mimes = dndDataOffers.at(0)->source->mimes();

        if (mimes.empty())
            Debug::log(WARN, "[xwm] WARNING: No mimes in TARGETS?");

        std::vector<xcb_atom_t> atoms;
        atoms.push_back(HYPRATOMS["TIMESTAMP"]);
        atoms.push_back(HYPRATOMS["TARGETS"]);

        for (auto const& m : mimes) {
            atoms.push_back(mimeToAtom(m));
        }

        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, e->requestor, e->property, XCB_ATOM_ATOM, 32, atoms.size(), atoms.data());
        selectionSendNotify(e, true);
    } else if (e->target == HYPRATOMS["TIMESTAMP"]) {
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, e->requestor, e->property, XCB_ATOM_INTEGER, 32, 1, &sel->timestamp);
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

    if (sel == &dndSelection)
        return true;

    if (e->owner == XCB_WINDOW_NONE) {
        if (sel->owner != sel->window && sel == &clipboard)
            g_pSeatManager->setCurrentSelection(nullptr);

        sel->owner = 0;
        return true;
    }

    sel->owner = e->owner;

    if (sel->owner == sel->window) {
        sel->timestamp = e->timestamp;
        return true;
    }

    xcb_convert_selection(connection, sel->window, HYPRATOMS["CLIPBOARD"], HYPRATOMS["TARGETS"], HYPRATOMS["_WL_SELECTION"], e->timestamp);
    xcb_flush(connection);

    return true;
}

bool CXWM::handleSelectionEvent(xcb_generic_event_t* e) {
    switch (e->response_type & XCB_EVENT_RESPONSE_TYPE_MASK) {
        case XCB_SELECTION_NOTIFY: {
            handleSelectionNotify((xcb_selection_notify_event_t*)e);
            return true;
        }
        case XCB_PROPERTY_NOTIFY: {
            return handleSelectionPropertyNotify((xcb_property_notify_event_t*)e);
        }
        case XCB_SELECTION_REQUEST: {
            handleSelectionRequest((xcb_selection_request_event_t*)e);
            return true;
        }
    }

    if (e->response_type - xfixes->first_event == XCB_XFIXES_SELECTION_NOTIFY)
        return handleSelectionXFixesNotify((xcb_xfixes_selection_notify_event_t*)e);

    return 0;
}

int CXWM::onEvent(int fd, uint32_t mask) {

    if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
        Debug::log(ERR, "XWayland has yeeten the xwm off?!");
        Debug::log(CRIT, "XWayland has yeeten the xwm off?!");
        g_pXWayland->pWM.reset();
        g_pXWayland->pServer.reset();
        // Attempt to create fresh instance
        g_pEventLoopManager->doLater([]() { g_pXWayland = std::make_unique<CXWayland>(true); });
        return 0;
    }

    int count = 0;

    while (42069) {
        xcb_generic_event_t* event = xcb_poll_for_event(connection);
        if (!event)
            break;

        count++;

        if (handleSelectionEvent(event)) {
            free(event);
            continue;
        }

        switch (event->response_type & XCB_EVENT_RESPONSE_TYPE_MASK) {
            case XCB_CREATE_NOTIFY: handleCreate((xcb_create_notify_event_t*)event); break;
            case XCB_DESTROY_NOTIFY: handleDestroy((xcb_destroy_notify_event_t*)event); break;
            case XCB_CONFIGURE_REQUEST: handleConfigure((xcb_configure_request_event_t*)event); break;
            case XCB_CONFIGURE_NOTIFY: handleConfigureNotify((xcb_configure_notify_event_t*)event); break;
            case XCB_MAP_REQUEST: handleMapRequest((xcb_map_request_event_t*)event); break;
            case XCB_MAP_NOTIFY: handleMapNotify((xcb_map_notify_event_t*)event); break;
            case XCB_UNMAP_NOTIFY: handleUnmapNotify((xcb_unmap_notify_event_t*)event); break;
            case XCB_PROPERTY_NOTIFY: handlePropertyNotify((xcb_property_notify_event_t*)event); break;
            case XCB_CLIENT_MESSAGE: handleClientMessage((xcb_client_message_event_t*)event); break;
            case XCB_FOCUS_IN: handleFocusIn((xcb_focus_in_event_t*)event); break;
            case XCB_FOCUS_OUT: handleFocusOut((xcb_focus_out_event_t*)event); break;
            case 0: handleError((xcb_value_error_t*)event); break;
            default: {
                Debug::log(TRACE, "[xwm] unhandled event {}", event->response_type & XCB_EVENT_RESPONSE_TYPE_MASK);
            }
        }
        free(event);
    }

    if (count)
        xcb_flush(connection);

    return count;
}

void CXWM::gatherResources() {
    xcb_prefetch_extension_data(connection, &xcb_xfixes_id);
    xcb_prefetch_extension_data(connection, &xcb_composite_id);
    xcb_prefetch_extension_data(connection, &xcb_res_id);

    for (auto& ATOM : HYPRATOMS) {
        xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 0, ATOM.first.length(), ATOM.first.c_str());
        xcb_intern_atom_reply_t* reply  = xcb_intern_atom_reply(connection, cookie, nullptr);

        if (!reply) {
            Debug::log(ERR, "[xwm] Atom failed: {}", ATOM.first);
            continue;
        }

        ATOM.second = reply->atom;
        free(reply);
    }

    xfixes = xcb_get_extension_data(connection, &xcb_xfixes_id);

    if (!xfixes || !xfixes->present)
        Debug::log(WARN, "XFixes not available");

    xcb_xfixes_query_version_cookie_t xfixes_cookie;
    xcb_xfixes_query_version_reply_t* xfixes_reply;
    xfixes_cookie = xcb_xfixes_query_version(connection, XCB_XFIXES_MAJOR_VERSION, XCB_XFIXES_MINOR_VERSION);
    xfixes_reply  = xcb_xfixes_query_version_reply(connection, xfixes_cookie, nullptr);

    Debug::log(LOG, "xfixes version: {}.{}", xfixes_reply->major_version, xfixes_reply->minor_version);
    xfixesMajor = xfixes_reply->major_version;

    free(xfixes_reply);

    const xcb_query_extension_reply_t* xresReply1 = xcb_get_extension_data(connection, &xcb_res_id);
    if (!xresReply1 || !xresReply1->present)
        return;

    xcb_res_query_version_cookie_t xres_cookie = xcb_res_query_version(connection, XCB_RES_MAJOR_VERSION, XCB_RES_MINOR_VERSION);
    xcb_res_query_version_reply_t* xres_reply  = xcb_res_query_version_reply(connection, xres_cookie, nullptr);
    if (xres_reply == nullptr)
        return;

    Debug::log(LOG, "xres version: {}.{}", xres_reply->server_major, xres_reply->server_minor);
    if (xres_reply->server_major > 1 || (xres_reply->server_major == 1 && xres_reply->server_minor >= 2)) {
        xres = xresReply1;
    }
    free(xres_reply);
}

void CXWM::getVisual() {
    xcb_depth_iterator_t      d_iter;
    xcb_visualtype_iterator_t vt_iter;
    xcb_visualtype_t*         visualtype;

    d_iter     = xcb_screen_allowed_depths_iterator(screen);
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

    visual_id = visualtype->visual_id;
    colormap  = xcb_generate_id(connection);
    xcb_create_colormap(connection, XCB_COLORMAP_ALLOC_NONE, colormap, screen->root, visual_id);
}

void CXWM::getRenderFormat() {
    xcb_render_query_pict_formats_cookie_t cookie = xcb_render_query_pict_formats(connection);
    xcb_render_query_pict_formats_reply_t* reply  = xcb_render_query_pict_formats_reply(connection, cookie, nullptr);
    if (!reply) {
        Debug::log(LOG, "xwm: No xcb_render_query_pict_formats_reply_t reply");
        return;
    }
    xcb_render_pictforminfo_iterator_t iter   = xcb_render_query_pict_formats_formats_iterator(reply);
    xcb_render_pictforminfo_t*         format = nullptr;
    while (iter.rem > 0) {
        if (iter.data->depth == 32) {
            format = iter.data;
            break;
        }

        xcb_render_pictforminfo_next(&iter);
    }

    if (format == nullptr) {
        Debug::log(LOG, "xwm: No 32-bit render format");
        free(reply);
        return;
    }

    render_format_id = format->id;
    free(reply);
}

CXWM::CXWM() : connection(g_pXWayland->pServer->xwmFDs[0]) {

    if (connection.hasError()) {
        Debug::log(ERR, "[xwm] Couldn't start, error {}", connection.hasError());
        return;
    }

    CXCBErrorContext xcbErrCtx(connection);
    if (!xcbErrCtx.isValid()) {
        Debug::log(ERR, "[xwm] Couldn't allocate errors context");
        return;
    }

    dndDataDevice->self = dndDataDevice;

    xcb_screen_iterator_t screen_iterator = xcb_setup_roots_iterator(xcb_get_setup(connection));
    screen                                = screen_iterator.data;

    eventSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, g_pXWayland->pServer->xwmFDs[0], WL_EVENT_READABLE, ::onX11Event, nullptr);
    wl_event_source_check(eventSource);

    gatherResources();
    getVisual();
    getRenderFormat();

    uint32_t values[] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_PROPERTY_CHANGE,
    };
    xcb_change_window_attributes(connection, screen->root, XCB_CW_EVENT_MASK, values);

    xcb_composite_redirect_subwindows(connection, screen->root, XCB_COMPOSITE_REDIRECT_MANUAL);

    xcb_atom_t supported[] = {
        HYPRATOMS["_NET_WM_STATE"],        HYPRATOMS["_NET_ACTIVE_WINDOW"],       HYPRATOMS["_NET_WM_MOVERESIZE"],           HYPRATOMS["_NET_WM_STATE_FOCUSED"],
        HYPRATOMS["_NET_WM_STATE_MODAL"],  HYPRATOMS["_NET_WM_STATE_FULLSCREEN"], HYPRATOMS["_NET_WM_STATE_MAXIMIZED_VERT"], HYPRATOMS["_NET_WM_STATE_MAXIMIZED_HORZ"],
        HYPRATOMS["_NET_WM_STATE_HIDDEN"], HYPRATOMS["_NET_CLIENT_LIST"],         HYPRATOMS["_NET_CLIENT_LIST_STACKING"],
    };
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root, HYPRATOMS["_NET_SUPPORTED"], XCB_ATOM_ATOM, 32, sizeof(supported) / sizeof(*supported), supported);

    setActiveWindow(XCB_WINDOW_NONE);
    initSelection();

    listeners.newWLSurface     = PROTO::compositor->events.newSurface.registerListener([this](std::any d) { onNewSurface(std::any_cast<SP<CWLSurfaceResource>>(d)); });
    listeners.newXShellSurface = PROTO::xwaylandShell->events.newSurface.registerListener([this](std::any d) { onNewResource(std::any_cast<SP<CXWaylandSurfaceResource>>(d)); });

    createWMWindow();

    xcb_flush(connection);
}

CXWM::~CXWM() {

    if (eventSource)
        wl_event_source_remove(eventSource);

    for (auto const& sr : surfaces) {
        sr->events.destroy.emit();
    }
}

void CXWM::setActiveWindow(xcb_window_t window) {
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root, HYPRATOMS["_NET_ACTIVE_WINDOW"], HYPRATOMS["WINDOW"], 32, 1, &window);
}

void CXWM::createWMWindow() {
    constexpr const char* wmName = "Hyprland :D";
    wmWindow                     = xcb_generate_id(connection);
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, wmWindow, screen->root, 0, 0, 10, 10, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, 0, nullptr);
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wmWindow, HYPRATOMS["_NET_WM_NAME"], HYPRATOMS["UTF8_STRING"],
                        8, // format
                        strlen(wmName), wmName);
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root, HYPRATOMS["_NET_SUPPORTING_WM_CHECK"], XCB_ATOM_WINDOW,
                        32, // format
                        1, &wmWindow);
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, wmWindow, HYPRATOMS["_NET_SUPPORTING_WM_CHECK"], XCB_ATOM_WINDOW,
                        32, // format
                        1, &wmWindow);
    xcb_set_selection_owner(connection, wmWindow, HYPRATOMS["WM_S0"], XCB_CURRENT_TIME);
    xcb_set_selection_owner(connection, wmWindow, HYPRATOMS["_NET_WM_CM_S0"], XCB_CURRENT_TIME);
}

void CXWM::activateSurface(SP<CXWaylandSurface> surf, bool activate) {
    if ((surf == focusedSurface && activate) || (surf && surf->overrideRedirect))
        return;

    if (!surf || (!activate && g_pCompositor->m_pLastWindow && !g_pCompositor->m_pLastWindow->m_bIsX11)) {
        setActiveWindow((uint32_t)XCB_WINDOW_NONE);
        focusWindow(nullptr);
    } else {
        setActiveWindow(surf ? surf->xID : (uint32_t)XCB_WINDOW_NONE);
        focusWindow(surf);
    }

    xcb_flush(connection);
}

void CXWM::sendState(SP<CXWaylandSurface> surf) {
    Debug::log(TRACE, "[xwm] sendState for {} {} {} surface {}", surf->mapped ? "mapped" : "unmapped", surf->fullscreen ? "fullscreen" : "windowed",
               surf == focusedSurface ? "focused" : "unfocused", surf->state.title);
    if (surf->fullscreen && surf->mapped && surf == focusedSurface)
        surf->setWithdrawn(false); // resend normal state

    if (surf->withdrawn) {
        xcb_delete_property(connection, surf->xID, HYPRATOMS["_NET_WM_STATE"]);
        return;
    }

    std::vector<uint32_t> props;
    if (surf->modal)
        props.push_back(HYPRATOMS["_NET_WM_STATE_MODAL"]);
    if (surf->fullscreen)
        props.push_back(HYPRATOMS["_NET_WM_STATE_FULLSCREEN"]);
    if (surf->maximized) {
        props.push_back(HYPRATOMS["NET_WM_STATE_MAXIMIZED_VERT"]);
        props.push_back(HYPRATOMS["NET_WM_STATE_MAXIMIZED_HORZ"]);
    }
    if (surf->minimized)
        props.push_back(HYPRATOMS["_NET_WM_STATE_HIDDEN"]);
    if (surf == focusedSurface)
        props.push_back(HYPRATOMS["_NET_WM_STATE_FOCUSED"]);

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, surf->xID, HYPRATOMS["_NET_WM_STATE"], XCB_ATOM_ATOM, 32, props.size(), props.data());
}

void CXWM::onNewSurface(SP<CWLSurfaceResource> surf) {
    if (surf->client() != g_pXWayland->pServer->xwaylandClient)
        return;

    Debug::log(LOG, "[xwm] New XWayland surface at {:x}", (uintptr_t)surf.get());

    const auto WLID = surf->id();

    for (auto const& sr : surfaces) {
        if (sr->surface || sr->wlID != WLID)
            continue;

        associate(sr, surf);
        return;
    }

    Debug::log(WARN, "[xwm] CXWM::onNewSurface: no matching xwaylandSurface");
}

void CXWM::onNewResource(SP<CXWaylandSurfaceResource> resource) {
    Debug::log(LOG, "[xwm] New XWayland resource at {:x}", (uintptr_t)resource.get());

    std::erase_if(shellResources, [](const auto& e) { return e.expired(); });
    shellResources.emplace_back(resource);

    for (auto const& surf : surfaces) {
        if (surf->resource || surf->wlSerial != resource->serial)
            continue;

        associate(surf, resource->surface.lock());
        break;
    }
}

void CXWM::readWindowData(SP<CXWaylandSurface> surf) {
    const std::array<xcb_atom_t, 8> interestingProps = {
        XCB_ATOM_WM_CLASS,          XCB_ATOM_WM_NAME,          XCB_ATOM_WM_TRANSIENT_FOR,        HYPRATOMS["WM_HINTS"],
        HYPRATOMS["_NET_WM_STATE"], HYPRATOMS["_NET_WM_NAME"], HYPRATOMS["_NET_WM_WINDOW_TYPE"], HYPRATOMS["WM_NORMAL_HINTS"],
    };

    for (size_t i = 0; i < interestingProps.size(); i++) {
        xcb_get_property_cookie_t cookie = xcb_get_property(connection, 0, surf->xID, interestingProps.at(i), XCB_ATOM_ANY, 0, 2048);
        xcb_get_property_reply_t* reply  = xcb_get_property_reply(connection, cookie, nullptr);
        if (!reply) {
            Debug::log(ERR, "[xwm] Failed to get window property");
            continue;
        }
        readProp(surf, interestingProps.at(i), reply);
        free(reply);
    }
}

SP<CXWaylandSurface> CXWM::windowForWayland(SP<CWLSurfaceResource> surf) {
    for (auto& s : surfaces) {
        if (s->surface == surf)
            return s;
    }

    return nullptr;
}

void CXWM::associate(SP<CXWaylandSurface> surf, SP<CWLSurfaceResource> wlSurf) {
    if (surf->surface)
        return;

    auto existing = std::find_if(surfaces.begin(), surfaces.end(), [wlSurf](const auto& e) { return e->surface == wlSurf; });

    if (existing != surfaces.end()) {
        Debug::log(WARN, "[xwm] associate() called but surface is already associated to {:x}, ignoring...", (uintptr_t)surf.get());
        return;
    }

    surf->surface = wlSurf;
    surf->ensureListeners();

    readWindowData(surf);

    surf->events.resourceChange.emit();
}

void CXWM::dissociate(SP<CXWaylandSurface> surf) {
    if (!surf->surface)
        return;

    if (surf->mapped)
        surf->unmap();

    surf->surface.reset();
    surf->events.resourceChange.emit();

    Debug::log(LOG, "Dissociate for {:x}", (uintptr_t)surf.get());
}

void CXWM::updateClientList() {
    std::erase_if(mappedSurfaces, [](const auto& e) { return e.expired() || !e->mapped; });
    std::erase_if(mappedSurfacesStacking, [](const auto& e) { return e.expired() || !e->mapped; });

    std::vector<xcb_window_t> windows;
    for (auto const& m : mappedSurfaces) {
        windows.push_back(m->xID);
    }

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root, HYPRATOMS["_NET_CLIENT_LIST"], XCB_ATOM_WINDOW, 32, windows.size(), windows.data());

    windows.clear();

    for (auto const& m : mappedSurfacesStacking) {
        windows.push_back(m->xID);
    }

    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, screen->root, HYPRATOMS["_NET_CLIENT_LIST_STACKING"], XCB_ATOM_WINDOW, 32, windows.size(), windows.data());
}

bool CXWM::isWMWindow(xcb_window_t w) {
    return w == wmWindow || w == clipboard.window || w == dndSelection.window;
}

void CXWM::updateOverrideRedirect(SP<CXWaylandSurface> surf, bool overrideRedirect) {
    if (!surf || surf->overrideRedirect == overrideRedirect)
        return;

    surf->overrideRedirect = overrideRedirect;
}

void CXWM::initSelection() {
    clipboard.window = xcb_generate_id(connection);
    uint32_t mask[1] = {XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, clipboard.window, screen->root, 0, 0, 10, 10, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, XCB_CW_EVENT_MASK,
                      mask);
    xcb_set_selection_owner(connection, clipboard.window, HYPRATOMS["CLIPBOARD_MANAGER"], XCB_TIME_CURRENT_TIME);

    uint32_t mask2 =
        XCB_XFIXES_SELECTION_EVENT_MASK_SET_SELECTION_OWNER | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_WINDOW_DESTROY | XCB_XFIXES_SELECTION_EVENT_MASK_SELECTION_CLIENT_CLOSE;
    xcb_xfixes_select_selection_input(connection, clipboard.window, HYPRATOMS["CLIPBOARD"], mask2);

    clipboard.listeners.setSelection        = g_pSeatManager->events.setSelection.registerListener([this](std::any d) { clipboard.onSelection(); });
    clipboard.listeners.keyboardFocusChange = g_pSeatManager->events.keyboardFocusChange.registerListener([this](std::any d) { clipboard.onKeyboardFocus(); });

    dndSelection.window = xcb_generate_id(connection);
    xcb_create_window(connection, XCB_COPY_FROM_PARENT, dndSelection.window, screen->root, 0, 0, 8192, 8192, 0, XCB_WINDOW_CLASS_INPUT_ONLY, screen->root_visual, XCB_CW_EVENT_MASK,
                      mask);

    uint32_t val1 = XDND_VERSION;
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, dndSelection.window, HYPRATOMS["XdndAware"], XCB_ATOM_ATOM, 32, 1, &val1);
}

void CXWM::setClipboardToWayland(SXSelection& sel) {
    auto source = makeShared<CXDataSource>(sel);
    if (source->mimes().empty()) {
        Debug::log(ERR, "[xwm] can't set clipboard: no MIMEs");
        return;
    }

    sel.dataSource = source;

    Debug::log(LOG, "[xwm] X clipboard at {:x} takes clipboard", (uintptr_t)sel.dataSource.get());
    g_pSeatManager->setCurrentSelection(sel.dataSource);
}

void CXWM::getTransferData(SXSelection& sel) {
    Debug::log(LOG, "[xwm] getTransferData");

    sel.transfer->getIncomingSelectionProp(true);

    if (sel.transfer->propertyReply->type == HYPRATOMS["INCR"]) {
        Debug::log(ERR, "[xwm] Transfer is INCR, which we don't support :(");
        close(sel.transfer->wlFD);
        sel.transfer.reset();
        return;
    } else {
        char*   property  = (char*)xcb_get_property_value(sel.transfer->propertyReply);
        int     remainder = xcb_get_property_value_length(sel.transfer->propertyReply) - sel.transfer->propertyStart;

        ssize_t len = write(sel.transfer->wlFD, property + sel.transfer->propertyStart, remainder);
        if (len == -1) {
            Debug::log(ERR, "[xwm] write died in transfer get");
            close(sel.transfer->wlFD);
            sel.transfer.reset();
            return;
        }

        if (len < remainder) {
            sel.transfer->propertyStart += len;
            Debug::log(ERR, "[xwm] wl client read partially: len {}", len);
            return;
        } else {
            Debug::log(LOG, "[xwm] cb transfer to wl client complete, read {} bytes", len);
            close(sel.transfer->wlFD);
            sel.transfer.reset();
        }
    }
}

void CXWM::setCursor(unsigned char* pixData, uint32_t stride, const Vector2D& size, const Vector2D& hotspot) {
    if (!render_format_id) {
        Debug::log(ERR, "[xwm] can't set cursor: no render format");
        return;
    }

    if (cursorXID)
        xcb_free_cursor(connection, cursorXID);

    constexpr int CURSOR_DEPTH = 32;

    xcb_pixmap_t  pix = xcb_generate_id(connection);
    xcb_create_pixmap(connection, CURSOR_DEPTH, pix, screen->root, size.x, size.y);

    xcb_render_picture_t pic = xcb_generate_id(connection);
    xcb_render_create_picture(connection, pic, pix, render_format_id, 0, 0);

    xcb_gcontext_t gc = xcb_generate_id(connection);
    xcb_create_gc(connection, gc, pix, 0, nullptr);

    xcb_put_image(connection, XCB_IMAGE_FORMAT_Z_PIXMAP, pix, gc, size.x, size.y, 0, 0, 0, CURSOR_DEPTH, stride * size.y * sizeof(uint8_t), pixData);
    xcb_free_gc(connection, gc);

    cursorXID = xcb_generate_id(connection);
    xcb_render_create_cursor(connection, cursorXID, pic, hotspot.x, hotspot.y);
    xcb_free_pixmap(connection, pix);
    xcb_render_free_picture(connection, pic);

    uint32_t values[] = {cursorXID};
    xcb_change_window_attributes(connection, screen->root, XCB_CW_CURSOR, values);
    xcb_flush(connection);
}

void CXWM::sendDndEvent(SP<CWLSurfaceResource> destination, xcb_atom_t type, xcb_client_message_data_t& data) {
    auto XSURF = windowForWayland(destination);

    if (!XSURF) {
        Debug::log(ERR, "[xwm] No xwayland surface for destination in sendDndEvent");
        return;
    }

    xcb_client_message_event_t event = {
        .response_type = XCB_CLIENT_MESSAGE,
        .format        = 32,
        .sequence      = 0,
        .window        = XSURF->xID,
        .type          = type,
        .data          = data,
    };

    xcb_send_event(g_pXWayland->pWM->connection,
                   0, // propagate
                   XSURF->xID, XCB_EVENT_MASK_NO_EVENT, (const char*)&event);
    xcb_flush(g_pXWayland->pWM->connection);
}

SP<CX11DataDevice> CXWM::getDataDevice() {
    return dndDataDevice;
}

SP<IDataOffer> CXWM::createX11DataOffer(SP<CWLSurfaceResource> surf, SP<IDataSource> source) {
    auto XSURF = windowForWayland(surf);

    if (!XSURF) {
        Debug::log(ERR, "[xwm] No xwayland surface for destination in createX11DataOffer");
        return nullptr;
    }

    // invalidate old
    g_pXWayland->pWM->dndDataOffers.clear();

    auto offer             = dndDataOffers.emplace_back(makeShared<CX11DataOffer>());
    offer->self            = offer;
    offer->xwaylandSurface = XSURF;
    offer->source          = source;

    return offer;
}

void SXSelection::onSelection() {
    if (g_pSeatManager->selection.currentSelection && g_pSeatManager->selection.currentSelection->type() == DATA_SOURCE_TYPE_X11)
        return;

    if (g_pSeatManager->selection.currentSelection) {
        xcb_set_selection_owner(g_pXWayland->pWM->connection, g_pXWayland->pWM->clipboard.window, HYPRATOMS["CLIPBOARD"], XCB_TIME_CURRENT_TIME);
        xcb_flush(g_pXWayland->pWM->connection);
        g_pXWayland->pWM->clipboard.notifyOnFocus = true;
    }
}

void SXSelection::onKeyboardFocus() {
    if (!g_pSeatManager->state.keyboardFocusResource || g_pSeatManager->state.keyboardFocusResource->client() != g_pXWayland->pServer->xwaylandClient)
        return;
    if (g_pXWayland->pWM->clipboard.notifyOnFocus) {
        onSelection();
        g_pXWayland->pWM->clipboard.notifyOnFocus = false;
    }
}

int SXSelection::onRead(int fd, uint32_t mask) {
    // TODO: support INCR

    size_t pre = transfer->data.size();
    transfer->data.resize(INCR_CHUNK_SIZE + pre);

    auto len = read(fd, transfer->data.data() + pre, INCR_CHUNK_SIZE - 1);
    if (len < 0) {
        Debug::log(ERR, "[xwm] readDataSource died");
        g_pXWayland->pWM->selectionSendNotify(&transfer->request, false);
        transfer.reset();
        return 0;
    }

    transfer->data.resize(pre + len);

    if (len == 0) {
        Debug::log(LOG, "[xwm] Received all the bytes, final length {}", transfer->data.size());
        xcb_change_property(g_pXWayland->pWM->connection, XCB_PROP_MODE_REPLACE, transfer->request.requestor, transfer->request.property, transfer->request.target, 8,
                            transfer->data.size(), transfer->data.data());
        xcb_flush(g_pXWayland->pWM->connection);
        g_pXWayland->pWM->selectionSendNotify(&transfer->request, true);
        transfer.reset();
    } else
        Debug::log(LOG, "[xwm] Received {} bytes, waiting...", len);

    return 1;
}

static int readDataSource(int fd, uint32_t mask, void* data) {
    Debug::log(LOG, "[xwm] readDataSource on fd {}", fd);

    auto selection = (SXSelection*)data;

    return selection->onRead(fd, mask);
}

bool SXSelection::sendData(xcb_selection_request_event_t* e, std::string mime) {
    WP<IDataSource> selection;
    if (this == &g_pXWayland->pWM->clipboard)
        selection = g_pSeatManager->selection.currentSelection;
    else if (!g_pXWayland->pWM->dndDataOffers.empty())
        selection = g_pXWayland->pWM->dndDataOffers.at(0)->getSource();

    if (!selection)
        return false;

    const auto MIMES = selection->mimes();

    if (MIMES.empty())
        return false;

    if (std::find(MIMES.begin(), MIMES.end(), mime) == MIMES.end()) {
        Debug::log(ERR, "[xwm] X Client asked for an invalid MIME, sending the first advertised. THIS SHIT MAY BREAK!");
        mime = *MIMES.begin();
    }

    transfer          = std::make_unique<SXTransfer>(*this);
    transfer->request = *e;

    int p[2];
    if (pipe(p) == -1) {
        Debug::log(ERR, "[xwm] selection: pipe() failed");
        return false;
    }

    fcntl(p[0], F_SETFD, FD_CLOEXEC);
    fcntl(p[0], F_SETFL, O_NONBLOCK);
    fcntl(p[1], F_SETFD, FD_CLOEXEC);
    fcntl(p[1], F_SETFL, O_NONBLOCK);

    transfer->wlFD = p[0];

    Debug::log(LOG, "[xwm] sending wayland selection to xwayland with mime {}, target {}, fds {} {}", mime, e->target, p[0], p[1]);

    selection->send(mime, p[1]);

    transfer->eventSource = wl_event_loop_add_fd(g_pCompositor->m_sWLEventLoop, transfer->wlFD, WL_EVENT_READABLE, ::readDataSource, this);

    return true;
}

SXTransfer::~SXTransfer() {
    if (wlFD)
        close(wlFD);
    if (eventSource)
        wl_event_source_remove(eventSource);
    if (incomingWindow)
        xcb_destroy_window(g_pXWayland->pWM->connection, incomingWindow);
    if (propertyReply)
        free(propertyReply);
}

bool SXTransfer::getIncomingSelectionProp(bool erase) {
    xcb_get_property_cookie_t cookie = xcb_get_property(g_pXWayland->pWM->connection, erase, incomingWindow, HYPRATOMS["_WL_SELECTION"], XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff);

    propertyStart = 0;
    propertyReply = xcb_get_property_reply(g_pXWayland->pWM->connection, cookie, nullptr);

    if (!propertyReply) {
        Debug::log(ERR, "[SXTransfer] couldn't get a prop reply");
        return false;
    }

    return true;
}

#endif
