#include "CrashReporter.hpp"
#include <fcntl.h>
#include <sys/utsname.h>
#include <link.h>
#include <ctime>
#include <cerrno>
#include <sys/stat.h>
#include <filesystem>

#include "../plugins/PluginSystem.hpp"
#include "../signal-safe.hpp"

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#endif

static char const* const MESSAGES[] = {"Sorry, didn't mean to...",
                                       "This was an accident, I swear!",
                                       "Calm down, it was a misinput! MISINPUT!",
                                       "Oops",
                                       "Vaxry is going to be upset.",
                                       "Who tried dividing by zero?!",
                                       "Maybe you should try dusting your PC in the meantime?",
                                       "I tried so hard, and got so far...",
                                       "I don't feel so good...",
                                       "*thud*",
                                       "Well this is awkward.",
                                       "\"stable\"",
                                       "I hope you didn't have any unsaved progress.",
                                       "All these computers..."};

// <random> is not async-signal-safe, fake it with time(NULL) instead
char const* getRandomMessage() {
    return MESSAGES[time(nullptr) % (sizeof(MESSAGES) / sizeof(MESSAGES[0]))];
}

[[noreturn]] inline void exitWithError(char const* err) {
    write(STDERR_FILENO, err, strlen(err));
    // perror() is not signal-safe, but we use it here
    // because if the crash-handler already crashed, it can't get any worse.
    perror("");
    abort();
}

