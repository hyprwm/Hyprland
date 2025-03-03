#include "DataState.hpp"
#include <toml++/toml.hpp>
#include <print>
#include <fstream>
#include "PluginManager.hpp"

static std::filesystem::path NDataState::getDataStatePath() {
    const auto HOME = getenv("HOME");
    if (!HOME) {
        std::println(stderr, "DataState: no $HOME");
        throw std::runtime_error("no $HOME");
        return "";
    }

    const auto XDG_DATA_HOME = getenv("XDG_DATA_HOME");

    if (XDG_DATA_HOME)
        return std::filesystem::path{XDG_DATA_HOME} / "hyprpm";
    return std::filesystem::path{HOME} / ".local/share/hyprpm";
}

static std::string NDataState::getHeadersPath() {
    return getDataStatePath() / "headersRoot";
}

static std::vector<std::filesystem::path> NDataState::getPluginStates() {
    ensureStateStoreExists();

    std::vector<std::filesystem::path> states;
    for (const auto& entry : std::filesystem::directory_iterator(getDataStatePath())) {
        if (!entry.is_directory() || entry.path().stem() == "headersRoot")
            continue;

        const auto STATE_FILE = entry.path() / "state.toml";
        if (!std::filesystem::exists(STATE_FILE))
            continue;

        states.emplace_back(STATE_FILE);
    }
    return states;
}

static void NDataState::ensureStateStoreExists() {
    const auto PATH = getDataStatePath();

    if (!std::filesystem::exists(PATH))
        std::filesystem::create_directories(PATH);

    if (!std::filesystem::exists(getHeadersPath()))
        std::filesystem::create_directories(getHeadersPath());
}

static void NDataState::addNewPluginRepo(const SPluginRepository& repo) {
    ensureStateStoreExists();

    const auto PATH = getDataStatePath() / repo.name;

    std::filesystem::create_directories(PATH);
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

        // copy .so to the good place
        if (std::filesystem::exists(p.filename))
            std::filesystem::copy_file(p.filename, PATH / filename);

        DATA.emplace(p.name, toml::table{
            {"filename", filename},
            {"enabled", p.enabled},
            {"failed", p.failed}
        });
    }
    // clang-format on

    std::ofstream ofs(PATH / "state.toml", std::ios::trunc);
    ofs << DATA;
    ofs.close();
}

static bool NDataState::pluginRepoExists(const std::string& urlOrName) {
    ensureStateStoreExists();

    const auto PATH = getDataStatePath();

    for (const auto& STATE_FILE : getPluginStates()) {
        const auto STATE = toml::parse_file(STATE_FILE.c_str());
        const auto NAME  = STATE["repository"]["name"].value_or("");
        const auto URL   = STATE["repository"]["url"].value_or("");

        if (URL == urlOrName || NAME == urlOrName)
            return true;
    }

    return false;
}

static void NDataState::removePluginRepo(const std::string& urlOrName) {
    ensureStateStoreExists();

    const auto PATH = getDataStatePath();

    for (const auto& STATE_FILE : getPluginStates()) {
        const auto STATE = toml::parse_file(STATE_FILE.c_str());
        const auto NAME  = STATE["repository"]["name"].value_or("");
        const auto URL   = STATE["repository"]["url"].value_or("");

        if (URL == urlOrName || NAME == urlOrName) {

            // unload the plugins!!
            for (const auto& file : std::filesystem::directory_iterator(STATE_FILE.parent_path())) {
                if (!file.path().string().ends_with(".so"))
                    continue;

                g_pPluginManager->loadUnloadPlugin(std::filesystem::absolute(file.path()), false);
            }

            std::filesystem::remove_all(STATE_FILE.parent_path());
            return;
        }
    }
}

static void NDataState::updateGlobalState(const SGlobalState& state) {
    ensureStateStoreExists();

    const auto PATH = getDataStatePath();

    std::filesystem::create_directories(PATH);
    // clang-format off
    auto DATA = toml::table{
        {"state", toml::table{
            {"hash", state.headersHashCompiled},
            {"dont_warn_install", state.dontWarnInstall}
        }}
    };
    // clang-format on

    std::ofstream ofs(PATH / "state.toml", std::ios::trunc);
    ofs << DATA;
    ofs.close();
}

static SGlobalState NDataState::getGlobalState() {
    ensureStateStoreExists();

    const auto STATE_FILE = getDataStatePath() / "state.toml";

    if (!std::filesystem::exists(STATE_FILE))
        return SGlobalState{};

    auto         DATA = toml::parse_file(STATE_FILE.c_str());

    SGlobalState state;
    state.headersHashCompiled = DATA["state"]["hash"].value_or("");
    state.dontWarnInstall     = DATA["state"]["dont_warn_install"].value_or(false);

    return state;
}

static std::vector<SPluginRepository> NDataState::getAllRepositories() {
    ensureStateStoreExists();

    const auto                     PATH = getDataStatePath();

    std::vector<SPluginRepository> repos;
    for (const auto& STATE_FILE : getPluginStates()) {
        const auto        STATE = toml::parse_file(STATE_FILE.c_str());

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

static bool NDataState::setPluginEnabled(const std::string& name, bool enabled) {
    ensureStateStoreExists();

    const auto PATH = getDataStatePath();

    for (const auto& STATE_FILE : getPluginStates()) {
        const auto STATE = toml::parse_file(STATE_FILE.c_str());
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

            std::ofstream state(STATE_FILE, std::ios::trunc);
            state << modifiedState;
            state.close();

            return true;
        }
    }

    return false;
}