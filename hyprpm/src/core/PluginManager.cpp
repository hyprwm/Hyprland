#include "PluginManager.hpp"
#include "../helpers/Colors.hpp"
#include "../progress/CProgressBar.hpp"
#include "Manifest.hpp"
#include "DataState.hpp"

#include <iostream>
#include <array>
#include <filesystem>
#include <thread>
#include <fstream>
#include <algorithm>
#include <format>

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>

#include <toml++/toml.hpp>

#include <hyprutils/string/String.hpp>
using namespace Hyprutils::String;

static std::string execAndGet(std::string cmd) {
    cmd += " 2>&1";
    std::array<char, 128> buffer;
    std::string           result;
    using PcloseType = int (*)(FILE*);
    const std::unique_ptr<FILE, PcloseType> pipe(popen(cmd.c_str(), "r"), static_cast<PcloseType>(pclose));
    if (!pipe)
        return "";

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

SHyprlandVersion CPluginManager::getHyprlandVersion() {
    static SHyprlandVersion ver;
    static bool             once = false;

    if (once)
        return ver;

    once                 = true;
    const auto HLVERCALL = execAndGet("hyprctl version");
    if (m_bVerbose)
        std::cout << Colors::BLUE << "[v] " << Colors::RESET << "version returned: " << HLVERCALL << "\n";

    if (!HLVERCALL.contains("Tag:")) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " You don't seem to be running Hyprland.";
        return SHyprlandVersion{};
    }

    std::string hlcommit = HLVERCALL.substr(HLVERCALL.find("at commit") + 10);
    hlcommit             = hlcommit.substr(0, hlcommit.find_first_of(' '));

    std::string hlbranch = HLVERCALL.substr(HLVERCALL.find("from branch") + 12);
    hlbranch             = hlbranch.substr(0, hlbranch.find(" at commit "));

    std::string hldate = HLVERCALL.substr(HLVERCALL.find("Date: ") + 6);
    hldate             = hldate.substr(0, hldate.find("\n"));

    std::string hlcommits;

    if (HLVERCALL.contains("commits:")) {
        hlcommits = HLVERCALL.substr(HLVERCALL.find("commits:") + 9);
        hlcommits = hlcommits.substr(0, hlcommits.find(" "));
    }

    int commits = 0;
    try {
        commits = std::stoi(hlcommits);
    } catch (...) { ; }

    if (m_bVerbose)
        std::cout << Colors::BLUE << "[v] " << Colors::RESET << "parsed commit " << hlcommit << " at branch " << hlbranch << " on " << hldate << ", commits " << commits << "\n";

    ver = SHyprlandVersion{hlbranch, hlcommit, hldate, commits};
    return ver;
}

bool CPluginManager::createSafeDirectory(const std::string& path) {
    if (path.empty() || !path.starts_with("/tmp"))
        return false;

    if (std::filesystem::exists(path))
        std::filesystem::remove_all(path);

    if (std::filesystem::exists(path))
        return false;

    if (mkdir(path.c_str(), S_IRWXU) < 0)
        return false;

    return true;
}

