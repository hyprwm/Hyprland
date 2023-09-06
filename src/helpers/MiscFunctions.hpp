#pragma once

#include <string>
#include <wayland-server.h>
#include <wlr/util/box.h>
#include "Vector2D.hpp"
#include <vector>
#include <format>

struct SCallstackFrameInfo {
    void*       adr = nullptr;
    std::string desc;
};

std::string                      absolutePath(const std::string&, const std::string&);
void                             addWLSignal(wl_signal*, wl_listener*, void* pOwner, const std::string& ownerString);
std::string                      escapeJSONStrings(const std::string& str);
void                             scaleBox(wlr_box*, float);
std::string                      removeBeginEndSpacesTabs(std::string);
bool                             isNumber(const std::string&, bool allowfloat = false);
bool                             isDirection(const std::string&);
int                              getWorkspaceIDFromString(const std::string&, std::string&);
float                            vecToRectDistanceSquared(const Vector2D& vec, const Vector2D& p1, const Vector2D& p2);
void                             logSystemInfo();
std::string                      execAndGet(const char*);
int64_t                          getPPIDof(int64_t pid);
int64_t                          configStringToInt(const std::string&);
float                            getPlusMinusKeywordResult(std::string in, float relative);
void                             matrixProjection(float mat[9], int w, int h, wl_output_transform tr);
double                           normalizeAngleRad(double ang);
std::string                      replaceInString(std::string subject, const std::string& search, const std::string& replace);
std::vector<SCallstackFrameInfo> getBacktrace();
void                             throwError(const std::string& err);

// why, C++.
std::string sendToLog(uint8_t, const std::string&);
template <typename... Args>
std::string getFormat(const std::string& fmt, Args&&... args) {
    std::string fmtdMsg;

    try {
        fmtdMsg += std::vformat(fmt, std::make_format_args(args...));
    } catch (std::exception& e) {
        std::string exceptionMsg = e.what();
        sendToLog(2, std::format("caught exception in getFormat: {}", exceptionMsg));

        const auto CALLSTACK = getBacktrace();

        sendToLog(0, "stacktrace:");

        for (size_t i = 0; i < CALLSTACK.size(); ++i) {
            sendToLog(1, std::format("\t #{} | {}", i, CALLSTACK[i].desc));
        }
    }

    return fmtdMsg;
}