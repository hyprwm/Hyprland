#include "CrashReporter.hpp"
#include <random>
#include <sys/utsname.h>
#include <execinfo.h>
#include <fstream>

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

    const std::string GPUINFO = execAndGet("lspci -vnn | grep VGA");

    finalCrashReport += "GPU:\n\t" + GPUINFO;

    finalCrashReport += getFormat("\n\nos-release:\n\t%s\n\n\n", replaceInString(execAndGet("cat /etc/os-release"), "\n", "\n\t").c_str());

    finalCrashReport += "Backtrace:\n";

    void*  bt[1024];
    size_t btSize;
    char** btSymbols;

    btSize    = backtrace(bt, 1024);
    btSymbols = backtrace_symbols(bt, btSize);

    for (size_t i = 0; i < btSize; ++i) {
        finalCrashReport += getFormat("\t#%i | %s\n", i, btSymbols[i]);

        std::string btSymbol = btSymbols[i];
        size_t      hlPos    = 0;
        while (btSymbol.find("Hyprland", hlPos + 1) != std::string::npos) {
            hlPos = btSymbol.find("Hyprland", hlPos + 1);
        }

        if (hlPos != 0) {
            const auto CMD       = getFormat("addr2line -e %s -f 0x%lx", btSymbol.substr(0, hlPos + 8).c_str(), (uint64_t)bt[i]);
            const auto ADDR2LINE = replaceInString(execAndGet(CMD.c_str()), "\n", "\n\t\t");
            finalCrashReport += "\t\t" + ADDR2LINE.substr(0, ADDR2LINE.length() - 2);
        }
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