bool CPluginManager::addNewPluginRepo(const std::string& url, const std::string& rev) {
    const auto HLVER = getHyprlandVersion();

    if (!hasDeps()) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not clone the plugin repository. Dependencies not satisfied. Hyprpm requires: cmake, meson, cpio\n";
        return false;
    }

    if (DataState::pluginRepoExists(url)) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not clone the plugin repository. Repository already installed.\n";
        return false;
    }

    auto GLOBALSTATE = DataState::getGlobalState();
    if (!GLOBALSTATE.dontWarnInstall) {
        std::cout << Colors::YELLOW << "!" << Colors::RED << " Disclaimer:\n  " << Colors::RESET
                  << "plugins, especially not official, have no guarantee of stability, availablity or security.\n  Run them at your own risk.\n  "
                  << "This message will not appear again.\n";
        GLOBALSTATE.dontWarnInstall = true;
        DataState::updateGlobalState(GLOBALSTATE);
    }

    std::cout << Colors::GREEN << "✔" << Colors::RESET << Colors::RED << " adding a new plugin repository " << Colors::RESET << "from " << url << "\n  " << Colors::RED
              << "MAKE SURE" << Colors::RESET << " that you trust the authors. " << Colors::RED << "DO NOT" << Colors::RESET
              << " install random plugins without verifying the code and author.\n  "
              << "Are you sure? [Y/n] ";
    std::fflush(stdout);
    std::string input;
    std::getline(std::cin, input);

    if (input.size() > 0 && input[0] != 'Y' && input[0] != 'y') {
        std::cout << "Aborting.\n";
        return false;
    }

    CProgressBar progress;
    progress.m_iMaxSteps        = 5;
    progress.m_iSteps           = 0;
    progress.m_szCurrentMessage = "Cloning the plugin repository";

    progress.print();

    if (!std::filesystem::exists("/tmp/hyprpm")) {
        std::filesystem::create_directory("/tmp/hyprpm");
        std::filesystem::permissions("/tmp/hyprpm", std::filesystem::perms::all, std::filesystem::perm_options::replace);
    } else if (!std::filesystem::is_directory("/tmp/hyprpm")) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not prepare working dir for hyprpm\n";
        return false;
    }

    const std::string USERNAME = getpwuid(getuid())->pw_name;

    m_szWorkingPluginDirectory = "/tmp/hyprpm/" + USERNAME;

    if (!createSafeDirectory(m_szWorkingPluginDirectory)) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not prepare working dir for repo\n";
        return false;
    }

    progress.printMessageAbove(std::string{Colors::RESET} + " → Cloning " + url);

    std::string ret = execAndGet("cd /tmp/hyprpm && git clone --recursive " + url + " " + USERNAME);

    if (!std::filesystem::exists(m_szWorkingPluginDirectory + "/.git")) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not clone the plugin repository. shell returned:\n" << ret << "\n";
        return false;
    }

    if (!rev.empty()) {
        std::string ret = execAndGet("git -C " + m_szWorkingPluginDirectory + " reset --hard --recurse-submodules " + rev);
        if (ret.compare(0, 6, "fatal:") == 0) {
            std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not check out revision " << rev << ". shell returned:\n" << ret << "\n";
            return false;
        }
        ret = execAndGet("git -C " + m_szWorkingPluginDirectory + " submodule update --init");
        if (m_bVerbose)
            std::cout << Colors::BLUE << "[v] " << Colors::RESET << "git submodule update --init returned: " << ret << "\n";
    }

    progress.m_iSteps = 1;
    progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " cloned");
    progress.m_szCurrentMessage = "Reading the manifest";
    progress.print();

    std::unique_ptr<CManifest> pManifest;

    if (std::filesystem::exists(m_szWorkingPluginDirectory + "/hyprpm.toml")) {
        progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " found hyprpm manifest");
        pManifest = std::make_unique<CManifest>(MANIFEST_HYPRPM, m_szWorkingPluginDirectory + "/hyprpm.toml");
    } else if (std::filesystem::exists(m_szWorkingPluginDirectory + "/hyprload.toml")) {
        progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " found hyprload manifest");
        pManifest = std::make_unique<CManifest>(MANIFEST_HYPRLOAD, m_szWorkingPluginDirectory + "/hyprload.toml");
    }

    if (!pManifest) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " The provided plugin repository does not have a valid manifest\n";
        return false;
    }

    if (!pManifest->m_bGood) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " The provided plugin repository has a corrupted manifest\n";
        return false;
    }

    progress.m_iSteps = 2;
    progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " parsed manifest, found " + std::to_string(pManifest->m_vPlugins.size()) + " plugins:");
    for (auto const& pl : pManifest->m_vPlugins) {
        std::string message = std::string{Colors::RESET} + " → " + pl.name + " by ";
        for (auto const& a : pl.authors) {
            message += a + ", ";
        }
        if (pl.authors.size() > 0) {
            message.pop_back();
            message.pop_back();
        }
        message += " version " + pl.version;
        progress.printMessageAbove(message);
    }

    if (!pManifest->m_sRepository.commitPins.empty()) {
        // check commit pins

        progress.printMessageAbove(std::string{Colors::RESET} + " → Manifest has " + std::to_string(pManifest->m_sRepository.commitPins.size()) + " pins, checking");

        for (auto const& [hl, plugin] : pManifest->m_sRepository.commitPins) {
            if (hl != HLVER.hash)
                continue;

            progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " commit pin " + plugin + " matched hl, resetting");

            execAndGet("cd " + m_szWorkingPluginDirectory + " && git reset --hard --recurse-submodules " + plugin);

            ret = execAndGet("git -C " + m_szWorkingPluginDirectory + " submodule update --init");
            if (m_bVerbose)
                std::cout << Colors::BLUE << "[v] " << Colors::RESET << "git submodule update --init returned: " << ret << "\n";

            break;
        }
    }

    progress.m_szCurrentMessage = "Verifying headers";
    progress.print();

    const auto HEADERSSTATUS = headersValid();

    if (HEADERSSTATUS != HEADERS_OK) {
        std::cerr << "\n" << headerError(HEADERSSTATUS);
        return false;
    }

    progress.m_iSteps = 3;
    progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " Hyprland headers OK");
    progress.m_szCurrentMessage = "Building plugin(s)";
    progress.print();

    for (auto& p : pManifest->m_vPlugins) {
        std::string out;

        if (p.since > HLVER.commits && HLVER.commits >= 1 /* for --depth 1 clones, we can't check this. */) {
            progress.printMessageAbove(std::string{Colors::RED} + "✖" + Colors::RESET + " Not building " + p.name + ": your Hyprland version is too old.\n");
            p.failed = true;
            continue;
        }

        progress.printMessageAbove(std::string{Colors::RESET} + " → Building " + p.name);

        for (auto const& bs : p.buildSteps) {
            std::string cmd = std::format("cd {} && PKG_CONFIG_PATH=\"{}/share/pkgconfig\" {}", m_szWorkingPluginDirectory, DataState::getHeadersPath(), bs);
            out += " -> " + cmd + "\n" + execAndGet(cmd) + "\n";
        }

        if (m_bVerbose)
            std::cout << Colors::BLUE << "[v] " << Colors::RESET << "shell returned: " << out << "\n";

        if (!std::filesystem::exists(m_szWorkingPluginDirectory + "/" + p.output)) {
            progress.printMessageAbove(std::string{Colors::RED} + "✖" + Colors::RESET + " Plugin " + p.name + " failed to build.\n" +
                                       "  This likely means that the plugin is either outdated, not yet available for your version, or broken.\n  If you are on -git, update "
                                       "first.\n  Try re-running with -v to see "
                                       "more verbose output.\n");

            p.failed = true;
            continue;
        }

        progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " built " + p.name + " into " + p.output);
    }

    progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " all plugins built");
    progress.m_iSteps           = 4;
    progress.m_szCurrentMessage = "Installing repository";
    progress.print();

    // add repo toml to DataState
    SPluginRepository repo;
    std::string       repohash = execAndGet("cd " + m_szWorkingPluginDirectory + " && git rev-parse HEAD");
    if (repohash.length() > 0)
        repohash.pop_back();
    repo.name = pManifest->m_sRepository.name.empty() ? url.substr(url.find_last_of('/') + 1) : pManifest->m_sRepository.name;
    repo.url  = url;
    repo.rev  = rev;
    repo.hash = repohash;
    for (auto const& p : pManifest->m_vPlugins) {
        repo.plugins.push_back(SPlugin{p.name, m_szWorkingPluginDirectory + "/" + p.output, false, p.failed});
    }
    DataState::addNewPluginRepo(repo);

    progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " installed repository");
    progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " you can now enable the plugin(s) with hyprpm enable");
    progress.m_iSteps           = 5;
    progress.m_szCurrentMessage = "Done!";
    progress.print();

    std::cout << "\n";

    // remove build files
    std::filesystem::remove_all(m_szWorkingPluginDirectory);

    return true;
}

