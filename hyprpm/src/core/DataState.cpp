#include "DataState.hpp"
#include <toml++/toml.hpp>
#include <print>
#include <fstream>
#include "PluginManager.hpp"

std::filesystem::path DataState::getDataStatePath() {
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
    const auto PATH = getDataStatePath();

    if (!std::filesystem::exists(PATH))
        std::filesystem::create_directories(PATH);

    if (!std::filesystem::exists(getHeadersPath()))
        std::filesystem::create_directories(getHeadersPath());
}

void DataState::addNewPluginRepo(const SPluginRepository& repo) {
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

bool DataState::pluginRepoExists(const std::string& urlOrName) {
    ensureStateStoreExists();

    const auto PATH = getDataStatePath();

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

    const auto PATH = getDataStatePath();

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

            std::filesystem::remove_all(stateFile.parent_path());
            return;
        }
    }
}

void DataState::updateGlobalState(const SGlobalState& state) {
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

SGlobalState DataState::getGlobalState() {
    ensureStateStoreExists();

    const auto stateFile = getDataStatePath() / "state.toml";

    if (!std::filesystem::exists(stateFile))
        return SGlobalState{};

    auto         DATA = toml::parse_file(stateFile.c_str());

    SGlobalState state;
    state.headersHashCompiled = DATA["state"]["hash"].value_or("");
    state.dontWarnInstall     = DATA["state"]["dont_warn_install"].value_or(false);

    return state;
}

std::vector<SPluginRepository> DataState::getAllRepositories() {
    ensureStateStoreExists();

    const auto                     PATH = getDataStatePath();

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

    const auto PATH = getDataStatePath();

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

            std::ofstream state(stateFile, std::ios::trunc);
            state << modifiedState;
            state.close();

            return true;
        }
    }

    return false;
}
