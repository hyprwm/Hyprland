#pragma once

#include <optional>
#include <wayland-server.h>
#include <vector>
#include <format>
#include <expected>
#include <hyprutils/os/FileDescriptor.hpp>
#include "../SharedDefs.hpp"
#include "../macros.hpp"

struct SCallstackFrameInfo {
    void*       adr = nullptr;
    std::string desc;
};

struct SWorkspaceIDName {
    WORKSPACEID id = WORKSPACE_INVALID;
    std::string name;
};

std::string                             absolutePath(const std::string&, const std::string&);
std::string                             escapeJSONStrings(const std::string& str);
bool                                    isDirection(const std::string&);
bool                                    isDirection(const char&);
SWorkspaceIDName                        getWorkspaceIDNameFromString(const std::string&);
std::optional<std::string>              cleanCmdForWorkspace(const std::string&, std::string);
float                                   vecToRectDistanceSquared(const Vector2D& vec, const Vector2D& p1, const Vector2D& p2);
void                                    logSystemInfo();
std::string                             execAndGet(const char*);
int64_t                                 getPPIDof(int64_t pid);
std::expected<int64_t, std::string>     configStringToInt(const std::string&);
Vector2D                                configStringToVector2D(const std::string&);
std::optional<float>                    getPlusMinusKeywordResult(std::string in, float relative);
double                                  normalizeAngleRad(double ang);
std::vector<SCallstackFrameInfo>        getBacktrace();
void                                    throwError(const std::string& err);
bool                                    envEnabled(const std::string& env);
Hyprutils::OS::CFileDescriptor          allocateSHMFile(size_t len);
bool                                    allocateSHMFilePair(size_t size, Hyprutils::OS::CFileDescriptor& rw_fd_ptr, Hyprutils::OS::CFileDescriptor& ro_fd_ptr);
float                                   stringToPercentage(const std::string& VALUE, const float REL);
bool                                    isNvidiaDriverVersionAtLeast(int threshold);
std::expected<std::string, std::string> binaryNameForWlClient(wl_client* client);
std::expected<std::string, std::string> binaryNameForPid(pid_t pid);
std::string                             deviceNameToInternalString(std::string in);

template <typename... Args>
[[deprecated("use std::format instead")]] std::string getFormat(std::format_string<Args...> fmt, Args&&... args) {
    // no need for try {} catch {} because std::format_string<Args...> ensures that vformat never throw std::format_error
    // because any suck format specifier will cause a compilation error
    // this is actually what std::format in stdlib does
    return std::vformat(fmt.get(), std::make_format_args(args...));
}