bool CPluginManager::removePluginRepo(const std::string& urlOrName) {
    if (!DataState::pluginRepoExists(urlOrName)) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not remove the repository. Repository is not installed.\n";
        return false;
    }

    std::cout << Colors::YELLOW << "!" << Colors::RESET << Colors::RED << " removing a plugin repository: " << Colors::RESET << urlOrName << "\n  "
              << "Are you sure? [Y/n] ";
    std::fflush(stdout);
    std::string input;
    std::getline(std::cin, input);

    if (input.size() > 0 && input[0] != 'Y' && input[0] != 'y') {
        std::cout << "Aborting.\n";
        return false;
    }

    DataState::removePluginRepo(urlOrName);

    return true;
}

eHeadersErrors CPluginManager::headersValid() {
    const auto HLVER = getHyprlandVersion();

    if (!std::filesystem::exists(DataState::getHeadersPath() + "/share/pkgconfig/hyprland.pc"))
        return HEADERS_MISSING;

    // find headers commit
    std::string cmd     = std::format("PKG_CONFIG_PATH=\"{}/share/pkgconfig\" pkgconf --cflags --keep-system-cflags hyprland", DataState::getHeadersPath());
    auto        headers = execAndGet(cmd.c_str());

    if (!headers.contains("-I/"))
        return HEADERS_MISSING;

    headers.pop_back(); // pop newline

    std::string verHeader = "";

    while (!headers.empty()) {
        const auto PATH = headers.substr(0, headers.find(" -I/", 3));

        if (headers.find(" -I/", 3) != std::string::npos)
            headers = headers.substr(headers.find("-I/", 3));
        else
            headers = "";

        if (PATH.ends_with("protocols"))
            continue;

        verHeader = trim(PATH.substr(2)) + "/hyprland/src/version.h";
        break;
    }

    if (verHeader.empty())
        return HEADERS_CORRUPTED;

    // read header
    std::ifstream ifs(verHeader);
    if (!ifs.good())
        return HEADERS_CORRUPTED;

    std::string verHeaderContent((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
    ifs.close();

    const auto HASHPOS = verHeaderContent.find("#define GIT_COMMIT_HASH");

    if (HASHPOS == std::string::npos || HASHPOS + 23 >= verHeaderContent.length())
        return HEADERS_CORRUPTED;

    std::string hash = verHeaderContent.substr(HASHPOS + 23);
    hash             = hash.substr(0, hash.find_first_of('\n'));
    hash             = hash.substr(hash.find_first_of('"') + 1);
    hash             = hash.substr(0, hash.find_first_of('"'));

    if (hash != HLVER.hash)
        return HEADERS_MISMATCHED;

    return HEADERS_OK;
}

bool CPluginManager::updateHeaders(bool force) {

    DataState::ensureStateStoreExists();

    const auto HLVER = getHyprlandVersion();

    if (!hasDeps()) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not update. Dependencies not satisfied. Hyprpm requires: cmake, meson, cpio\n";
        return false;
    }

    if (!std::filesystem::exists("/tmp/hyprpm")) {
        std::filesystem::create_directory("/tmp/hyprpm");
        std::filesystem::permissions("/tmp/hyprpm", std::filesystem::perms::all, std::filesystem::perm_options::replace);
    }

    if (!force && headersValid() == HEADERS_OK) {
        std::cout << "\n" << std::string{Colors::GREEN} + "✔" + Colors::RESET + " Headers up to date.\n";
        return true;
    }

    CProgressBar progress;
    progress.m_iMaxSteps        = 5;
    progress.m_iSteps           = 0;
    progress.m_szCurrentMessage = "Cloning the hyprland repository";
    progress.print();

    const std::string USERNAME   = getpwuid(getuid())->pw_name;
    const auto        WORKINGDIR = "/tmp/hyprpm/hyprland-" + USERNAME;

    if (!createSafeDirectory(WORKINGDIR)) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not prepare working dir for hl\n";
        return false;
    }

    progress.printMessageAbove(std::string{Colors::YELLOW} + "!" + Colors::RESET + " Cloning https://github.com/hyprwm/hyprland, this might take a moment.");

    const bool bShallow = (HLVER.branch == "main" || HLVER.branch == "") && !m_bNoShallow;

    // let us give a bit of leg-room for shallowing
    // due to timezones, etc.
    const std::string SHALLOW_DATE =
        trim(HLVER.date).empty() ? "" : execAndGet("LC_TIME=\"en_US.UTF-8\" date --date='" + HLVER.date + " - 1 weeks' '+\%a \%b \%d \%H:\%M:\%S \%Y'");

    if (m_bVerbose && bShallow)
        progress.printMessageAbove(std::string{Colors::BLUE} + "[v] " + Colors::RESET + "will shallow since: " + SHALLOW_DATE);

    std::string ret =
        execAndGet("cd /tmp/hyprpm && git clone --recursive https://github.com/hyprwm/hyprland hyprland-" + USERNAME + (bShallow ? " --shallow-since='" + SHALLOW_DATE + "'" : ""));

    if (!std::filesystem::exists(WORKINGDIR)) {
        progress.printMessageAbove(std::string{Colors::RED} + "✖" + Colors::RESET + " Clone failed. Retrying without shallow.");
        ret = execAndGet("cd /tmp/hyprpm && git clone --recursive https://github.com/hyprwm/hyprland hyprland-" + USERNAME);
    }

    if (!std::filesystem::exists(WORKINGDIR + "/.git")) {
        std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " Could not clone the hyprland repository. shell returned:\n" << ret << "\n";
        return false;
    }

    progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " cloned");
    progress.m_iSteps           = 2;
    progress.m_szCurrentMessage = "Checking out sources";
    progress.print();

    if (m_bVerbose)
        progress.printMessageAbove(std::string{Colors::BLUE} + "[v] " + Colors::RESET + "will run: " + "cd " + WORKINGDIR + " && git checkout " + HLVER.hash + " 2>&1");

    ret = execAndGet("cd " + WORKINGDIR + " && git checkout " + HLVER.hash + " 2>&1");

    if (ret.contains("fatal: unable to read tree")) {
        std::cerr << "\n"
                  << Colors::RED << "✖" << Colors::RESET
                  << " Could not checkout the running Hyprland commit. If you are on -git, try updating.\nYou can also try re-running hyprpm update with --no-shallow.\n";
        return false;
    }

    if (m_bVerbose)
        progress.printMessageAbove(std::string{Colors::BLUE} + "[v] " + Colors::RESET + "git returned (co): " + ret);

    ret = execAndGet("cd " + WORKINGDIR + " ; git rm subprojects/tracy ; git submodule update --init 2>&1 ; git reset --hard --recurse-submodules " + HLVER.hash);

    if (m_bVerbose)
        progress.printMessageAbove(std::string{Colors::BLUE} + "[v] " + Colors::RESET + "git returned (rs): " + ret);

    progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " checked out to running ver");
    progress.m_iSteps           = 3;
    progress.m_szCurrentMessage = "Building Hyprland";
    progress.print();

    progress.printMessageAbove(std::string{Colors::YELLOW} + "!" + Colors::RESET + " configuring Hyprland");

    if (m_bVerbose)
        progress.printMessageAbove(std::string{Colors::BLUE} + "[v] " + Colors::RESET + "setting PREFIX for cmake to " + DataState::getHeadersPath());

    ret = execAndGet(std::format("cd {} && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:STRING=\"{}\" -S . -B ./build -G Ninja", WORKINGDIR,
                                 DataState::getHeadersPath()));
    if (m_bVerbose)
        progress.printMessageAbove(std::string{Colors::BLUE} + "[v] " + Colors::RESET + "cmake returned: " + ret);

    if (ret.contains("CMake Error at")) {
        // missing deps, let the user know.
        std::string missing = ret.substr(ret.find("CMake Error at"));
        missing             = ret.substr(ret.find_first_of('\n') + 1);
        missing             = missing.substr(0, missing.find("-- Configuring incomplete"));
        missing             = missing.substr(0, missing.find_last_of('\n'));

        std::cerr << "\n"
                  << Colors::RED << "✖" << Colors::RESET << " Could not configure the hyprland source, cmake complained:\n"
                  << missing << "\n\nThis likely means that you are missing the above dependencies or they are out of date.\n";
        return false;
    }

    progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " configured Hyprland");
    progress.m_iSteps           = 4;
    progress.m_szCurrentMessage = "Installing sources";
    progress.print();

    std::string cmd =
        std::format("sed -i -e \"s#PREFIX = /usr/local#PREFIX = {}#\" {}/Makefile && cd {} && make installheaders", DataState::getHeadersPath(), WORKINGDIR, WORKINGDIR);
    if (m_bVerbose)
        progress.printMessageAbove(std::string{Colors::BLUE} + "[v] " + Colors::RESET + "installation will run: " + cmd);

    ret = execAndGet(cmd);

    if (m_bVerbose)
        std::cout << Colors::BLUE << "[v] " << Colors::RESET << "installer returned: " << ret << "\n";

    // remove build files
    std::filesystem::remove_all(WORKINGDIR);

    auto HEADERSVALID = headersValid();
    if (HEADERSVALID == HEADERS_OK) {
        progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " installed headers");
        progress.m_iSteps           = 5;
        progress.m_szCurrentMessage = "Done!";
        progress.print();

        std::cout << "\n";
    } else {
        progress.printMessageAbove(std::string{Colors::RED} + "✖" + Colors::RESET + " failed to install headers with error code " + std::to_string((int)HEADERSVALID) + " (" +
                                   headerErrorShort(HEADERSVALID) + ")");
        progress.m_iSteps           = 5;
        progress.m_szCurrentMessage = "Failed";
        progress.print();

        std::cout << "\n";

        std::cerr << "\n" << headerError(HEADERSVALID);

        return false;
    }

    return true;
}