void NCrashReporter::createAndSaveCrash(int sig) {
    int reportFd = -1;

    // We're in the signal handler, so we *only* have stack memory.
    // To save as much stack memory as possible,
    // destroy things as soon as possible.
    {
        CMaxLengthCString<255> reportPath;

        const auto             HOME       = sigGetenv("HOME");
        const auto             CACHE_HOME = sigGetenv("XDG_CACHE_HOME");

        if (CACHE_HOME && CACHE_HOME[0] != '\0') {
            reportPath += CACHE_HOME;
            reportPath += "/hyprland";
        } else if (HOME && HOME[0] != '\0') {
            reportPath += HOME;
            reportPath += "/.cache/hyprland";
        } else {
            exitWithError("$CACHE_HOME and $HOME not set, nowhere to report crash\n");
            return;
        }

        int ret = mkdir(reportPath.getStr(), S_IRWXU);
        //__asm__("int $3");
        if (ret < 0 && errno != EEXIST) {
            exitWithError("failed to mkdir() crash report directory\n");
        }
        reportPath += "/hyprlandCrashReport";
        reportPath.writeNum(getpid());
        reportPath += ".txt";

        {
            CBufFileWriter<64> stderr(2);
            stderr += "Hyprland has crashed :( Consult the crash report at ";
            if (!reportPath.boundsExceeded()) {
                stderr += reportPath.getStr();
            } else {
                stderr += "[ERROR: Crash report path does not fit into memory! Check if your $CACHE_HOME/$HOME is too deeply nested. Max 255 characters.]";
            }
            stderr += " for more information.\n";
            stderr.flush();
        }

        reportFd = open(reportPath.getStr(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        if (reportFd < 0) {
            exitWithError("Failed to open crash report path for writing");
        }
    }
    CBufFileWriter<512> finalCrashReport(reportFd);

    finalCrashReport += "--------------------------------------------\n   Hyprland Crash Report\n--------------------------------------------\n";
    finalCrashReport += getRandomMessage();
    finalCrashReport += "\n\n";

    finalCrashReport += "Hyprland received signal ";
    finalCrashReport.writeNum(sig);
    finalCrashReport += '(';
    finalCrashReport += sigStrsignal(sig);
    finalCrashReport += ")\nVersion: ";
    finalCrashReport += GIT_COMMIT_HASH;
    finalCrashReport += "\nTag: ";
    finalCrashReport += GIT_TAG;
    finalCrashReport += "\nDate: ";
    finalCrashReport += GIT_COMMIT_DATE;
    finalCrashReport += "\nFlags:\n";
#ifdef LEGACY_RENDERER
    finalCrashReport += "legacyrenderer\n";
#endif
#ifndef ISDEBUG
    finalCrashReport += "debug\n";
#endif
#ifdef NO_XWAYLAND
    finalCrashReport += "no xwayland\n";
#endif
    finalCrashReport += "\n";

    if (g_pPluginSystem && g_pPluginSystem->pluginCount() > 0) {
        finalCrashReport += "Hyprland seems to be running with plugins. This crash might not be Hyprland's fault.\nPlugins:\n";

        const size_t          count = g_pPluginSystem->pluginCount();
        std::vector<CPlugin*> plugins(count);
        g_pPluginSystem->sigGetPlugins(plugins.data(), count);

        for (size_t i = 0; i < count; i++) {
            auto p = plugins[i];
            finalCrashReport += '\t';
            finalCrashReport += p->name;
            finalCrashReport += " (";
            finalCrashReport += p->author;
            finalCrashReport += ") ";
            finalCrashReport += p->version;
            finalCrashReport += '\n';
        }

        finalCrashReport += "\n\n";
    }

    finalCrashReport += "System info:\n";

    {
        struct utsname unameInfo;
        uname(&unameInfo);

        finalCrashReport += "\tSystem name: ";
        finalCrashReport += unameInfo.sysname;
        finalCrashReport += "\n\tNode name: ";
        finalCrashReport += unameInfo.nodename;
        finalCrashReport += "\n\tRelease: ";
        finalCrashReport += unameInfo.release;
        finalCrashReport += "\n\tVersion: ";
        finalCrashReport += unameInfo.version;
        finalCrashReport += "\n\n";
    }

    finalCrashReport += "GPU:\n\t";
#if defined(__DragonFly__) || defined(__FreeBSD__)
    finalCrashReport.writeCmdOutput("pciconf -lv | grep -F -A4 vga");
#else
    finalCrashReport.writeCmdOutput("lspci -vnn | grep -E (VGA|Display|3D)");
#endif

    finalCrashReport += "\n\nos-release:\n";
    finalCrashReport.writeCmdOutput("cat /etc/os-release | sed 's/^/\t/'");

    // dladdr1()/backtrace_symbols()/this entire section allocates, and hence is NOT async-signal-safe.
    // Make sure that we save the current known crash report information,
    // so that if we are caught in a deadlock during a call to malloc(),
    // there is still something to debug from.
    finalCrashReport.flush();

    finalCrashReport += "Backtrace:\n";

    const auto CALLSTACK = getBacktrace();

#if defined(KERN_PROC_PATHNAME)
    int mib[] = {
        CTL_KERN,
#if defined(__NetBSD__)
        KERN_PROC_ARGS,
        -1,
        KERN_PROC_PATHNAME,
#else
        KERN_PROC,
        KERN_PROC_PATHNAME,
        -1,
#endif
    };
    u_int  miblen        = sizeof(mib) / sizeof(mib[0]);
    char   exe[PATH_MAX] = "";
    size_t sz            = sizeof(exe);
    sysctl(mib, miblen, &exe, &sz, NULL, 0);
    const auto FPATH = std::filesystem::canonical(exe);
#elif defined(__OpenBSD__)
    // Neither KERN_PROC_PATHNAME nor /proc are supported
    const auto FPATH = std::filesystem::canonical("/usr/local/bin/Hyprland");
#else
    const auto FPATH = std::filesystem::canonical("/proc/self/exe");
#endif

    std::string addrs = "";
    for (size_t i = 0; i < CALLSTACK.size(); ++i) {
#ifdef __GLIBC__
        // convert in memory address to VMA address
        Dl_info          info;
        struct link_map* linkMap;
        dladdr1((void*)CALLSTACK[i].adr, &info, (void**)&linkMap, RTLD_DL_LINKMAP);
        size_t vmaAddr = (size_t)CALLSTACK[i].adr - linkMap->l_addr;
#else
        // musl doesn't define dladdr1
        size_t vmaAddr = (size_t)CALLSTACK[i].adr;
#endif

        addrs += std::format("0x{:x} ", vmaAddr);
    }
#ifdef __clang__
    const auto CMD = std::format("llvm-addr2line -e {} -Cf {}", FPATH.c_str(), addrs);
#else
    const auto CMD = std::format("addr2line -e {} -Cf {}", FPATH.c_str(), addrs);
#endif

    const auto        ADDR2LINE = execAndGet(CMD.c_str());

    std::stringstream ssin(ADDR2LINE);

    for (size_t i = 0; i < CALLSTACK.size(); ++i) {
        finalCrashReport += "\t#";
        finalCrashReport.writeNum(i);
        finalCrashReport += " | ";
        finalCrashReport += CALLSTACK[i].desc;
        std::string functionInfo;
        std::string fileLineInfo;
        std::getline(ssin, functionInfo);
        std::getline(ssin, fileLineInfo);
        finalCrashReport += std::format("\n\t\t{}\n\t\t{}\n", functionInfo, fileLineInfo);
    }

    finalCrashReport += "\n\nLog tail:\n";

    finalCrashReport += std::string_view(Debug::rollingLog).substr(Debug::rollingLog.find('\n') + 1);
}
