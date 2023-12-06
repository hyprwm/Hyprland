#include "DataState.hpp"
#include <toml++/toml.hpp>
#include <iostream>
#include <filesystem>
#include <fstream>

void DataState::addNewPluginRepo(const SPluginRepository& repo) {
    const auto HOME = getenv("HOME");
    if (!HOME) {
        std::cerr << "DataState: no $HOME\n";
        return;
    }
    const auto PATH = std::string(HOME) + "/.hyprpm/" + repo.name;

    std::filesystem::create_directories(PATH);
    // clang-format off
    auto DATA = toml::table{
        {"repository", toml::table{
            {"name", repo.name},
            {"hash", repo.hash},
            {"url", repo.url}
        }}
    };
    for (auto& p : repo.plugins) {
        // copy .so to the good place
        std::filesystem::copy_file(p.filename, PATH + "/" + p.name + ".so");

        DATA.emplace(p.name, toml::table{
            {"filename", p.name + ".so"},
            {"enabled", p.enabled}
        });
    }
    // clang-format on

    std::ofstream ofs(PATH + "/state.toml", std::ios::trunc);
    ofs << DATA;
    ofs.close();
}

bool DataState::pluginRepoExists(const std::string& urlOrName) {
    const auto HOME = getenv("HOME");
    if (!HOME) {
        std::cerr << "DataState: no $HOME\n";
        return false;
    }
    const auto PATH = std::string(HOME) + "/.hyprpm/";

    for (const auto& entry : std::filesystem::directory_iterator(PATH)) {
        if (!entry.is_directory())
            continue;

        auto       STATE = toml::parse_file(entry.path().string() + "/state.toml");

        const auto NAME = STATE["repository"]["name"].value_or("");
        const auto URL  = STATE["repository"]["url"].value_or("");

        if (URL == urlOrName || NAME == urlOrName)
            return true;
    }

    return false;
}

void DataState::removePluginRepo(const std::string& urlOrName) {
    const auto HOME = getenv("HOME");
    if (!HOME) {
        std::cerr << "DataState: no $HOME\n";
        return;
    }
    const auto PATH = std::string(HOME) + "/.hyprpm/";

    for (const auto& entry : std::filesystem::directory_iterator(PATH)) {
        if (!entry.is_directory())
            continue;

        auto       STATE = toml::parse_file(entry.path().string() + "/state.toml");

        const auto NAME = STATE["repository"]["name"].value_or("");
        const auto URL  = STATE["repository"]["url"].value_or("");

        if (URL == urlOrName || NAME == urlOrName) {
            std::filesystem::remove_all(entry.path());
            return;
        }
    }
}

void DataState::updateGlobalState(const SGlobalState& state) {
    const auto HOME = getenv("HOME");
    if (!HOME) {
        std::cerr << "DataState: no $HOME\n";
        return;
    }
    const auto PATH = std::string(HOME) + "/.hyprpm/";

    std::filesystem::create_directories(PATH);
    // clang-format off
    auto DATA = toml::table{
        {"state", toml::table{
            {"hash", state.headersHashCompiled},
        }}
    };
    // clang-format on

    std::ofstream ofs(PATH + "/state.toml", std::ios::trunc);
    ofs << DATA;
    ofs.close();
}

std::vector<SPluginRepository> DataState::getAllRepositories() {
    const auto HOME = getenv("HOME");
    if (!HOME) {
        std::cerr << "DataState: no $HOME\n";
        return {};
    }
    const auto                     PATH = std::string(HOME) + "/.hyprpm/";

    std::vector<SPluginRepository> repos;

    for (const auto& entry : std::filesystem::directory_iterator(PATH)) {
        if (!entry.is_directory())
            continue;

        auto              STATE = toml::parse_file(entry.path().string() + "/state.toml");

        const auto        NAME = STATE["repository"]["name"].value_or("");
        const auto        URL  = STATE["repository"]["url"].value_or("");
        const auto        HASH = STATE["repository"]["hash"].value_or("");

        SPluginRepository repo;
        repo.hash = HASH;
        repo.name = NAME;
        repo.url  = URL;

        for (const auto& [key, val] : STATE) {
            if (key == "repository")
                continue;

            const auto ENABLED  = STATE[key]["enabled"].value_or(false);
            const auto FILENAME = STATE[key]["filename"].value_or("");

            repo.plugins.push_back(SPlugin{std::string{key.str()}, FILENAME, ENABLED});
        }

        repos.push_back(repo);
    }

    return repos;
}

bool DataState::setPluginEnabled(const std::string& name, bool enabled) {
    const auto HOME = getenv("HOME");
    if (!HOME) {
        std::cerr << "DataState: no $HOME\n";
        return false;
    }
    const auto PATH = std::string(HOME) + "/.hyprpm/";

    for (const auto& entry : std::filesystem::directory_iterator(PATH)) {
        if (!entry.is_directory())
            continue;

        auto STATE = toml::parse_file(entry.path().string() + "/state.toml");

        for (const auto& [key, val] : STATE) {
            if (key == "repository")
                continue;

            if (key.str() != name)
                continue;

            (*STATE[key].as_table()).insert_or_assign("enabled", enabled);

            std::ofstream state(entry.path().string() + "/state.toml", std::ios::trunc);
            state << STATE;
            state.close();

            return true;
        }
    }

    return false;
}