bool CPluginManager::updatePlugins(bool forceUpdateAll) {
    if (headersValid() != HEADERS_OK) {
        std::cout << "\n" << std::string{Colors::RED} + "✖" + Colors::RESET + " headers are not up-to-date, please run hyprpm update.\n";
        return false;
    }

    const auto REPOS = DataState::getAllRepositories();

    if (REPOS.size() < 1) {
        std::cout << "\n" << std::string{Colors::RED} + "✖" + Colors::RESET + " No repos to update.\n";
        return true;
    }

    const auto   HLVER = getHyprlandVersion();

    CProgressBar progress;
    progress.m_iMaxSteps        = REPOS.size() * 2 + 2;
    progress.m_iSteps           = 0;
    progress.m_szCurrentMessage = "Updating repositories";
    progress.print();

    const std::string USERNAME = getpwuid(getuid())->pw_name;
    m_szWorkingPluginDirectory = "/tmp/hyprpm/" + USERNAME;

    for (auto const& repo : REPOS) {
        bool update = forceUpdateAll;

        progress.m_iSteps++;
        progress.m_szCurrentMessage = "Updating " + repo.name;
        progress.print();

        progress.printMessageAbove(std::string{Colors::RESET} + " → checking for updates for " + repo.name);

        createSafeDirectory(m_szWorkingPluginDirectory);

        progress.printMessageAbove(std::string{Colors::RESET} + " → Cloning " + repo.url);

        std::string ret = execAndGet("cd /tmp/hyprpm && git clone --recursive " + repo.url + " " + USERNAME);

        if (!std::filesystem::exists(m_szWorkingPluginDirectory + "/.git")) {
            std::cout << "\n" << std::string{Colors::RED} + "✖" + Colors::RESET + " could not clone repo: shell returned:\n" + ret;
            return false;
        }

        if (!repo.rev.empty()) {
            progress.printMessageAbove(std::string{Colors::RESET} + " → Plugin has revision set, resetting: " + repo.rev);

            std::string ret = execAndGet("git -C " + m_szWorkingPluginDirectory + " reset --hard --recurse-submodules " + repo.rev);
            if (ret.compare(0, 6, "fatal:") == 0) {
                std::cout << "\n" << std::string{Colors::RED} + "✖" + Colors::RESET + " could not check out revision " + repo.rev + ": shell returned:\n" + ret;
                return false;
            }
        }

        if (!update) {
            // check if git has updates
            std::string hash = execAndGet("cd " + m_szWorkingPluginDirectory + " && git rev-parse HEAD");
            if (!hash.empty())
                hash.pop_back();

            update = update || hash != repo.hash;
        }

        if (!update) {
            std::filesystem::remove_all(m_szWorkingPluginDirectory);
            progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " repository " + repo.name + " is up-to-date.");
            progress.m_iSteps++;
            progress.print();
            continue;
        }

        // we need to update

        progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " repository " + repo.name + " has updates.");
        progress.printMessageAbove(std::string{Colors::RESET} + " → Building " + repo.name);
        progress.m_iSteps++;
        progress.print();

        std::unique_ptr<CManifest> pManifest;

        if (std::filesystem::exists(m_szWorkingPluginDirectory + "/hyprpm.toml")) {
            progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " found hyprpm manifest");
            pManifest = std::make_unique<CManifest>(MANIFEST_HYPRPM, m_szWorkingPluginDirectory + "/hyprpm.toml");
        } else if (std::filesystem::exists(m_szWorkingPluginDirectory + "/hyprload.toml")) {
            progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " found hyprload manifest");
            pManifest = std::make_unique<CManifest>(MANIFEST_HYPRLOAD, m_szWorkingPluginDirectory + "/hyprload.toml");
        }

        if (!pManifest) {
            std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " The provided plugin repository does not have a valid manifest\n";
            continue;
        }

        if (!pManifest->m_bGood) {
            std::cerr << "\n" << Colors::RED << "✖" << Colors::RESET << " The provided plugin repository has a corrupted manifest\n";
            continue;
        }

        if (repo.rev.empty() && !pManifest->m_sRepository.commitPins.empty()) {
            // check commit pins unless a revision is specified

            progress.printMessageAbove(std::string{Colors::RESET} + " → Manifest has " + std::to_string(pManifest->m_sRepository.commitPins.size()) + " pins, checking");

            for (auto const& [hl, plugin] : pManifest->m_sRepository.commitPins) {
                if (hl != HLVER.hash)
                    continue;

                progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " commit pin " + plugin + " matched hl, resetting");

                execAndGet("cd " + m_szWorkingPluginDirectory + " && git reset --hard --recurse-submodules " + plugin);
            }
        }

        for (auto& p : pManifest->m_vPlugins) {
            std::string out;

            if (p.since > HLVER.commits && HLVER.commits >= 1000 /* for shallow clones, we can't check this. 1000 is an arbitrary number I chose. */) {
                progress.printMessageAbove(std::string{Colors::RED} + "✖" + Colors::RESET + " Not building " + p.name + ": your Hyprland version is too old.\n");
                p.failed = true;
                continue;
            }

            progress.printMessageAbove(std::string{Colors::RESET} + " → Building " + p.name);

            for (auto const& bs : p.buildSteps) {
                std::string cmd = std::format("cd {} && PKG_CONFIG_PATH=\"{}/share/pkgconfig\" {}", m_szWorkingPluginDirectory, DataState::getHeadersPath(), bs);
                out += " -> " + cmd + "\n" + execAndGet(cmd) + "\n";
            }

            if (m_bVerbose)
                std::cout << Colors::BLUE << "[v] " << Colors::RESET << "shell returned: " << out << "\n";

            if (!std::filesystem::exists(m_szWorkingPluginDirectory + "/" + p.output)) {
                std::cerr << "\n"
                          << Colors::RED << "✖" << Colors::RESET << " Plugin " << p.name << " failed to build.\n"
                          << "  This likely means that the plugin is either outdated, not yet available for your version, or broken.\n  If you are on -git, update first.\n  Try "
                             "re-running with -v to see more verbose "
                             "output.\n";
                p.failed = true;
                continue;
            }

            progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " built " + p.name + " into " + p.output);
        }

        // add repo toml to DataState
        SPluginRepository newrepo = repo;
        newrepo.plugins.clear();
        execAndGet("cd " + m_szWorkingPluginDirectory +
                   " && git pull --recurse-submodules && git reset --hard --recurse-submodules"); // repo hash in the state.toml has to match head and not any pin
        std::string repohash = execAndGet("cd " + m_szWorkingPluginDirectory + " && git rev-parse HEAD");
        if (repohash.length() > 0)
            repohash.pop_back();
        newrepo.hash = repohash;
        for (auto const& p : pManifest->m_vPlugins) {
            const auto OLDPLUGINIT = std::find_if(repo.plugins.begin(), repo.plugins.end(), [&](const auto& other) { return other.name == p.name; });
            newrepo.plugins.push_back(SPlugin{p.name, m_szWorkingPluginDirectory + "/" + p.output, OLDPLUGINIT != repo.plugins.end() ? OLDPLUGINIT->enabled : false});
        }
        DataState::removePluginRepo(newrepo.name);
        DataState::addNewPluginRepo(newrepo);

        std::filesystem::remove_all(m_szWorkingPluginDirectory);

        progress.printMessageAbove(std::string{Colors::GREEN} + "✔" + Colors::RESET + " updated " + repo.name);
    }

    progress.m_iSteps++;
    progress.m_szCurrentMessage = "Updating global state...";
    progress.print();

    auto GLOBALSTATE                = DataState::getGlobalState();
    GLOBALSTATE.headersHashCompiled = HLVER.hash;
    DataState::updateGlobalState(GLOBALSTATE);

    progress.m_iSteps++;
    progress.m_szCurrentMessage = "Done!";
    progress.print();

    std::cout << "\n";

    return true;
}

