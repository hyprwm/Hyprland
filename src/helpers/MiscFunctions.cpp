#include "MiscFunctions.hpp"
#include "../defines.hpp"
#include <algorithm>
#include "../Compositor.hpp"
#include "../managers/TokenManager.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/history/WorkspaceHistoryTracker.hpp"
#include "../output/Monitor.hpp"
#include "../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "fs/FsUtils.hpp"
#include <optional>
#include <cstring>
#include <climits>
#include <cmath>
#include <filesystem>
#include <set>
#include <sys/utsname.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <fstream>
#ifdef HAS_EXECINFO
#include <execinfo.h>
#endif
#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/os/Process.hpp>
#include "../version.h"

using namespace Hyprutils::String;
using namespace Hyprutils::OS;

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/sysctl.h>
#if defined(__DragonFly__)
#include <sys/kinfo.h> // struct kinfo_proc
#elif defined(__FreeBSD__)
#include <sys/user.h> // struct kinfo_proc
#endif

#if defined(__NetBSD__)
#undef KERN_PROC
#define KERN_PROC  KERN_PROC2
#define KINFO_PROC struct kinfo_proc2
#else
#define KINFO_PROC struct kinfo_proc
#endif
#if defined(__DragonFly__)
#define KP_PPID(kp) kp.kp_ppid
#elif defined(__FreeBSD__)
#define KP_PPID(kp) kp.ki_ppid
#else
#define KP_PPID(kp) kp.p_ppid
#endif
#endif

std::string absolutePath(const std::string& rawpath, const std::string& currentPath) {
    auto value = rawpath;

    if (value[0] == '~') {
        static const char* const ENVHOME = getenv("HOME");
        value.replace(0, 1, std::string(ENVHOME));
    } else if (value[0] != '/') {
        auto currentDir = currentPath.substr(0, currentPath.find_last_of('/'));

        if (value[0] == '.') {
            if (value[1] == '.' && value[2] == '/') {
                auto parentDir = currentDir.substr(0, currentDir.find_last_of('/'));
                value.replace(0, 2 + currentPath.empty(), parentDir);
            } else if (value[1] == '/')
                value.replace(0, 1 + currentPath.empty(), currentDir);
            else
                value = currentDir + '/' + value;
        } else
            value = currentDir + '/' + value;
    }

    return value;
}

