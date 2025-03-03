#ifndef NO_XWAYLAND

#include "XWayland.hpp"
#include "../defines.hpp"
#include "XDataSource.hpp"

#include <fcntl.h>
using namespace Hyprutils::OS;

CXDataSource::CXDataSource(SXSelection& sel_) : selection(sel_) {
    xcb_get_property_cookie_t cookie = xcb_get_property(g_pXWayland->pWM->connection,
                                                        1, // delete
                                                        selection.window, HYPRATOMS["_WL_SELECTION"], XCB_GET_PROPERTY_TYPE_ANY, 0, 4096);

    xcb_get_property_reply_t* reply = xcb_get_property_reply(g_pXWayland->pWM->connection, cookie, nullptr);
    if (!reply)
        return;

    if (reply->type != XCB_ATOM_ATOM) {
        free(reply);
        return;
    }

    auto value = (xcb_atom_t*)xcb_get_property_value(reply);
    for (uint32_t i = 0; i < reply->value_len; i++) {
        if (value[i] == HYPRATOMS["UTF8_STRING"])
            mimeTypes.emplace_back("text/plain;charset=utf-8");
        else if (value[i] == HYPRATOMS["TEXT"])
            mimeTypes.emplace_back("text/plain");
        else if (value[i] != HYPRATOMS["TARGETS"] && value[i] != HYPRATOMS["TIMESTAMP"]) {

            auto type = g_pXWayland->pWM->mimeFromAtom(value[i]);

            if (type == "INVALID")
                continue;

            mimeTypes.push_back(type);
        } else
            continue;

        mimeAtoms.push_back(value[i]);
    }

    free(reply);
}

std::vector<std::string> CXDataSource::mimes() {
    return mimeTypes;
}

void CXDataSource::send(const std::string& mime, CFileDescriptor fd) {
    xcb_atom_t mimeAtom = 0;

    if (mime == "text/plain")
        mimeAtom = HYPRATOMS["TEXT"];
    else if (mime == "text/plain;charset=utf-8")
        mimeAtom = HYPRATOMS["UTF8_STRING"];
    else {
        for (size_t i = 0; i < mimeTypes.size(); ++i) {
            if (mimeTypes[i] == mime) {
                mimeAtom = mimeAtoms[i];
                break;
            }
        }
    }

    if (!mimeAtom) {
        NDebug::log(ERR, "[XDataSource] mime atom not found");
        return;
    }

    NDebug::log(LOG, "[XDataSource] send with mime {} to fd {}", mime, fd.get());

    auto transfer            = makeUnique<SXTransfer>(selection);
    transfer->incomingWindow = xcb_generate_id(g_pXWayland->pWM->connection);
    const uint32_t MASK      = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_create_window(g_pXWayland->pWM->connection, XCB_COPY_FROM_PARENT, transfer->incomingWindow, g_pXWayland->pWM->screen->root, 0, 0, 10, 10, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      g_pXWayland->pWM->screen->root_visual, XCB_CW_EVENT_MASK, &MASK);

    xcb_convert_selection(g_pXWayland->pWM->connection, transfer->incomingWindow, HYPRATOMS["CLIPBOARD"], mimeAtom, HYPRATOMS["_WL_SELECTION"], XCB_TIME_CURRENT_TIME);

    xcb_flush(g_pXWayland->pWM->connection);

    //TODO: make CFileDescriptor setflags take SETFL aswell
    fcntl(fd.get(), F_SETFL, O_WRONLY | O_NONBLOCK);
    transfer->wlFD = std::move(fd);
    selection.transfers.emplace_back(std::move(transfer));
}

void CXDataSource::accepted(const std::string& mime) {
    NDebug::log(LOG, "[XDataSource] accepted is a stub");
}

void CXDataSource::cancelled() {
    NDebug::log(LOG, "[XDataSource] cancelled is a stub");
}

void CXDataSource::error(uint32_t code, const std::string& msg) {
    NDebug::log(LOG, "[XDataSource] error is a stub: err {}: {}", code, msg);
}

eDataSourceType CXDataSource::type() {
    return DATA_SOURCE_TYPE_X11;
}

#endif
