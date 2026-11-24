#include "CrashReporter.hpp"
#include <fcntl.h>
#include <sys/utsname.h>
#include <link.h>
#include <ctime>
#include <cerrno>
#include <sys/stat.h>
#include <filesystem>
#include <map>
#include <unordered_map>
#include <optional>
#include <charconv>
#include <hyprutils/os/Process.hpp>
#include "../../helpers/MiscFunctions.hpp"

#include "../../plugins/PluginSystem.hpp"
#include "SignalSafe.hpp"

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#endif

static char const* const MESSAGES[] = {
    "Sorry, didn't mean to...",
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
    "All these computers...",
    "The math isn't mathing...",
    "We've got an imposter in the code!",
    "Well, at least the crash reporter didn't crash!",
    "Everything's just fi-",
    "Have you tried asking Hyprland politely not to crash?",
    "It worked on my machine!",
    "Suboptimal.",
    "Well, that's one way to exit fullscreen.",
    "Somewhere, a nullptr is feeling very accomplished.",
    "The kernel was unhappy.",
    "Introducing our brand new zero-window layout!",
    "Can't have rendering issues if you don't render anything!",
    "Reduced memory usage by 100%",
    "Your desktop is now so much more lightweight.",
    "Please do not feed the stacktrace.",
    "There are no bugs in Ba Sing Se",
    "The Industrial Revolution and its consequences...",
    "Tell my config I love it...",
    "This will negatively affect the trout population.",
    "Anyway, here's a bunch of hexadecimal numbers.",
    "The horrors persist, but I don't.",
    "The pointer pointedn't.",
    "One does not simply dereference that.",
    "Goodbye, cruel address space.",
    "Skill issue at 0x00000004",
    "The compiler warned us about this relationship.",
    "That's probably not my memory.",
    "An unexpected feature has occurred.",
};

// set before raising SIGABRT, only read in the signal handler
static std::string assertReason;

// captured at startup. Forked children (e.g. libseat's builtin seatd) inherit it, but have a different pid
static const pid_t HYPRLAND_PID = getpid();

// <random> is not async-signal-safe, fake it with time(NULL) instead
static char const* getRandomMessage() {
    return MESSAGES[time(nullptr) % std::size(MESSAGES)];
}

[[noreturn]] static inline void exitWithError(char const* err) {
    [[maybe_unused]] auto w = write(STDERR_FILENO, err, strlen(err));
    // perror() is not signal-safe, but we use it here
    // because if the crash-handler already crashed, it can't get any worse.
    perror("");
    abort();
}

void CrashReporter::setAssertReason(std::string_view reason) {
    assertReason = reason;
}