bool CPluginManager::enablePlugin(const std::string& name) {
    bool ret = DataState::setPluginEnabled(name, true);
    if (ret)
        std::cout << Colors::GREEN << "✔" << Colors::RESET << " Enabled " << name << "\n";
    return ret;
}

bool CPluginManager::disablePlugin(const std::string& name) {
    bool ret = DataState::setPluginEnabled(name, false);
    if (ret)
        std::cout << Colors::GREEN << "✔" << Colors::RESET << " Disabled " << name << "\n";
    return ret;
}

ePluginLoadStateReturn CPluginManager::ensurePluginsLoadState() {
    if (headersValid() != HEADERS_OK) {
        std::cerr << "\n" << std::string{Colors::RED} + "✖" + Colors::RESET + " headers are not up-to-date, please run hyprpm update.\n";
        return LOADSTATE_HEADERS_OUTDATED;
    }

    const auto HOME = getenv("HOME");
    const auto HIS  = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!HOME || !HIS) {
        std::cerr << "PluginManager: no $HOME or HIS\n";
        return LOADSTATE_FAIL;
    }
    const auto               HYPRPMPATH = DataState::getDataStatePath() + "/";

    auto                     pluginLines = execAndGet("hyprctl plugins list | grep Plugin");

    std::vector<std::string> loadedPlugins;

    std::cout << Colors::GREEN << "✔" << Colors::RESET << " Ensuring plugin load state\n";

    // iterate line by line
    while (!pluginLines.empty()) {
        auto plLine = pluginLines.substr(0, pluginLines.find("\n"));

        if (pluginLines.find("\n") != std::string::npos)
            pluginLines = pluginLines.substr(pluginLines.find("\n") + 1);
        else
            pluginLines = "";

        if (plLine.back() != ':')
            continue;

        plLine = plLine.substr(7);
        plLine = plLine.substr(0, plLine.find(" by "));

        loadedPlugins.push_back(plLine);
    }

    // get state
    const auto REPOS = DataState::getAllRepositories();

    auto       enabled = [REPOS](const std::string& plugin) -> bool {
        for (auto const& r : REPOS) {
            for (auto const& p : r.plugins) {
                if (p.name == plugin && p.enabled)
                    return true;
            }
        }

        return false;
    };

    auto repoForName = [REPOS](const std::string& name) -> std::string {
        for (auto const& r : REPOS) {
            for (auto const& p : r.plugins) {
                if (p.name == name)
                    return r.name;
            }
        }

        return "";
    };

    // unload disabled plugins
    for (auto const& p : loadedPlugins) {
        if (!enabled(p)) {
            // unload
            loadUnloadPlugin(HYPRPMPATH + repoForName(p) + "/" + p + ".so", false);
            std::cout << Colors::GREEN << "✔" << Colors::RESET << " Unloaded " << p << "\n";
        }
    }

    // load enabled plugins
    for (auto const& r : REPOS) {
        for (auto const& p : r.plugins) {
            if (!p.enabled)
                continue;

            if (std::find_if(loadedPlugins.begin(), loadedPlugins.end(), [&](const auto& other) { return other == p.name; }) != loadedPlugins.end())
                continue;

            loadUnloadPlugin(HYPRPMPATH + repoForName(p.name) + "/" + p.filename, true);
            std::cout << Colors::GREEN << "✔" << Colors::RESET << " Loaded " << p.name << "\n";
        }
    }

    std::cout << Colors::GREEN << "✔" << Colors::RESET << " Plugin load state ensured\n";

    return LOADSTATE_OK;
}

