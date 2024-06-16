#ifndef NO_XWAYLAND

#include "XDataSource.hpp"
#include "XWayland.hpp"
#include "../defines.hpp"

#include <fcntl.h>

CXDataSource::CXDataSource(SXSelection& sel_) : selection(sel_) {
    xcb_get_property_cookie_t cookie = xcb_get_property(g_pXWayland->pWM->connection,
                                                        1, // delete
                                                        selection.window, HYPRATOMS["_WL_SELECTION"], XCB_GET_PROPERTY_TYPE_ANY, 0, 4096);

    xcb_get_property_reply_t* reply = xcb_get_property_reply(g_pXWayland->pWM->connection, cookie, NULL);
    if (!reply)
        return;

    if (reply->type != XCB_ATOM_ATOM) {
        free(reply);
        return;
    }

    auto value = (xcb_atom_t*)xcb_get_property_value(reply);
    for (uint32_t i = 0; i < reply->value_len; i++) {
        if (value[i] == HYPRATOMS["UTF8_STRING"])
            mimeTypes.push_back("text/plain;charset=utf-8");
        else if (value[i] == HYPRATOMS["TEXT"])
            mimeTypes.push_back("text/plain");
        else if (value[i] != HYPRATOMS["TARGETS"] && value[i] != HYPRATOMS["TIMESTAMP"]) {

            auto type = g_pXWayland->pWM->mimeFromAtom(value[i]);

            if (type == "INVALID")
                continue;

            mimeTypes.push_back(type);
        }

        mimeAtoms.push_back(value[i]);
    }

    free(reply);
}

std::vector<std::string> CXDataSource::mimes() {
    return mimeTypes;
}

void CXDataSource::send(const std::string& mime, uint32_t fd) {
    xcb_atom_t mimeAtom = 0;

    if (mime == "text/plain")
        mimeAtom = HYPRATOMS["TEXT"];
    else if (mime == "text/plain;charset=utf-8")
        mimeAtom = HYPRATOMS["UTF8_STRING"];
    else {
        for (size_t i = 0; i < mimeTypes.size(); ++i) {
            if (mimeTypes.at(i) == mime) {
                mimeAtom = mimeAtoms.at(i);
                break;
            }
        }
    }

    if (!mimeAtom) {
        Debug::log(ERR, "[XDataSource] mime atom not found");
        close(fd);
        return;
    }

    Debug::log(LOG, "[XDataSource] send with mime {} to fd {}", mime, fd);

    selection.transfer                 = std::make_unique<SXTransfer>(selection);
    selection.transfer->incomingWindow = xcb_generate_id(g_pXWayland->pWM->connection);
    const uint32_t MASK                = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_create_window(g_pXWayland->pWM->connection, XCB_COPY_FROM_PARENT, selection.transfer->incomingWindow, g_pXWayland->pWM->screen->root, 0, 0, 10, 10, 0,
                      XCB_WINDOW_CLASS_INPUT_OUTPUT, g_pXWayland->pWM->screen->root_visual, XCB_CW_EVENT_MASK, &MASK);

    xcb_convert_selection(g_pXWayland->pWM->connection, selection.transfer->incomingWindow, HYPRATOMS["CLIPBOARD"], mimeAtom, HYPRATOMS["_WL_SELECTION"], XCB_TIME_CURRENT_TIME);

    xcb_flush(g_pXWayland->pWM->connection);

    fcntl(fd, F_SETFL, O_WRONLY | O_NONBLOCK);
    selection.transfer->wlFD = fd;
}

void CXDataSource::accepted(const std::string& mime) {
    Debug::log(LOG, "[XDataSource] accepted is a stub");
}

void CXDataSource::cancelled() {
    Debug::log(LOG, "[XDataSource] cancelled is a stub");
}

void CXDataSource::error(uint32_t code, const std::string& msg) {
    Debug::log(LOG, "[XDataSource] error is a stub: err {}: {}", code, msg);
}

eDataSourceType CXDataSource::type() {
    return DATA_SOURCE_TYPE_X11;
}

#endif