std::string escapeJSONStrings(const std::string& str) {
    std::ostringstream oss;
    for (auto const& c : str) {
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
                    oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << sc<int>(c);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

float vecToRectDistanceSquared(const Vector2D& vec, const Vector2D& p1, const Vector2D& p2) {
    const float DX = std::max({0.0, p1.x - vec.x, vec.x - p2.x});
    const float DY = std::max({0.0, p1.y - vec.y, vec.y - p2.y});
    return DX * DX + DY * DY;
}

// Execute a shell command and get the output
std::string execAndGet(const char* cmd) {
    CProcess proc("/bin/sh", {"-c", cmd});

    if (!proc.runSync())
        return "error";

    return proc.stdOut();
}

int64_t getPPIDof(int64_t pid) {
#if defined(KERN_PROC_PID)
    int mib[] = {
        CTL_KERN,           KERN_PROC, KERN_PROC_PID, (int)pid,
#if defined(__NetBSD__) || defined(__OpenBSD__)
        sizeof(KINFO_PROC), 1,
#endif
    };
    u_int      miblen = sizeof(mib) / sizeof(mib[0]);
    KINFO_PROC kp;
    size_t     sz = sizeof(KINFO_PROC);
    if (sysctl(mib, miblen, &kp, &sz, NULL, 0) != -1)
        return KP_PPID(kp);

    return 0;
#else
    std::string dir = "/proc/" + std::to_string(pid) + "/status";
    FILE*       infile;

    infile = fopen(dir.c_str(), "r");
    if (!infile)
        return 0;

    char*       line = nullptr;
    size_t      len  = 0;
    ssize_t     len2 = 0;

    std::string pidstr;

    while ((len2 = getline(&line, &len, infile)) != -1) {
        if (strstr(line, "PPid:")) {
            pidstr            = std::string(line, len2);
            const auto tabpos = pidstr.find_last_of('\t');
            if (tabpos != std::string::npos)
                pidstr = pidstr.substr(tabpos);
            break;
        }
    }

    fclose(infile);
    if (line)
        free(line); // NOLINT(cppcoreguidelines-no-malloc)

    try {
        return std::stoll(pidstr);
    } catch (std::exception& e) { return 0; }
#endif
}

double normalizeAngleRad(double ang) {
    if (ang > M_PI * 2) {
        while (ang > M_PI * 2)
            ang -= M_PI * 2;
        return ang;
    }

    if (ang < 0.0) {
        while (ang < 0.0)
            ang += M_PI * 2;
        return ang;
    }

    return ang;
}

std::vector<SCallstackFrameInfo> getBacktrace() {
    std::vector<SCallstackFrameInfo> callstack;

#ifdef HAS_EXECINFO
    void*  bt[1024];
    int    btSize;
    char** btSymbols;

    btSize    = backtrace(bt, 1024);
    btSymbols = backtrace_symbols(bt, btSize);

    for (auto i = 0; i < btSize; ++i) {
        callstack.emplace_back(SCallstackFrameInfo{bt[i], std::string{btSymbols[i]}});
    }
#else
    callstack.emplace_back(SCallstackFrameInfo{nullptr, "configuration does not support execinfo.h"});
#endif

    return callstack;
}

void throwError(const std::string& err) {
    Log::logger->log(Log::CRIT, "Critical error thrown: {}", err);
    throw std::runtime_error(err);
}

std::pair<CFileDescriptor, std::string> openExclusiveShm() {
    // Only absolute paths can be shared across different shm_open() calls
    std::string name = "/" + g_pTokenManager->getRandomUUID();

    for (size_t i = 0; i < 69; ++i) {
        CFileDescriptor fd{shm_open(name.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600)};
        if (fd.isValid())
            return {std::move(fd), name};
    }

    return {{}, ""};
}

CFileDescriptor allocateSHMFile(size_t len) {
    auto [fd, name] = openExclusiveShm();
    if (!fd.isValid())
        return {};

    shm_unlink(name.c_str());

    int ret;
    do {
        ret = ftruncate(fd.get(), len);
    } while (ret < 0 && errno == EINTR);

    if (ret < 0) {
        return {};
    }

    return std::move(fd);
}

bool allocateSHMFilePair(size_t size, CFileDescriptor& rw_fd_ptr, CFileDescriptor& ro_fd_ptr) {
    auto [fd, name] = openExclusiveShm();
    if (!fd.isValid()) {
        return false;
    }

    // CLOEXEC is guaranteed to be set by shm_open
    CFileDescriptor ro_fd{shm_open(name.c_str(), O_RDONLY, 0)};
    if (!ro_fd.isValid()) {
        shm_unlink(name.c_str());
        return false;
    }

    shm_unlink(name.c_str());

    // Make sure the file cannot be re-opened in read-write mode (e.g. via
    // "/proc/self/fd/" on Linux)
    if (fchmod(fd.get(), 0) != 0) {
        return false;
    }

    int ret;
    do {
        ret = ftruncate(fd.get(), size);
    } while (ret < 0 && errno == EINTR);
    if (ret < 0) {
        return false;
    }

    rw_fd_ptr = std::move(fd);
    ro_fd_ptr = std::move(ro_fd);
    return true;
}

float stringToPercentage(const std::string& VALUE, const float REL) {
    if (VALUE.ends_with('%'))
        return (std::stof(VALUE.substr(0, VALUE.length() - 1)) * REL) / 100.f;
    else
        return std::stof(VALUE);
}

// Checks if Nvidia driver major version is at least given version.
// Useful for explicit_sync_kms and ctm_animation as they only work
// past certain driver versions.
bool isNvidiaDriverVersionAtLeast(int threshold) {
    static int  driverMajor = 0;
    static bool once        = true;

    if (once) {
        once = false;

        std::error_code ec;
        if (std::filesystem::exists("/sys/module/nvidia_drm/version", ec) && !ec) {
            std::ifstream ifs("/sys/module/nvidia_drm/version");
            if (ifs.good()) {
                try {
                    std::string driverInfo((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

                    size_t      firstDot = driverInfo.find('.');
                    if (firstDot != std::string::npos)
                        driverMajor = std::stoi(driverInfo.substr(0, firstDot));

                    Log::logger->log(Log::DEBUG, "Parsed NVIDIA major version: {}", driverMajor);

                } catch (std::exception& e) {
                    driverMajor = 0; // Default to 0 if parsing fails
                }

                ifs.close();
            }
        }
    }

    return driverMajor >= threshold;
}

std::expected<std::string, std::string> binaryNameForWlClient(wl_client* client) {
    if (!client)
        return std::unexpected("client unknown");

    pid_t pid = 0;
    wl_client_get_credentials(client, &pid, nullptr, nullptr);

    return binaryNameForPid(pid);
}

std::expected<std::string, std::string> binaryNameForPid(pid_t pid) {
    if (pid <= 0)
        return std::unexpected("No pid for client");

#if defined(KERN_PROC_PATHNAME)
    int mib[] = {
        CTL_KERN,
#if defined(__NetBSD__)
        KERN_PROC_ARGS,
        pid,
        KERN_PROC_PATHNAME,
#else
        KERN_PROC,
        KERN_PROC_PATHNAME,
        pid,
#endif
    };
    u_int  miblen        = sizeof(mib) / sizeof(mib[0]);
    char   exe[PATH_MAX] = "/nonexistent";
    size_t sz            = sizeof(exe);
    sysctl(mib, miblen, &exe, &sz, NULL, 0);
    std::string path = exe;
#else
    std::string path = std::format("/proc/{}/exe", sc<uint64_t>(pid));
#endif
    std::error_code ec;

    std::string     fullPath = std::filesystem::canonical(path, ec);

    if (ec)
        return std::unexpected("canonical failed");

    return fullPath;
}

std::string deviceNameToInternalString(const std::string& in) {
    auto result = in | std::views::transform([](unsigned char ch) -> char {
                      switch (ch) {
                          case ' ':
                          case '\n':
                          case ',': return '-';

                          default: return sc<char>(std::tolower(ch));
                      }
                  });

    return result | std::ranges::to<std::string>();
}

static const std::vector<const char*> PKGCONF_PATHS = {"/usr/lib/pkgconfig", "/usr/local/lib/pkgconfig", "/usr/lib64/pkgconfig"};

//
std::string getSystemLibraryVersion(const std::string& name) {
    for (const auto& pkgconf : PKGCONF_PATHS) {
        std::error_code   ec;
        const std::string PATH = std::string{pkgconf} + "/" + name + ".pc";
        if (!std::filesystem::exists(PATH, ec))
            continue;

        const auto DATA = NFsUtils::readFileAsString(PATH);

        if (!DATA)
            continue;

        size_t versionAt    = DATA->find("\nVersion: ");
        size_t versionAtEnd = DATA->find("\n", versionAt + 11);

        if (versionAt == std::string::npos)
            continue;

        versionAt += 10;

        return DATA->substr(versionAt, versionAtEnd == std::string::npos ? std::string::npos : versionAtEnd - versionAt);
    }
    return "unknown";
}

std::string getBuiltSystemLibraryNames() {
    std::string result = "Libraries:\n";
    result += std::format("Hyprgraphics: built against {}, system has {}\n", HYPRGRAPHICS_VERSION, getSystemLibraryVersion("hyprgraphics"));
    result += std::format("Hyprutils: built against {}, system has {}\n", HYPRUTILS_VERSION, getSystemLibraryVersion("hyprutils"));
    result += std::format("Hyprcursor: built against {}, system has {}\n", HYPRCURSOR_VERSION, getSystemLibraryVersion("hyprcursor"));
    result += std::format("Hyprlang: built against {}, system has {}\n", HYPRLANG_VERSION, getSystemLibraryVersion("hyprlang"));
    result += std::format("Aquamarine: built against {}, system has {}\n", AQUAMARINE_VERSION, getSystemLibraryVersion("aquamarine"));
    return result;
}