bool CPluginManager::loadUnloadPlugin(const std::string& path, bool load) {
    if (load)
        execAndGet("hyprctl plugin load " + path);
    else
        execAndGet("hyprctl plugin unload " + path);

    return true;
}

void CPluginManager::listAllPlugins() {
    const auto REPOS = DataState::getAllRepositories();

    for (auto const& r : REPOS) {
        std::cout << std::string{Colors::RESET} + " → Repository " + r.name + ":\n";

        for (auto const& p : r.plugins) {

            std::cout << std::string{Colors::RESET} + "  │ Plugin " + p.name;

            if (!p.failed)
                std::cout << "\n  └─ enabled: " << (p.enabled ? Colors::GREEN : Colors::RED) << (p.enabled ? "true" : "false") << Colors::RESET << "\n";
            else
                std::cout << "\n  └─ enabled: " << Colors::RED << "Plugin failed to build" << Colors::RESET << "\n";
        }
    }
}

void CPluginManager::notify(const eNotifyIcons icon, uint32_t color, int durationMs, const std::string& message) {
    execAndGet("hyprctl notify " + std::to_string((int)icon) + " " + std::to_string(durationMs) + " " + std::to_string(color) + " " + message);
}

std::string CPluginManager::headerError(const eHeadersErrors err) {
    switch (err) {
        case HEADERS_CORRUPTED: return std::string{Colors::RED} + "✖" + Colors::RESET + " Headers corrupted. Please run hyprpm update to fix those.\n";
        case HEADERS_MISMATCHED: return std::string{Colors::RED} + "✖" + Colors::RESET + " Headers version mismatch. Please run hyprpm update to fix those.\n";
        case HEADERS_NOT_HYPRLAND: return std::string{Colors::RED} + "✖" + Colors::RESET + " It doesn't seem you are running on hyprland.\n";
        case HEADERS_MISSING: return std::string{Colors::RED} + "✖" + Colors::RESET + " Headers missing. Please run hyprpm update to fix those.\n";
        case HEADERS_DUPLICATED: {
            return std::string{Colors::RED} + "✖" + Colors::RESET + " Headers duplicated!!! This is a very bad sign.\n" +
                " This could be due to e.g. installing hyprland manually while a system package of hyprland is also installed.\n" +
                " If the above doesn't apply, check your /usr/include and /usr/local/include directories\n and remove all the hyprland headers.\n";
        }
        default: break;
    }

    return std::string{Colors::RED} + "✖" + Colors::RESET + " Unknown header error. Please run hyprpm update to fix those.\n";
}

std::string CPluginManager::headerErrorShort(const eHeadersErrors err) {
    switch (err) {
        case HEADERS_CORRUPTED: return "Headers corrupted";
        case HEADERS_MISMATCHED: return "Headers version mismatched";
        case HEADERS_NOT_HYPRLAND: return "Not running on Hyprland";
        case HEADERS_MISSING: return "Headers missing";
        case HEADERS_DUPLICATED: return "Headers duplicated";
        default: break;
    }
    return "?";
}

bool CPluginManager::hasDeps() {
    std::vector<std::string> deps = {"meson", "cpio", "cmake", "pkg-config"};
    for (auto const& d : deps) {
        if (!execAndGet("command -v " + d).contains("/"))
            return false;
    }

    return true;
}
