#include "DataState.hpp"
#include <sys/stat.h>
#include <toml++/toml.hpp>
#include <print>
#include <fstream>
#include "PluginManager.hpp"
#include "../helpers/Die.hpp"
#include "../helpers/Sys.hpp"
#include "../helpers/StringUtils.hpp"

std::filesystem::path DataState::getDataStatePath() {
    return std::filesystem::path("/var/cache/hyprpm/" + g_pPluginManager->m_szUsername);
}

std::string DataState::getHeadersPath() {
    return getDataStatePath() / "headersRoot";
}

std::vector<std::filesystem::path> DataState::getPluginStates() {
    ensureStateStoreExists();

    std::vector<std::filesystem::path> states;
    for (const auto& entry : std::filesystem::directory_iterator(getDataStatePath())) {
        if (!entry.is_directory() || entry.path().stem() == "headersRoot")
            continue;

        const auto stateFile = entry.path() / "state.toml";
        if (!std::filesystem::exists(stateFile))
            continue;

        states.emplace_back(stateFile);
    }
    return states;
}

void DataState::ensureStateStoreExists() {
    std::error_code ec;
    if (!std::filesystem::exists(getHeadersPath(), ec) || ec) {
        std::println("{}", infoString("The hyprpm state store doesn't exist. Creating now..."));
        if (!std::filesystem::exists("/var/cache/hyprpm/", ec) || ec)
            NSys::runAsSuperuser("mkdir -p -m 755 '/var/cache/hyprpm/'");
        if (!std::filesystem::exists(getDataStatePath(), ec) || ec)
            NSys::runAsSuperuser("mkdir -p -m 755 '" + getDataStatePath().string() + "'");
        NSys::runAsSuperuser("mkdir -p -m 755 '" + getHeadersPath() + "'");
    }
}

void DataState::addNewPluginRepo(const SPluginRepository& repo) {
    ensureStateStoreExists();

    const auto      PATH = getDataStatePath() / repo.name;

    std::error_code ec;
    if (!std::filesystem::exists(PATH, ec) || ec)
        NSys::runAsSuperuser("mkdir -p -m 755 '" + PATH.string() + "'");
    // clang-format off
    auto DATA = toml::table{
        {"repository", toml::table{
            {"name", repo.name},
            {"hash", repo.hash},
            {"url", repo.url},
            {"rev", repo.rev}
        }}
    };
    for (auto const& p : repo.plugins) {
        const auto filename = p.name + ".so";

        // copy .so to the good place and chmod 755
        if (std::filesystem::exists(p.filename))
            NSys::runAsSuperuser("cp '" + p.filename + "' '" + (PATH / filename).string() + "' && chmod 755 '" + (PATH / filename).string() + "'");

        DATA.emplace(p.name, toml::table{
            {"filename", filename},
            {"enabled", p.enabled},
            {"failed", p.failed}
        });
    }
    // clang-format on

    std::stringstream ss;
    ss << DATA;

    NSys::runAsSuperuser("cat << EOF > " + (PATH / "state.toml").string() + "\n" + ss.str() + "\nEOF");
    NSys::runAsSuperuser("chmod 644 '" + (PATH / "state.toml").string() + "'");
}

bool DataState::pluginRepoExists(const std::string& urlOrName) {
    ensureStateStoreExists();

    for (const auto& stateFile : getPluginStates()) {
        const auto STATE = toml::parse_file(stateFile.c_str());
        const auto NAME  = STATE["repository"]["name"].value_or("");
        const auto URL   = STATE["repository"]["url"].value_or("");

        if (URL == urlOrName || NAME == urlOrName)
            return true;
    }

    return false;
}

void DataState::removePluginRepo(const std::string& urlOrName) {
    ensureStateStoreExists();

    for (const auto& stateFile : getPluginStates()) {
        const auto STATE = toml::parse_file(stateFile.c_str());
        const auto NAME  = STATE["repository"]["name"].value_or("");
        const auto URL   = STATE["repository"]["url"].value_or("");

        if (URL == urlOrName || NAME == urlOrName) {

            // unload the plugins!!
            for (const auto& file : std::filesystem::directory_iterator(stateFile.parent_path())) {
                if (!file.path().string().ends_with(".so"))
                    continue;

                g_pPluginManager->loadUnloadPlugin(std::filesystem::absolute(file.path()), false);
            }

            const auto PATH = stateFile.parent_path().string();

            if (!PATH.starts_with("/var/cache/hyprpm") || PATH.contains('\''))
                return; // WTF?

            // scary!
            NSys::runAsSuperuser("rm -r '" + PATH + "'");
            return;
        }
    }
}

