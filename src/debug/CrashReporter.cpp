#include "CrashReporter.hpp"
#include <random>
#include <sys/utsname.h>
#include <fstream>
#include <signal.h>
#include <link.h>

#include "../plugins/PluginSystem.hpp"

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#endif

std::string getRandomMessage() {

    const std::vector<std::string>  MESSAGES = {"Sorry, didn't mean to...",
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

    std::random_device              dev;
    std::mt19937                    engine(dev());
    std::uniform_int_distribution<> distribution(0, MESSAGES.size() - 1);

    return MESSAGES[distribution(engine)];
}

void CrashReporter::createAndSaveCrash(int sig) {

    // get the backtrace
    const int   PID = getpid();

    std::string finalCrashReport = "";

    finalCrashReport += "--------------------------------------------\n   Hyprland Crash Report\n--------------------------------------------\n";
    finalCrashReport += getRandomMessage() + "\n\n";

    finalCrashReport += std::format("Hyprland received signal {} ({})\n\n", sig, (const char*)strsignal(sig));

    finalCrashReport += std::format("Version: {}\nTag: {}\n\n", GIT_COMMIT_HASH, GIT_TAG);

    if (g_pPluginSystem && !g_pPluginSystem->getAllPlugins().empty()) {
        finalCrashReport += "Hyprland seems to be running with plugins. This crash might not be Hyprland's fault.\nPlugins:\n";

        for (auto& p : g_pPluginSystem->getAllPlugins()) {
            finalCrashReport += std::format("\t{} ({}) {}\n", p->name, p->author, p->version);
        }

        finalCrashReport += "\n\n";
    }

    finalCrashReport += "System info:\n";

    struct utsname unameInfo;
    uname(&unameInfo);

    finalCrashReport += std::format("\tSystem name: {}\n\tNode name: {}\n\tRelease: {}\n\tVersion: {}\n\n", std::string{unameInfo.sysname}, std::string{unameInfo.nodename},
                                    std::string{unameInfo.release}, std::string{unameInfo.version});

#if defined(__DragonFly__) || defined(__FreeBSD__)
    const std::string GPUINFO = execAndGet("pciconf -lv | fgrep -A4 vga");
#else
    const std::string GPUINFO = execAndGet("lspci -vnn | grep VGA");
#endif

    finalCrashReport += "GPU:\n\t" + GPUINFO;

    finalCrashReport += std::format("\n\nos-release:\n\t{}\n\n\n", replaceInString(execAndGet("cat /etc/os-release"), "\n", "\n\t"));

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
        // convert in memory address to VMA address
        Dl_info          info;
        struct link_map* linkMap;
        dladdr1((void*)CALLSTACK[i].adr, &info, (void**)&linkMap, RTLD_DL_LINKMAP);
        size_t vmaAddr = (size_t)CALLSTACK[i].adr - linkMap->l_addr;

        addrs += std::format("0x{:x} ", vmaAddr);
    }
#ifdef __clang__
    const auto CMD = std::format("llvm-addr2line -e {} -Cf {}", FPATH.c_str(), addrs);
#else
    const auto CMD   = std::format("addr2line -e {} -Cf {}", FPATH.c_str(), addrs);
#endif

    const auto        ADDR2LINE = execAndGet(CMD.c_str());

    std::stringstream ssin(ADDR2LINE);

    for (size_t i = 0; i < CALLSTACK.size(); ++i) {
        finalCrashReport += std::format("\t#{} | {}", i, CALLSTACK[i].desc);
        std::string functionInfo;
        std::string fileLineInfo;
        std::getline(ssin, functionInfo);
        std::getline(ssin, fileLineInfo);
        finalCrashReport += std::format("\n\t\t{}\n\t\t{}\n", functionInfo, fileLineInfo);
    }

    finalCrashReport += "\n\nLog tail:\n";

    finalCrashReport += Debug::rollingLog.substr(Debug::rollingLog.find("\n") + 1);

    const auto HOME       = getenv("HOME");
    const auto CACHE_HOME = getenv("XDG_CACHE_HOME");

    if (!HOME)
        return;

    std::ofstream ofs;
    std::string   reportDir;

    if (!CACHE_HOME || std::string(CACHE_HOME).empty())
        reportDir = std::string(HOME) + "/.cache/hyprland";
    else
        reportDir = std::string(CACHE_HOME) + "/hyprland";

    if (!std::filesystem::exists(reportDir))
        std::filesystem::create_directory(reportDir);
    const auto path = reportDir + "/hyprlandCrashReport" + std::to_string(PID) + ".txt";

    ofs.open(path, std::ios::trunc);

    ofs << finalCrashReport;

    ofs.close();

    Debug::disableStdout = false;
    Debug::log(CRIT, "Hyprland has crashed :( Consult the crash report at {} for more information.", path);
}
