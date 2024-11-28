#include "PluginManager.hpp"
#include "../helpers/Colors.hpp"
#include "../helpers/StringUtils.hpp"
#include "../progress/CProgressBar.hpp"
#include "Manifest.hpp"
#include "DataState.hpp"

#include <cstdio>
#include <iostream>
#include <array>
#include <filesystem>
#include <print>
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
#include <hyprutils/os/Process.hpp>
using namespace Hyprutils::String;
using namespace Hyprutils::OS;

static std::string execAndGet(std::string cmd) {
    cmd += " 2>&1";

    CProcess proc("/bin/sh", {"-c", cmd});

    if (!proc.runSync())
        return "error";

    return proc.stdOut();
}

static std::string getTempRoot() {
    static auto ENV = getenv("XDG_RUNTIME_DIR");
    if (!ENV) {
        std::cerr << "\nERROR: XDG_RUNTIME_DIR not set!\n";
        exit(1);
    }

    const auto STR = ENV + std::string{"/hyprpm/"};

    return STR;
}

SHyprlandVersion CPluginManager::getHyprlandVersion() {
    static SHyprlandVersion ver;
    static bool             once = false;

    if (once)
        return ver;

    once                 = true;
    const auto HLVERCALL = execAndGet("hyprctl version");
    if (m_bVerbose)
        std::println("{}", verboseString("version returned: {}", HLVERCALL));

    if (!HLVERCALL.contains("Tag:")) {
        std::println(stderr, "\n{}", failureString("You don't seem to be running Hyprland."));
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
        std::println("{}", verboseString("parsed commit {} at branch {} on {}, commits {}", hlcommit, hlbranch, hldate, commits));

    ver = SHyprlandVersion{hlbranch, hlcommit, hldate, commits};
    return ver;
}

bool CPluginManager::createSafeDirectory(const std::string& path) {
    if (path.empty() || !path.starts_with(getTempRoot()))
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
        std::println(stderr, "\n{}", failureString("Could not clone the plugin repository. Dependencies not satisfied. Hyprpm requires: cmake, meson, cpio, pkg-config"));
        return false;
    }

    if (DataState::pluginRepoExists(url)) {
        std::println(stderr, "\n{}", failureString("Could not clone the plugin repository. Repository already installed."));
        return false;
    }

    auto GLOBALSTATE = DataState::getGlobalState();
    if (!GLOBALSTATE.dontWarnInstall) {
        std::println("{}!{} Disclaimer: {}", Colors::YELLOW, Colors::RED, Colors::RESET);
        std::println("plugins, especially not official, have no guarantee of stability, availablity or security.\n"
                     "Run them at your own risk.\n"
                     "This message will not appear again.");
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
        std::println(stderr, "Aborting.");
        return false;
    }

    CProgressBar progress;
    progress.m_iMaxSteps        = 5;
    progress.m_iSteps           = 0;
    progress.m_szCurrentMessage = "Cloning the plugin repository";

    progress.print();

    if (!std::filesystem::exists(getTempRoot())) {
        std::filesystem::create_directory(getTempRoot());
        std::filesystem::permissions(getTempRoot(), std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
    } else if (!std::filesystem::is_directory(getTempRoot())) {
        std::println(stderr, "\n{}", failureString("Could not prepare working dir for hyprpm"));
        return false;
    }

    const std::string USERNAME = getpwuid(getuid())->pw_name;

    m_szWorkingPluginDirectory = getTempRoot() + USERNAME;

    if (!createSafeDirectory(m_szWorkingPluginDirectory)) {
        std::println(stderr, "\n{}", failureString("Could not prepare working dir for repo"));
        return false;
    }

    progress.printMessageAbove(infoString("Cloning {}", url));

    std::string ret = execAndGet(std::format("cd {} && git clone --recursive {} {}", getTempRoot(), url, USERNAME));

    if (!std::filesystem::exists(m_szWorkingPluginDirectory + "/.git")) {
        std::println(stderr, "\n{}", failureString("Could not clone the plugin repository. shell returned:\n{}", ret));
        return false;
    }

    if (!rev.empty()) {
        std::string ret = execAndGet("git -C " + m_szWorkingPluginDirectory + " reset --hard --recurse-submodules " + rev);
        if (ret.compare(0, 6, "fatal:") == 0) {
            std::println(stderr, "\n{}", failureString("Could not check out revision {}. shell returned:\n{}", rev, ret));
            return false;
        }
        ret = execAndGet("git -C " + m_szWorkingPluginDirectory + " submodule update --init");
        if (m_bVerbose)
            std::println("{}", verboseString("git submodule update --init returned: {}", ret));
    }

    progress.m_iSteps = 1;
    progress.printMessageAbove(successString("cloned"));
    progress.m_szCurrentMessage = "Reading the manifest";
    progress.print();

    std::unique_ptr<CManifest> pManifest;

    if (std::filesystem::exists(m_szWorkingPluginDirectory + "/hyprpm.toml")) {
        progress.printMessageAbove(successString("found hyprpm manifest"));
        pManifest = std::make_unique<CManifest>(MANIFEST_HYPRPM, m_szWorkingPluginDirectory + "/hyprpm.toml");
    } else if (std::filesystem::exists(m_szWorkingPluginDirectory + "/hyprload.toml")) {
        progress.printMessageAbove(successString("found hyprload manifest"));
        pManifest = std::make_unique<CManifest>(MANIFEST_HYPRLOAD, m_szWorkingPluginDirectory + "/hyprload.toml");
    }

    if (!pManifest) {
        std::println(stderr, "\n{}", failureString("The provided plugin repository does not have a valid manifest"));
        return false;
    }

    if (!pManifest->m_bGood) {
        std::println(stderr, "\n{}", failureString("The provided plugin repository has a corrupted manifest"));
        return false;
    }

    progress.m_iSteps = 2;
    progress.printMessageAbove(successString("parsed manifest, found " + std::to_string(pManifest->m_vPlugins.size()) + " plugins:"));
    for (auto const& pl : pManifest->m_vPlugins) {
        std::string message = "→ " + pl.name + " by ";
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

        progress.printMessageAbove(infoString("Manifest has {} pins, checking", pManifest->m_sRepository.commitPins.size()));

        for (auto const& [hl, plugin] : pManifest->m_sRepository.commitPins) {
            if (hl != HLVER.hash)
                continue;

            progress.printMessageAbove(successString("commit pin {} matched hl, resetting", plugin));

            execAndGet("cd " + m_szWorkingPluginDirectory + " && git reset --hard --recurse-submodules " + plugin);

            ret = execAndGet("git -C " + m_szWorkingPluginDirectory + " submodule update --init");
            if (m_bVerbose)
                std::println("{}", verboseString("git submodule update --init returned: {}", ret));

            break;
        }
    }

    progress.m_szCurrentMessage = "Verifying headers";
    progress.print();

    const auto HEADERSSTATUS = headersValid();

    if (HEADERSSTATUS != HEADERS_OK) {
        std::println("\n{}", headerError(HEADERSSTATUS));
        return false;
    }

    progress.m_iSteps = 3;
    progress.printMessageAbove(successString("Hyprland headers OK"));
    progress.m_szCurrentMessage = "Building plugin(s)";
    progress.print();

    for (auto& p : pManifest->m_vPlugins) {
        std::string out;

        if (p.since > HLVER.commits && HLVER.commits >= 1 /* for --depth 1 clones, we can't check this. */) {
            progress.printMessageAbove(failureString("Not building {}: your Hyprland version is too old.\n", p.name));
            p.failed = true;
            continue;
        }

        progress.printMessageAbove(infoString("Building {}", p.name));

        for (auto const& bs : p.buildSteps) {
            const std::string& cmd = std::format("cd {} && PKG_CONFIG_PATH=\"{}/share/pkgconfig\" {}", m_szWorkingPluginDirectory, DataState::getHeadersPath(), bs);
            out += " -> " + cmd + "\n" + execAndGet(cmd) + "\n";
        }

        if (m_bVerbose)
            std::println("{}", verboseString("shell returned: {}", out));

        if (!std::filesystem::exists(m_szWorkingPluginDirectory + "/" + p.output)) {
            progress.printMessageAbove(failureString("Plugin {} failed to build.\n"
                                                     "  This likely means that the plugin is either outdated, not yet available for your version, or broken.\n"
                                                     "  If you are on -git, update first\n"
                                                     "  Try re-running with -v to see more verbose output.\n",
                                                     p.name));

            p.failed = true;
            continue;
        }

        progress.printMessageAbove(successString("built {} into {}", p.name, p.output));
    }

    progress.printMessageAbove(successString("all plugins built"));
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

    progress.printMessageAbove(successString("installed repository"));
    progress.printMessageAbove(successString("you can now enable the plugin(s) with hyprpm enable"));
    progress.m_iSteps           = 5;
    progress.m_szCurrentMessage = "Done!";
    progress.print();

    std::print("\n");

    // remove build files
    std::filesystem::remove_all(m_szWorkingPluginDirectory);

    return true;
}

bool CPluginManager::removePluginRepo(const std::string& urlOrName) {
    if (!DataState::pluginRepoExists(urlOrName)) {
        std::println(stderr, "\n{}", failureString("Could not remove the repository. Repository is not installed."));
        return false;
    }

    std::cout << Colors::YELLOW << "!" << Colors::RESET << Colors::RED << " removing a plugin repository: " << Colors::RESET << urlOrName << "\n  "
              << "Are you sure? [Y/n] ";
    std::fflush(stdout);
    std::string input;
    std::getline(std::cin, input);

    if (input.size() > 0 && input[0] != 'Y' && input[0] != 'y') {
        std::println("Aborting.");
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
    const std::string& cmd     = std::format("PKG_CONFIG_PATH=\"{}/share/pkgconfig\" pkgconf --cflags --keep-system-cflags hyprland", DataState::getHeadersPath());
    auto               headers = execAndGet(cmd.c_str());

    if (!headers.contains("-I/"))
        return HEADERS_MISSING;

    headers.pop_back(); // pop newline

    std::string verHeader;

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
        std::println("\n{}", failureString("Could not update. Dependencies not satisfied. Hyprpm requires: cmake, meson, cpio, pkg-config"));
        return false;
    }

    if (!std::filesystem::exists(getTempRoot())) {
        std::filesystem::create_directory(getTempRoot());
        std::filesystem::permissions(getTempRoot(), std::filesystem::perms::owner_all, std::filesystem::perm_options::replace);
    }

    if (!force && headersValid() == HEADERS_OK) {
        std::println("\n{}", successString("Headers up to date."));
        return true;
    }

    CProgressBar progress;
    progress.m_iMaxSteps        = 5;
    progress.m_iSteps           = 0;
    progress.m_szCurrentMessage = "Cloning the hyprland repository";
    progress.print();

    const std::string USERNAME   = getpwuid(getuid())->pw_name;
    const auto        WORKINGDIR = getTempRoot() + "hyprland-" + USERNAME;

    if (!createSafeDirectory(WORKINGDIR)) {
        std::println("\n{}", failureString("Could not prepare working dir for hl"));
        return false;
    }

    progress.printMessageAbove(statusString("!", Colors::YELLOW, "Cloning https://github.com/hyprwm/Hyprland, this might take a moment."));

    const bool bShallow = (HLVER.branch == "main") && !m_bNoShallow;

    // let us give a bit of leg-room for shallowing
    // due to timezones, etc.
    const std::string SHALLOW_DATE = trim(HLVER.date).empty() ? "" : execAndGet("LC_TIME=\"en_US.UTF-8\" date --date='" + HLVER.date + " - 1 weeks' '+%a %b %d %H:%M:%S %Y'");

    if (m_bVerbose && bShallow)
        progress.printMessageAbove(verboseString("will shallow since: {}", SHALLOW_DATE));

    std::string ret = execAndGet(std::format("cd {} && git clone --recursive https://github.com/hyprwm/Hyprland hyprland-{}{}", getTempRoot(), USERNAME,
                                             (bShallow ? " --shallow-since='" + SHALLOW_DATE + "'" : "")));

    if (!std::filesystem::exists(WORKINGDIR)) {
        progress.printMessageAbove(failureString("Clone failed. Retrying without shallow."));
        ret = execAndGet(std::format("cd {} && git clone --recursive https://github.com/hyprwm/hyprland hyprland-{}", getTempRoot(), USERNAME));
    }

    if (!std::filesystem::exists(WORKINGDIR + "/.git")) {
        std::println(stderr, "\n{}", failureString("Could not clone the Hyprland repository. shell returned:\n{}", ret));
        return false;
    }

    progress.printMessageAbove(successString("Hyprland cloned"));
    progress.m_iSteps           = 2;
    progress.m_szCurrentMessage = "Checking out sources";
    progress.print();

    if (m_bVerbose)
        progress.printMessageAbove(verboseString("will run: cd {} && git checkout {} 2>&1", WORKINGDIR, HLVER.hash));

    ret = execAndGet("cd " + WORKINGDIR + " && git checkout " + HLVER.hash + " 2>&1");

    if (ret.contains("fatal: unable to read tree")) {
        std::println(stderr, "\n{}",
                     failureString("Could not checkout the running Hyprland commit. If you are on -git, try updating.\n"
                                   "You can also try re-running hyprpm update with --no-shallow."));
        return false;
    }

    if (m_bVerbose)
        progress.printMessageAbove(verboseString("git returned (co): {}", ret));

    ret = execAndGet("cd " + WORKINGDIR + " ; git rm subprojects/tracy ; git submodule update --init 2>&1 ; git reset --hard --recurse-submodules " + HLVER.hash);

    if (m_bVerbose)
        progress.printMessageAbove(verboseString("git returned (rs): {}", ret));

    progress.printMessageAbove(successString("checked out to running ver"));
    progress.m_iSteps           = 3;
    progress.m_szCurrentMessage = "Building Hyprland";
    progress.print();

    progress.printMessageAbove(statusString("!", Colors::YELLOW, "configuring Hyprland"));

    if (m_bVerbose)
        progress.printMessageAbove(verboseString("setting PREFIX for cmake to {}", DataState::getHeadersPath()));

    ret = execAndGet(std::format("cd {} && cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release -DCMAKE_INSTALL_PREFIX:STRING=\"{}\" -S . -B ./build -G Ninja", WORKINGDIR,
                                 DataState::getHeadersPath()));
    if (m_bVerbose)
        progress.printMessageAbove(verboseString("cmake returned: {}", ret));

    if (ret.contains("CMake Error at")) {
        // missing deps, let the user know.
        std::string missing = ret.substr(ret.find("CMake Error at"));
        missing             = ret.substr(ret.find_first_of('\n') + 1);
        missing             = missing.substr(0, missing.find("-- Configuring incomplete"));
        missing             = missing.substr(0, missing.find_last_of('\n'));

        std::println(stderr, "\n{}",
                     failureString("Could not configure the hyprland source, cmake complained:\n{}\n\n"
                                   "This likely means that you are missing the above dependencies or they are out of date.",
                                   missing));
        return false;
    }

    progress.printMessageAbove(successString("configured Hyprland"));
    progress.m_iSteps           = 4;
    progress.m_szCurrentMessage = "Installing sources";
    progress.print();

    const std::string& cmd =
        std::format("sed -i -e \"s#PREFIX = /usr/local#PREFIX = {}#\" {}/Makefile && cd {} && make installheaders", DataState::getHeadersPath(), WORKINGDIR, WORKINGDIR);
    if (m_bVerbose)
        progress.printMessageAbove(verboseString("installation will run: {}", cmd));

    ret = execAndGet(cmd);

    if (m_bVerbose)
        std::println("{}", verboseString("installer returned: {}", ret));

    // remove build files
    std::filesystem::remove_all(WORKINGDIR);

    auto HEADERSVALID = headersValid();
    if (HEADERSVALID == HEADERS_OK) {
        progress.printMessageAbove(successString("installed headers"));
        progress.m_iSteps           = 5;
        progress.m_szCurrentMessage = "Done!";
        progress.print();

        std::print("\n");
    } else {
        progress.printMessageAbove(failureString("failed to install headers with error code {} ({})", (int)HEADERSVALID, headerErrorShort(HEADERSVALID)));
        progress.m_iSteps           = 5;
        progress.m_szCurrentMessage = "Failed";
        progress.print();

        std::print(stderr, "\n\n{}", headerError(HEADERSVALID));

        return false;
    }

    return true;
}

bool CPluginManager::updatePlugins(bool forceUpdateAll) {
    if (headersValid() != HEADERS_OK) {
        std::println("{}", failureString("headers are not up-to-date, please run hyprpm update."));
        return false;
    }

    const auto REPOS = DataState::getAllRepositories();

    if (REPOS.size() < 1) {
        std::println("{}", failureString("No repos to update."));
        return true;
    }

    const auto   HLVER = getHyprlandVersion();

    CProgressBar progress;
    progress.m_iMaxSteps        = REPOS.size() * 2 + 2;
    progress.m_iSteps           = 0;
    progress.m_szCurrentMessage = "Updating repositories";
    progress.print();

    const std::string USERNAME = getpwuid(getuid())->pw_name;
    m_szWorkingPluginDirectory = getTempRoot() + USERNAME;

    for (auto const& repo : REPOS) {
        bool update = forceUpdateAll;

        progress.m_iSteps++;
        progress.m_szCurrentMessage = "Updating " + repo.name;
        progress.print();

        progress.printMessageAbove(infoString("checking for updates for {}", repo.name));

        createSafeDirectory(m_szWorkingPluginDirectory);

        progress.printMessageAbove(infoString("Cloning {}", repo.url));

        std::string ret = execAndGet(std::format("cd {} && git clone --recursive {} {}", getTempRoot(), repo.url, USERNAME));

        if (!std::filesystem::exists(m_szWorkingPluginDirectory + "/.git")) {
            std::println("{}", failureString("could not clone repo: shell returned: {}", ret));
            return false;
        }

        if (!repo.rev.empty()) {
            progress.printMessageAbove(infoString("Plugin has revision set, resetting: {}", repo.rev));

            std::string ret = execAndGet("git -C " + m_szWorkingPluginDirectory + " reset --hard --recurse-submodules " + repo.rev);
            if (ret.compare(0, 6, "fatal:") == 0) {
                std::println(stderr, "\n{}", failureString("could not check out revision {}: shell returned:\n{}", repo.rev, ret));

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
            progress.printMessageAbove(successString("repository {} is up-to-date.", repo.name));
            progress.m_iSteps++;
            progress.print();
            continue;
        }

        // we need to update

        progress.printMessageAbove(successString("repository {} has updates.", repo.name));
        progress.printMessageAbove(infoString("Building {}", repo.name));
        progress.m_iSteps++;
        progress.print();

        std::unique_ptr<CManifest> pManifest;

        if (std::filesystem::exists(m_szWorkingPluginDirectory + "/hyprpm.toml")) {
            progress.printMessageAbove(successString("found hyprpm manifest"));
            pManifest = std::make_unique<CManifest>(MANIFEST_HYPRPM, m_szWorkingPluginDirectory + "/hyprpm.toml");
        } else if (std::filesystem::exists(m_szWorkingPluginDirectory + "/hyprload.toml")) {
            progress.printMessageAbove(successString("found hyprload manifest"));
            pManifest = std::make_unique<CManifest>(MANIFEST_HYPRLOAD, m_szWorkingPluginDirectory + "/hyprload.toml");
        }

        if (!pManifest) {
            std::println(stderr, "\n{}", failureString("The provided plugin repository does not have a valid manifest"));
            continue;
        }

        if (!pManifest->m_bGood) {
            std::println(stderr, "\n{}", failureString("The provided plugin repository has a corrupted manifest"));
            continue;
        }

        if (repo.rev.empty() && !pManifest->m_sRepository.commitPins.empty()) {
            // check commit pins unless a revision is specified

            progress.printMessageAbove(infoString("Manifest has {} pins, checking", pManifest->m_sRepository.commitPins.size()));

            for (auto const& [hl, plugin] : pManifest->m_sRepository.commitPins) {
                if (hl != HLVER.hash)
                    continue;

                progress.printMessageAbove(successString("commit pin {} matched hl, resetting", plugin));

                execAndGet("cd " + m_szWorkingPluginDirectory + " && git reset --hard --recurse-submodules " + plugin);
            }
        }

        for (auto& p : pManifest->m_vPlugins) {
            std::string out;

            if (p.since > HLVER.commits && HLVER.commits >= 1000 /* for shallow clones, we can't check this. 1000 is an arbitrary number I chose. */) {
                progress.printMessageAbove(failureString("Not building {}: your Hyprland version is too old.\n", p.name));
                p.failed = true;
                continue;
            }

            progress.printMessageAbove(infoString("Building {}", p.name));

            for (auto const& bs : p.buildSteps) {
                const std::string& cmd = std::format("cd {} && PKG_CONFIG_PATH=\"{}/share/pkgconfig\" {}", m_szWorkingPluginDirectory, DataState::getHeadersPath(), bs);
                out += " -> " + cmd + "\n" + execAndGet(cmd) + "\n";
            }

            if (m_bVerbose)
                std::println("{}", verboseString("shell returned: {}", out));

            if (!std::filesystem::exists(m_szWorkingPluginDirectory + "/" + p.output)) {
                std::println(stderr,
                             "\n{}\n"
                             "  This likely means that the plugin is either outdated, not yet available for your version, or broken.\n"
                             "If you are on -git, update first.\n"
                             "Try re-running with -v to see more verbose output.",
                             failureString("Plugin {} failed to build.", p.name));
                p.failed = true;
                continue;
            }

            progress.printMessageAbove(successString("built {} into {}", p.name, p.output));
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

        progress.printMessageAbove(successString("updated {}", repo.name));
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

    std::print("\n");

    return true;
}

bool CPluginManager::enablePlugin(const std::string& name) {
    bool ret = DataState::setPluginEnabled(name, true);
    if (ret)
        std::println("{}", successString("Enabled {}", name));
    return ret;
}

bool CPluginManager::disablePlugin(const std::string& name) {
    bool ret = DataState::setPluginEnabled(name, false);
    if (ret)
        std::println("{}", successString("Disabled {}", name));
    return ret;
}

ePluginLoadStateReturn CPluginManager::ensurePluginsLoadState() {
    if (headersValid() != HEADERS_OK) {
        std::println(stderr, "\n{}", failureString("headers are not up-to-date, please run hyprpm update."));
        return LOADSTATE_HEADERS_OUTDATED;
    }

    const auto HOME = getenv("HOME");
    const auto HIS  = getenv("HYPRLAND_INSTANCE_SIGNATURE");
    if (!HOME || !HIS) {
        std::println(stderr, "PluginManager: no $HOME or HIS");
        return LOADSTATE_FAIL;
    }
    const auto               HYPRPMPATH = DataState::getDataStatePath() + "/";

    auto                     pluginLines = execAndGet("hyprctl plugins list | grep Plugin");

    std::vector<std::string> loadedPlugins;

    std::println("{}", successString("Ensuring plugin load state"));

    // iterate line by line
    while (!pluginLines.empty()) {
        auto plLine = pluginLines.substr(0, pluginLines.find('\n'));

        if (pluginLines.find('\n') != std::string::npos)
            pluginLines = pluginLines.substr(pluginLines.find('\n') + 1);
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
            std::println("{}", successString("Unloaded {}", p));
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
            std::println("{}", successString("Loaded {}", p.name));
        }
    }

    std::println("{}", successString("Plugin load state ensured"));

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
        std::println("{}", infoString("Repository {}:", r.name));

        for (auto const& p : r.plugins) {
            std::println("  │ Plugin {}", p.name);

            if (!p.failed)
                std::println("  └─ enabled: {}", (p.enabled ? std::string{Colors::GREEN} + "true" : std::string{Colors::RED} + "false"));
            else
                std::println("  └─ enabled: {}Plugin failed to build", Colors::RED);

            std::println("{}", Colors::RESET);
        }
    }
}

void CPluginManager::notify(const eNotifyIcons icon, uint32_t color, int durationMs, const std::string& message) {
    execAndGet("hyprctl notify " + std::to_string((int)icon) + " " + std::to_string(durationMs) + " " + std::to_string(color) + " " + message);
}

std::string CPluginManager::headerError(const eHeadersErrors err) {
    switch (err) {
        case HEADERS_CORRUPTED: return failureString("Headers corrupted. Please run hyprpm update to fix those.\n");
        case HEADERS_MISMATCHED: return failureString("Headers version mismatch. Please run hyprpm update to fix those.\n");
        case HEADERS_NOT_HYPRLAND: return failureString("It doesn't seem you are running on hyprland.\n");
        case HEADERS_MISSING: return failureString("Headers missing. Please run hyprpm update to fix those.\n");
        case HEADERS_DUPLICATED: {
            return failureString("Headers duplicated!!! This is a very bad sign.\n"
                                 "This could be due to e.g. installing hyprland manually while a system package of hyprland is also installed.\n"
                                 "If the above doesn't apply, check your /usr/include and /usr/local/include directories\n and remove all the hyprland headers.\n");
        }
        default: break;
    }

    return failureString("Unknown header error. Please run hyprpm update to fix those.\n");
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