void CrashReporter::createAndSaveCrash(int sig, const void* sigReturnAddr) {
    // a forked child crashed while running our code (e.g. our exit handlers), this isn't a Hyprland crash
    if (getpid() != HYPRLAND_PID)
        return;

    int reportFd = -1;

    // We're in the signal handler, so we *only* have stack memory.
    // To save as much stack memory as possible,
    // destroy things as soon as possible.
    {
        SignalSafe::CMaxLengthCString<255> reportPath;

        const auto                         HOME       = SignalSafe::getenv("HOME");
        const auto                         CACHE_HOME = SignalSafe::getenv("XDG_CACHE_HOME");

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
        if (ret < 0 && errno != EEXIST)
            exitWithError("failed to mkdir() crash report directory\n");

        reportPath += "/hyprlandCrashReport";
        reportPath.writeNum(getpid());
        reportPath += ".txt";

        {
            SignalSafe::CBufFileWriter<64> stderrOut(STDERR_FILENO);
            stderrOut += "Hyprland has crashed :( Consult the crash report at ";
            if (!reportPath.boundsExceeded())
                stderrOut += reportPath.getStr();
            else
                stderrOut += "[ERROR: Crash report path does not fit into memory! Check if your $CACHE_HOME/$HOME is too deeply nested. Max 255 characters.]";

            stderrOut += " for more information.\n";
            stderrOut.flush();
        }

        reportFd = open(reportPath.getStr(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (reportFd < 0)
            exitWithError("Failed to open crash report path for writing");
    }
    SignalSafe::CBufFileWriter<512> finalCrashReport(reportFd);

    finalCrashReport += "--------------------------------------------\n   Hyprland Crash Report\n--------------------------------------------\n";
    finalCrashReport += getRandomMessage();
    finalCrashReport += "\n\n";

    finalCrashReport += "Hyprland received signal ";
    finalCrashReport.writeNum(sig);
    finalCrashReport += '(';
    finalCrashReport += SignalSafe::strsignal(sig);
    finalCrashReport += ")\n";

    if (!assertReason.empty()) {
        finalCrashReport += "Assertion failed: ";
        finalCrashReport += assertReason;
        finalCrashReport += '\n';
    }

    finalCrashReport += "Version: ";
    finalCrashReport += GIT_COMMIT_HASH;
    finalCrashReport += "\nTag: ";
    finalCrashReport += GIT_TAG;
    finalCrashReport += "\nDate: ";
    finalCrashReport += GIT_COMMIT_DATE;
    finalCrashReport += "\nFlags:\n";
#if ISDEBUG
    finalCrashReport += "debug\n";
#endif
#ifdef NO_XWAYLAND
    finalCrashReport += "no xwayland\n";
#endif
    finalCrashReport += "\n";

    // Flush after every section, so that if the reporter itself crashes or hangs,
    // everything gathered up to that point is kept.
    finalCrashReport.flush();

    if (g_pPluginSystem && g_pPluginSystem->pluginCount() > 0) {
        finalCrashReport += "Hyprland seems to be running with plugins. This crash might not be Hyprland's fault.\nPlugins:\n";

        const size_t          count = g_pPluginSystem->pluginCount();
        std::vector<CPlugin*> plugins(count);
        g_pPluginSystem->sigGetPlugins(plugins.data(), count);

        for (size_t i = 0; i < count; i++) {
            auto p = plugins[i];
            finalCrashReport += '\t';
            finalCrashReport += p->m_name;
            finalCrashReport += " (";
            finalCrashReport += p->m_author;
            finalCrashReport += ") ";
            finalCrashReport += p->m_version;
            finalCrashReport += '\n';
        }

        finalCrashReport += "\n\n";
    }

    finalCrashReport.flush();

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

    finalCrashReport.flush();

    finalCrashReport += "GPU:\n\t";
#if defined(__DragonFly__) || defined(__FreeBSD__)
    finalCrashReport.writeCmdOutput("pciconf -lv | grep -F -A4 vga");
#else
    finalCrashReport.writeCmdOutput("lspci -vnn | grep -E '(VGA|Display|3D)'");
#endif

    finalCrashReport.flush();

    finalCrashReport += "\n\nos-release:\n";
    finalCrashReport.writeCmdOutput("cat /etc/os-release | sed 's/^/\t/'");

    finalCrashReport.flush();

    finalCrashReport += '\n';
    finalCrashReport += getBuiltSystemLibraryNames();
    finalCrashReport += '\n';

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
    u_int  miblen        = std::size(mib);
    char   exe[PATH_MAX] = "/nonexistent";
    size_t sz            = sizeof(exe);
    sysctl(mib, miblen, &exe, &sz, NULL, 0);
    const auto FPATH = std::filesystem::canonical(exe);
#elif defined(__OpenBSD__)
    // Neither KERN_PROC_PATHNAME nor /proc are supported
    const auto FPATH = std::filesystem::canonical("/usr/local/bin/Hyprland");
#else
    const auto FPATH = std::filesystem::canonical("/proc/self/exe");
#endif

    // every frame has to be symbolized against the object it's in: Hyprland, a library, a plugin...
    std::vector<size_t>                        frameAddrs(CALLSTACK.size());
    std::map<std::string, std::vector<size_t>> framesByObject;

    for (size_t i = 0; i < CALLSTACK.size(); ++i) {
        // Return addresses point after the call, which can be on another line, or even in the next function
        // if the call was the last instruction. Look up the call instead.
        // The signal trampoline and the frame interrupted by the signal (right after it) aren't return addresses, leave them be.
        const bool  EXACT = CALLSTACK[i].adr == sigReturnAddr || (i > 0 && CALLSTACK[i - 1].adr == sigReturnAddr);
        const auto  PC    = rc<size_t>(CALLSTACK[i].adr) - (EXACT ? 0 : 1);

        std::string object = FPATH.string();
        frameAddrs[i]      = PC;
#ifdef __GLIBC__
        // convert in memory address to VMA address
        Dl_info          info;
        struct link_map* linkMap;
        if (dladdr1(rc<void*>(PC), &info, rc<void**>(&linkMap), RTLD_DL_LINKMAP)) {
            frameAddrs[i] = PC - linkMap->l_addr;

            // the main executable has an empty name
            if (linkMap->l_name && linkMap->l_name[0] != '\0')
                object = linkMap->l_name;
        }
#else
        // musl doesn't define dladdr1
#endif
        framesByObject[object].emplace_back(i);
    }

    // locations of every frame, innermost first, followed by the ones it's inlined in
    std::vector<std::vector<std::string>> symbols(CALLSTACK.size());

    for (const auto& [object, frames] : framesByObject) {
        std::vector<std::string> args = {"-e", object, "-apiCf"};
        for (const auto& i : frames) {
            args.emplace_back(std::format("0x{:x}", frameAddrs[i]));
        }

#ifdef __clang__
        Hyprutils::OS::CProcess addr2line("llvm-addr2line", args);
#else
        Hyprutils::OS::CProcess addr2line("addr2line", args);
#endif

        if (!addr2line.runSync())
            continue;

        // every address is echoed, followed by its location and the ones it's inlined in:
        // 0x1234: func at file:line
        //  (inlined by) caller at file:line
        std::unordered_map<size_t, std::vector<std::string>> locations;
        std::optional<size_t>                                currentAddr;
        std::stringstream                                    ssin(addr2line.stdOut());

        for (std::string line; std::getline(ssin, line);) {
            const auto COLON = line.find(": ");

            if (line.starts_with("0x") && COLON != std::string::npos) {
                size_t addr = 0;
                if (std::from_chars(line.data() + 2, line.data() + COLON, addr, 16).ec != std::errc{}) {
                    currentAddr.reset();
                    continue;
                }

                currentAddr = addr;
                locations[addr].emplace_back(line.substr(COLON + 2));
                continue;
            }

            if (currentAddr)
                locations[*currentAddr].emplace_back(line);
        }

        for (const auto& i : frames) {
            if (const auto it = locations.find(frameAddrs[i]); it != locations.end())
                symbols[i] = it->second;
        }
    }

    for (size_t i = 0; i < CALLSTACK.size(); ++i) {
        finalCrashReport += "\t#";
        finalCrashReport.writeNum(i);
        finalCrashReport += " | ";
        finalCrashReport += CALLSTACK[i].desc;
        finalCrashReport += '\n';

        if (symbols[i].empty())
            finalCrashReport += "\t\t??\n";

        for (const auto& location : symbols[i]) {
            finalCrashReport += "\t\t";
            finalCrashReport += location;
            finalCrashReport += '\n';
        }
    }

    finalCrashReport.flush();

    finalCrashReport += "\n\nLog tail:\n";

    finalCrashReport += Log::logger->rolling();
}
