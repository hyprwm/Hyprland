#include "MiscFunctions.hpp"
#include "../defines.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include <sys/utsname.h>
#include <iomanip>

static const float transforms[][9] = {{
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},{
		0.0f, 1.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},{
		-1.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},{
		0.0f, -1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},{
		-1.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},{
		0.0f, 1.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},{
		1.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},{
		0.0f, -1.0f, 0.0f,
		-1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f,
	},
};

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
    char buf[LOGMESSAGESIZE] = "";
    char* outputStr;
    int logLen;

    va_list args;
    va_start(args, fmt);
    logLen = vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);

    if ((long unsigned int)logLen < sizeof buf) {
        outputStr = strdup(buf);
    } else {
        outputStr = (char*)malloc(logLen + 1);

        if (!outputStr) {
            printf("CRITICAL: Cannot alloc size %d for log! (Out of memory?)", logLen + 1);
            return "";
        }

        va_start(args, fmt);
        vsnprintf(outputStr, logLen + 1U, fmt, args);
        va_end(args);
    }

    std::string output = std::string(outputStr);

    free(outputStr);

    return output;
}

std::string escapeJSONStrings(const std::string& str) {
    std::ostringstream oss;
    for (auto &c : str) {
        switch (c) {
        case '"': oss << "\\\""; break;
        case '\\': oss << "\\\\"; break;
        case '\b': oss << "\\b"; break;
        case '\f': oss << "\\f"; break;
        case '\n': oss << "\\n"; break;
        case '\r': oss << "\\r"; break;
        case '\t': oss << "\\t"; break;
        default:
            if ('\x00' <= c && c <= '\x1f') {
                oss << "\\u"
                    << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
            } else {
                oss << c;
            }
        }
    }
    return oss.str();
}

void scaleBox(wlr_box* box, float scale) {
    box->width = std::round(box->width * scale);
    box->height = std::round(box->height * scale);
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
            if (source.contains("."))
                result = relative + std::stof(source.substr(1));
            else
                result = relative + std::stoi(source.substr(1));
        } catch (...) {
            Debug::log(ERR, "Invalid arg \"%s\" in getPlusMinusKeywordResult!", source.c_str());
            return INT_MAX;
        }
    } else if (source.find_first_of("-") == 0) {
        try {
            if (source.contains("."))
                result = relative - std::stof(source.substr(1));
            else
                result = relative - std::stoi(source.substr(1));
        } catch (...) {
            Debug::log(ERR, "Invalid arg \"%s\" in getPlusMinusKeywordResult!", source.c_str());
            return INT_MAX;
        }
    } else {
        try {
            if (source.contains("."))
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

bool isNumber(const std::string& str, bool allowfloat) {
    return std::ranges::all_of(str.begin(), str.end(), [&](char c) { return isdigit(c) != 0 || c == '-' || (allowfloat && c == '.'); });
}

bool isDirection(const std::string& arg) {
    return arg == "l" || arg == "r" || arg == "u" || arg == "d" || arg == "t" || arg == "b";
}

int getWorkspaceIDFromString(const std::string& in, std::string& outName) {
    int result = INT_MAX;
    if (in.find("special") == 0) {
        outName = "special";
        return SPECIAL_WORKSPACE_ID;
    } else if (in.find("name:") == 0) {
        const auto WORKSPACENAME = in.substr(in.find_first_of(':') + 1);
        const auto WORKSPACE = g_pCompositor->getWorkspaceByName(WORKSPACENAME);
        if (!WORKSPACE) {
            result = g_pCompositor->getNextAvailableNamedWorkspace();
        } else {
            result = WORKSPACE->m_iID;
        }
        outName = WORKSPACENAME;
    } else {
        if (in[0] == 'm' || in[0] == 'e') {
            bool onAllMonitors = in[0] == 'e';

            if (!g_pCompositor->m_pLastMonitor) {
                Debug::log(ERR, "Relative monitor workspace on monitor null!");
                result = INT_MAX;
                return result;
            }

            // monitor relative
            result = (int)getPlusMinusKeywordResult(in.substr(1), 0);

            // result now has +/- what we should move on mon
            int remains = (int)result;
            int currentID = g_pCompositor->m_pLastMonitor->activeWorkspace;
            int searchID = currentID;

            while (remains != 0) {
                if (remains < 0)
                    searchID--;
                else 
                    searchID++;
                
                if (g_pCompositor->workspaceIDOutOfBounds(searchID)){
                    // means we need to wrap around
                    int lowestID = 99999;
                    int highestID = -99999;

                    for (auto& w : g_pCompositor->m_vWorkspaces) {
                        if (w->m_iID < lowestID)
                            lowestID = w->m_iID;

                        if (w->m_iID > highestID)
                            highestID = w->m_iID;
                    }

                    if (remains < 0)
                        searchID = highestID;
                    else 
                        searchID = lowestID;
                }

                if (const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(searchID); PWORKSPACE && PWORKSPACE->m_iID != SPECIAL_WORKSPACE_ID) {
                    if (onAllMonitors || PWORKSPACE->m_iMonitorID == g_pCompositor->m_pLastMonitor->ID) {
                        currentID = PWORKSPACE->m_iID;

                        if (remains < 0)
                            remains++;
                        else
                            remains--;
                    }
                }
            }

            result = currentID;
            outName = g_pCompositor->getWorkspaceByID(currentID)->m_szName;

        } else {
            if (g_pCompositor->m_pLastMonitor)
                result = std::clamp((int)getPlusMinusKeywordResult(in, g_pCompositor->m_pLastMonitor->activeWorkspace), 1, INT_MAX);
            else if (isNumber(in))
                result = std::clamp(std::stoi(in), 1, INT_MAX);
            else {
                Debug::log(ERR, "Relative workspace on no mon!");
                result = INT_MAX;
            }
            outName = std::to_string(result);
        }        
    }

    return result;
}

float vecToRectDistanceSquared(const Vector2D& vec, const Vector2D& p1, const Vector2D& p2) {
    const float DX = std::max((double)0, std::max(p1.x - vec.x, vec.x - p2.x));
    const float DY = std::max((double)0, std::max(p1.y - vec.y, vec.y - p2.y));
    return DX * DX + DY * DY;
}

// Execute a shell command and get the output
std::string execAndGet(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        Debug::log(ERR, "execAndGet: failed in pipe");
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

void logSystemInfo() {
    struct utsname unameInfo;

    uname(&unameInfo);

    Debug::log(LOG, "System name: %s", unameInfo.sysname);
    Debug::log(LOG, "Node name: %s", unameInfo.nodename);
    Debug::log(LOG, "Release: %s", unameInfo.release);
    Debug::log(LOG, "Version: %s", unameInfo.version);

    // log etc
    Debug::log(LOG, "os-release:");

    Debug::log(NONE, "%s", execAndGet("cat /etc/os-release").c_str());
}

void matrixProjection(float mat[9], int w, int h, wl_output_transform tr) {
    memset(mat, 0, sizeof(*mat) * 9);

    const float* t = transforms[tr];
    float x = 2.0f / w;
    float y = 2.0f / h;

    // Rotation + reflection
    mat[0] = x * t[0];
    mat[1] = x * t[1];
    mat[3] = y * -t[3];
    mat[4] = y * -t[4];

    // Translation
    mat[2] = -copysign(1.0f, mat[0] + mat[1]);
    mat[5] = -copysign(1.0f, mat[3] + mat[4]);

    // Identity
    mat[8] = 1.0f;
}
