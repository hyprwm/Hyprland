#include "MiscFunctions.hpp"
#include "../defines.hpp"

void addWLSignal(wl_signal* pSignal, wl_listener* pListener, void* pOwner, std::string ownerString) {
    ASSERT(pSignal);
    ASSERT(pListener);
    
    wl_signal_add(pSignal, pListener);

    Debug::log(LOG, "Registered signal for owner %x: %x -> %x (owner: %s)", pOwner, pSignal, pListener, ownerString.c_str());
}

void handleNoop(struct wl_listener *listener, void *data) {
    // Do nothing
}

void wlr_signal_emit_safe(struct wl_signal *signal, void *data) {
    struct wl_listener cursor;
    struct wl_listener end;

    /* Add two special markers: one cursor and one end marker. This way, we know
	 * that we've already called listeners on the left of the cursor and that we
	 * don't want to call listeners on the right of the end marker. The 'it'
	 * function can remove any element it wants from the list without troubles.
	 * wl_list_for_each_safe tries to be safe but it fails: it works fine
	 * if the current item is removed, but not if the next one is. */
    wl_list_insert(&signal->listener_list, &cursor.link);
    cursor.notify = handleNoop;
    wl_list_insert(signal->listener_list.prev, &end.link);
    end.notify = handleNoop;

    while (cursor.link.next != &end.link) {
        struct wl_list *pos = cursor.link.next;
        struct wl_listener *l = wl_container_of(pos, l, link);

        wl_list_remove(&cursor.link);
        wl_list_insert(pos, &cursor.link);

        l->notify(l, data);
    }

    wl_list_remove(&cursor.link);
    wl_list_remove(&end.link);
}

std::string getFormat(const char *fmt, ...) {
    char buf[2048] = "";

    va_list args;
    va_start(args, fmt);

    vsprintf(buf, fmt, args);

    va_end(args);

    return std::string(buf);
}

void scaleBox(wlr_box* box, float scale) {
    box->width = std::round((box->x + box->width) * scale) - std::round(box->x * scale);
    box->height = std::round((box->y + box->height) * scale) - std::round(box->y * scale);
    box->x = std::round(box->x * scale);
    box->y = std::round(box->y * scale);
}