void DataState::updateGlobalState(const SGlobalState& state) {
    ensureStateStoreExists();

    const auto      PATH = getDataStatePath();

    std::error_code ec;
    if (!std::filesystem::exists(PATH, ec) || ec)
        NSys::runAsSuperuser("mkdir -p -m 755 '" + PATH.string() + "'");
    // clang-format off
    auto DATA = toml::table{
        {"state", toml::table{
            {"hash", state.headersHashCompiled},
            {"dont_warn_install", state.dontWarnInstall}
        }}
    };
    // clang-format on

    std::stringstream ss;
    ss << DATA;

    NSys::runAsSuperuser("cat << EOF > " + (PATH / "state.toml").string() + "\n" + ss.str() + "\nEOF");
    NSys::runAsSuperuser("chmod 644 '" + (PATH / "state.toml").string() + "'");
}

SGlobalState DataState::getGlobalState() {
    ensureStateStoreExists();

    const auto      stateFile = getDataStatePath() / "state.toml";

    std::error_code ec;
    if (!std::filesystem::exists(stateFile, ec) || ec)
        return SGlobalState{};

    auto         DATA = toml::parse_file(stateFile.c_str());

    SGlobalState state;
    state.headersHashCompiled = DATA["state"]["hash"].value_or("");
    state.dontWarnInstall     = DATA["state"]["dont_warn_install"].value_or(false);

    return state;
}

std::vector<SPluginRepository> DataState::getAllRepositories() {
    ensureStateStoreExists();

    std::vector<SPluginRepository> repos;
    for (const auto& stateFile : getPluginStates()) {
        const auto        STATE = toml::parse_file(stateFile.c_str());

        const auto        NAME = STATE["repository"]["name"].value_or("");
        const auto        URL  = STATE["repository"]["url"].value_or("");
        const auto        REV  = STATE["repository"]["rev"].value_or("");
        const auto        HASH = STATE["repository"]["hash"].value_or("");

        SPluginRepository repo;
        repo.hash = HASH;
        repo.name = NAME;
        repo.url  = URL;
        repo.rev  = REV;

        for (const auto& [key, val] : STATE) {
            if (key == "repository")
                continue;

            const auto ENABLED  = STATE[key]["enabled"].value_or(false);
            const auto FAILED   = STATE[key]["failed"].value_or(false);
            const auto FILENAME = STATE[key]["filename"].value_or("");

            repo.plugins.push_back(SPlugin{std::string{key.str()}, FILENAME, ENABLED, FAILED});
        }

        repos.push_back(repo);
    }

    return repos;
}

bool DataState::setPluginEnabled(const std::string& name, bool enabled) {
    ensureStateStoreExists();

    for (const auto& stateFile : getPluginStates()) {
        const auto STATE = toml::parse_file(stateFile.c_str());
        for (const auto& [key, val] : STATE) {
            if (key == "repository")
                continue;

            if (key.str() != name)
                continue;

            const auto FAILED = STATE[key]["failed"].value_or(false);

            if (FAILED)
                return false;

            auto modifiedState = STATE;
            (*modifiedState[key].as_table()).insert_or_assign("enabled", enabled);

            std::stringstream ss;
            ss << modifiedState;

            NSys::runAsSuperuser("cat << EOF > " + stateFile.string() + "\n" + ss.str() + "\nEOF");
            NSys::runAsSuperuser("chmod 644 '" + stateFile.string() + "'");

            return true;
        }
    }

    return false;
}

void DataState::purgeAllCache() {
    const auto PATH = getDataStatePath().string();
    if (PATH.contains('\''))
        return;
    // scary!
    NSys::runAsSuperuser("rm -r '" + PATH + "'");
}
