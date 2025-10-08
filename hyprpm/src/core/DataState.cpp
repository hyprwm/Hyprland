#include "DataState.hpp"
#include <sys/stat.h>
#include <toml++/toml.hpp>
#include <print>
#include <sstream>
#include <fstream>
#include "PluginManager.hpp"
#include "../helpers/Die.hpp"
#include "../helpers/Sys.hpp"
#include "../helpers/StringUtils.hpp"

static std::string getTempRoot() {
    static auto ENV = getenv("XDG_RUNTIME_DIR");
    if (!ENV) {
        std::cerr << "\nERROR: XDG_RUNTIME_DIR not set!\n";
        exit(1);
    }

    const auto STR = ENV + std::string{"/hyprpm/"};

    if (!std::filesystem::exists(STR))
        mkdir(STR.c_str(), S_IRWXU);

    return STR;
}

// write the state to a file
static bool writeState(const std::string& str, const std::string& to) {
    // create temp file in a safe temp root
    std::ofstream of(getTempRoot() + ".temp-state", std::ios::trunc);
    if (!of.good())
        return false;

    of << str;
    of.close();

    return NSys::root::install(getTempRoot() + ".temp-state", to, "644");
}

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
        if (!std::filesystem::exists("/var/cache/hyprpm/", ec) || ec) {
            if (!NSys::root::createDirectory("/var/cache/hyprpm", "755"))
                Debug::die("ensureStateStoreExists: Failed to run a superuser cmd");
        }
        if (!std::filesystem::exists(getDataStatePath(), ec) || ec) {
            if (!NSys::root::createDirectory(getDataStatePath().string(), "755"))
                Debug::die("ensureStateStoreExists: Failed to run a superuser cmd");
        }
        if (!NSys::root::createDirectory(getHeadersPath(), "755"))
            Debug::die("ensureStateStoreExists: Failed to run a superuser cmd");
    }
}

void DataState::addNewPluginRepo(const SPluginRepository& repo) {
    ensureStateStoreExists();

    const auto      PATH = getDataStatePath() / repo.name;

    std::error_code ec;
    if (!std::filesystem::exists(PATH, ec) || ec) {
        if (!NSys::root::createDirectory(PATH.string(), "755"))
            Debug::die("addNewPluginRepo: failed to create cache dir");
    }
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
        if (std::filesystem::exists(p.filename)) {
            if (!NSys::root::install(p.filename, (PATH / filename).string(), "0755"))
                Debug::die("addNewPluginRepo: failed to install so file");
        }

        DATA.emplace(p.name, toml::table{
            {"filename", filename},
            {"enabled", p.enabled},
            {"failed", p.failed}
        });
    }
    // clang-format on

    std::stringstream ss;
    ss << DATA;

    if (!writeState(ss.str(), (PATH / "state.toml").string()))
        Debug::die("{}", failureString("Failed to write plugin state"));
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
            if (!NSys::root::removeRecursive(PATH))
                Debug::die("removePluginRepo: failed to remove dir");
            return;
        }
    }
}

void DataState::updateGlobalState(const SGlobalState& state) {
    ensureStateStoreExists();

    const auto      PATH = getDataStatePath();

    std::error_code ec;
    if (!std::filesystem::exists(PATH, ec) || ec) {
        if (!NSys::root::createDirectory(PATH.string(), "755"))
            Debug::die("updateGlobalState: failed to create dir");
    }
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

    if (!writeState(ss.str(), (PATH / "state.toml").string()))
        Debug::die("{}", failureString("Failed to write plugin state"));
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

            if (!writeState(ss.str(), stateFile.string()))
                Debug::die("{}", failureString("Failed to write plugin state"));

            return true;
        }
    }

    return false;
}

void DataState::purgeAllCache() {
    std::error_code ec;
    if (!std::filesystem::exists(getDataStatePath()) && !ec) {
        std::println("{}", infoString("Nothing to do"));
        return;
    }

    const auto PATH = getDataStatePath().string();
    if (PATH.contains('\''))
        return;
    // scary!
    if (!NSys::root::removeRecursive(PATH))
        Debug::die("Failed to run a superuser cmd");
}
