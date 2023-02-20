#include "CrashReporter.hpp"
#include <random>
#include <sys/utsname.h>
#include <execinfo.h>
#include <fstream>

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
                                               "I hope you didn't have any unsaved progress."};

    std::random_device              dev;
    std::mt19937                    engine(dev());
    std::uniform_int_distribution<> distribution(0, MESSAGES.size() - 1);

    return MESSAGES[distribution(engine)];
}

void CrashReporter::createAndSaveCrash() {

    // get the backtrace
    const int   PID = getpid();

    std::string finalCrashReport = "";

    finalCrashReport += "--------------------------------------------\n   Hyprland Crash Report\n--------------------------------------------\n";
    finalCrashReport += getRandomMessage() + "\n\n";

    finalCrashReport += "Hyprland received signal 11 (SIGSEGV): Segmentation Fault\n\n";

    finalCrashReport += "System info:\n";

    struct utsname unameInfo;
    uname(&unameInfo);

    finalCrashReport +=
        getFormat("\tSystem name: %s\n\tNode name: %s\n\tRelease: %s\n\tVersion: %s\n\n", unameInfo.sysname, unameInfo.nodename, unameInfo.release, unameInfo.version);

#if defined(__DragonFly__) || defined(__FreeBSD__)
    const std::string GPUINFO = execAndGet("pciconf -lv | fgrep -A4 vga");
#else
    const std::string GPUINFO = execAndGet("lspci -vnn | grep VGA");
#endif

    finalCrashReport += "GPU:\n\t" + GPUINFO;

    finalCrashReport += getFormat("\n\nos-release:\n\t%s\n\n\n", replaceInString(execAndGet("cat /etc/os-release"), "\n", "\n\t").c_str());

    finalCrashReport += "Backtrace:\n";

    void*  bt[1024];
    size_t btSize;
    char** btSymbols;

    btSize           = backtrace(bt, 1024);
    btSymbols        = backtrace_symbols(bt, btSize);

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
    u_int      miblen = sizeof(mib) / sizeof(mib[0]);
    char       exe[PATH_MAX] = "";
    size_t     sz = sizeof(exe);
    sysctl(mib, miblen, &exe, &sz, NULL, 0);
    const auto FPATH = std::filesystem::canonical(exe);
#elif defined(__OpenBSD__)
    // Neither KERN_PROC_PATHNAME nor /proc are supported
    const auto FPATH = std::filesystem::canonical("/usr/local/bin/Hyprland");
#else
    const auto FPATH = std::filesystem::canonical("/proc/self/exe");
#endif

    for (size_t i = 0; i < btSize; ++i) {
        finalCrashReport += getFormat("\t#%i | %s\n", i, btSymbols[i]);

#ifdef __clang__
        const auto CMD       = getFormat("llvm-addr2line -e %s -f 0x%lx", FPATH.c_str(), (uint64_t)bt[i]);
#else
        const auto CMD       = getFormat("addr2line -e %s -f 0x%lx", FPATH.c_str(), (uint64_t)bt[i]);
#endif
        const auto ADDR2LINE = replaceInString(execAndGet(CMD.c_str()), "\n", "\n\t\t");
        finalCrashReport += "\t\t" + ADDR2LINE.substr(0, ADDR2LINE.length() - 2);
    }

    free(btSymbols);

    const auto HOME = getenv("HOME");

    if (!HOME)
        return;

    if (!std::filesystem::exists(std::string(HOME) + "/.hyprland")) {
        std::filesystem::create_directory(std::string(HOME) + "/.hyprland");
        std::filesystem::permissions(std::string(HOME) + "/.hyprland", std::filesystem::perms::all, std::filesystem::perm_options::replace);
    }

    std::ofstream ofs(std::string(HOME) + "/.hyprland/.hyprlandCrashReport" + std::to_string(PID), std::ios::trunc);

    ofs << finalCrashReport;

    ofs.close();
}