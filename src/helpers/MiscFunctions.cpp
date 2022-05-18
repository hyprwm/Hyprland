#include "MiscFunctions.hpp"
#include "../defines.hpp"
#include <algorithm>
#include "../Compositor.hpp"

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

std::string removeBeginEndSpacesTabs(std::string str) {
    while (str[0] == ' ' || str[0] == '\t') {
        str = str.substr(1);
    }

    while (str.length() != 0 && (str[str.length() - 1] == ' ' || str[str.length() - 1] == '\t')) {
        str = str.substr(0, str.length() - 1);
    }

    return str;
}

float getPlusMinusKeywordResult(std::string source, float relative) {
    float result = INT_MAX;

    if (source.find_first_of("+") == 0) {
        try {
            if (source.find('.') != std::string::npos)
                result = relative + std::stof(source.substr(1));
            else
                result = relative + std::stoi(source.substr(1));
        } catch (...) {
            Debug::log(ERR, "Invalid arg \"%s\" in getPlusMinusKeywordResult!", source.c_str());
            return INT_MAX;
        }
    } else if (source.find_first_of("-") == 0) {
        try {
            if (source.find('.') != std::string::npos)
                result = relative - std::stof(source.substr(1));
            else
                result = relative - std::stoi(source.substr(1));
        } catch (...) {
            Debug::log(ERR, "Invalid arg \"%s\" in getPlusMinusKeywordResult!", source.c_str());
            return INT_MAX;
        }
    } else {
        try {
            if (source.find('.') != std::string::npos)
                result = stof(source);
            else
                result = stoi(source);
        } catch (...) {
            Debug::log(ERR, "Invalid arg \"%s\" in getPlusMinusKeywordResult!", source.c_str());
            return INT_MAX;
        }
    }

    return result;
}

bool isNumber(const std::string& str) {
    return std::ranges::all_of(str.begin(), str.end(), [](char c) { return isdigit(c) != 0; });
}

bool isDirection(const std::string& arg) {
    return arg == "l" || arg == "r" || arg == "u" || arg == "d" || arg == "t" || arg == "b";
}

int getWorkspaceIDFromString(const std::string& in, std::string& outName) {
    int result = INT_MAX;
    if (in.find("name:") == 0) {
        const auto WORKSPACENAME = in.substr(in.find_first_of(':') + 1);
        const auto WORKSPACE = g_pCompositor->getWorkspaceByName(WORKSPACENAME);
        if (!WORKSPACE) {
            result = g_pCompositor->getNextAvailableNamedWorkspace();
        } else {
            result = WORKSPACE->m_iID;
        }
        outName = WORKSPACENAME;
    } else {
        result = std::clamp((int)getPlusMinusKeywordResult(in, g_pCompositor->m_pLastMonitor->activeWorkspace), 1, INT_MAX);
        outName = std::to_string(result);
    }

    return result;
}