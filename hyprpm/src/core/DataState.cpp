#include "DataState.hpp"
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>
#include <cerrno>
#include <toml++/toml.hpp>
#include <print>
#include <sstream>
#include <string_view>
#include <format>
#include "PluginManager.hpp"
#include "../helpers/Die.hpp"
#include "../helpers/Sys.hpp"
#include "../helpers/StringUtils.hpp"

#include <hyprutils/memory/Casts.hpp>
using namespace Hyprutils::Memory;

static std::string getTempRoot() {
    static auto ENV = getenv("XDG_RUNTIME_DIR");
    if (!ENV) {
        std::cerr << "\nERROR: XDG_RUNTIME_DIR not set!\n";
        exit(1);
    }

    const auto STR = std::format("{}/hyprpm/", ENV);

    if (!std::filesystem::exists(STR))
        mkdir(STR.c_str(), S_IRWXU);

    return STR;
}

static bool writeAll(int fd, std::string_view data) {
    size_t written = 0;
    while (written < data.size()) {
        const auto WRITE = write(fd, data.data() + written, data.size() - written);
        if (WRITE < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        if (WRITE == 0)
            return false;
        written += WRITE;
    }
    return true;
}

// write the state to a file
static bool writeState(const std::string& str, const std::string& to) {
    int TEMP_FD = open(getTempRoot().c_str(), O_RDWR | O_TMPFILE | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (TEMP_FD < 0)
        TEMP_FD = memfd_create("hyprpm-state", MFD_CLOEXEC);
    if (TEMP_FD < 0)
        return false;

    const bool WRITTEN   = writeAll(TEMP_FD, str) && lseek(TEMP_FD, 0, SEEK_SET) == 0;
    const bool INSTALLED = WRITTEN && NSys::root::installFromFD(TEMP_FD, to, "644");
    close(TEMP_FD);
    return INSTALLED;
}

std::filesystem::path DataState::getDataStatePath() {
    return std::filesystem::path(std::format("/var/cache/hyprpm/{}", g_pPluginManager->m_szUsername));
}

std::filesystem::path DataState::getRepositoryCachePath() {
    const auto XDG_CACHE_HOME = getenv("XDG_CACHE_HOME");
    if (XDG_CACHE_HOME && std::filesystem::path{XDG_CACHE_HOME}.is_absolute())
        return std::filesystem::path{XDG_CACHE_HOME} / "hyprpm" / "repos";

    const auto USER = getpwuid(NSys::getUID());
    if (!USER || !USER->pw_dir)
        Debug::die("getRepositoryCachePath: Failed to determine the user's home directory");

    return std::filesystem::path{USER->pw_dir} / ".cache" / "hyprpm" / "repos";
}

static bool installPluginBinary(const std::filesystem::path& source, const std::filesystem::path& destination) {
    const int SOURCE_FD = open(source.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (SOURCE_FD < 0)
        return false;

    struct stat sourceStat;
    if (fstat(SOURCE_FD, &sourceStat) != 0 || !S_ISREG(sourceStat.st_mode) || sourceStat.st_uid != sc<uid_t>(NSys::getUID())) {
        close(SOURCE_FD);
        return false;
    }

    const bool INSTALLED = NSys::root::installFromFD(SOURCE_FD, destination.string(), "0755");
    close(SOURCE_FD);
    return INSTALLED;
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
            {"author", repo.author},
            {"hash", repo.hash},
            {"url", repo.url},
            {"rev", repo.rev}
        }}
    };
    for (auto const& p : repo.plugins) {
        const auto filename = std::format("{}.so", p.name);

        // copy .so to the good place and chmod 755
        if (!p.failed && std::filesystem::exists(p.filename)) {
            if (!installPluginBinary(p.filename, PATH / filename))
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

bool DataState::pluginRepoExists(const SPluginRepoIdentifier& identifier) {
    ensureStateStoreExists();

    for (const auto& stateFile : getPluginStates()) {
        const auto STATE  = toml::parse_file(stateFile.c_str());
        const auto NAME   = STATE["repository"]["name"].value_or("");
        const auto AUTHOR = STATE["repository"]["author"].value_or("");
        const auto URL    = STATE["repository"]["url"].value_or("");

        if (identifier.matches(URL, NAME, AUTHOR))
            return true;
    }

    return false;
}

void DataState::removePluginRepo(const SPluginRepoIdentifier& identifier) {
    ensureStateStoreExists();

    for (const auto& stateFile : getPluginStates()) {
        const auto STATE  = toml::parse_file(stateFile.c_str());
        const auto NAME   = STATE["repository"]["name"].value_or("");
        const auto AUTHOR = STATE["repository"]["author"].value_or("");
        const auto URL    = STATE["repository"]["url"].value_or("");

        if (identifier.matches(URL, NAME, AUTHOR)) {
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
            {"hash", state.headersAbiCompiled},
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
    state.headersAbiCompiled = DATA["state"]["hash"].value_or("");
    state.dontWarnInstall    = DATA["state"]["dont_warn_install"].value_or(false);

    return state;
}

std::vector<SPluginRepository> DataState::getAllRepositories() {
    ensureStateStoreExists();

    std::vector<SPluginRepository> repos;
    for (const auto& stateFile : getPluginStates()) {
        const auto        STATE = toml::parse_file(stateFile.c_str());

        const auto        NAME   = STATE["repository"]["name"].value_or("");
        const auto        AUTHOR = STATE["repository"]["author"].value_or("");
        const auto        URL    = STATE["repository"]["url"].value_or("");
        const auto        REV    = STATE["repository"]["rev"].value_or("");
        const auto        HASH   = STATE["repository"]["hash"].value_or("");

        SPluginRepository repo;
        repo.hash   = HASH;
        repo.name   = NAME;
        repo.author = AUTHOR;
        repo.url    = URL;
        repo.rev    = REV;

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

bool DataState::setPluginEnabled(const SPluginRepoIdentifier& identifier, bool enabled) {
    ensureStateStoreExists();

    for (const auto& stateFile : getPluginStates()) {
        const auto STATE = toml::parse_file(stateFile.c_str());
        for (const auto& [key, val] : STATE) {
            if (key == "repository")
                continue;

            switch (identifier.type) {
                case IDENTIFIER_NAME:
                    if (key.str() != identifier.name)
                        continue;
                    break;
                case IDENTIFIER_AUTHOR_NAME:
                    if (STATE["repository"]["author"] != identifier.author || key.str() != identifier.name)
                        continue;
                    break;
                default: return false;
            }

            const auto FAILED = STATE[key]["failed"].value_or(false);

            // a plugin that failed to build cannot be enabled, but can always be disabled
            if (FAILED && enabled)
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
    bool            removed = false;

    if (std::filesystem::exists(getDataStatePath(), ec) && !ec) {
        const auto PATH = getDataStatePath().string();
        if (PATH.contains('\''))
            return;
        // scary!
        if (!NSys::root::removeRecursive(PATH))
            Debug::die("Failed to run a superuser cmd");
        removed = true;
    }

    ec.clear();
    const auto REPOSITORY_CACHE        = getRepositoryCachePath();
    const auto REPOSITORY_CACHE_STATUS = std::filesystem::symlink_status(REPOSITORY_CACHE, ec);
    if (!ec && REPOSITORY_CACHE_STATUS.type() != std::filesystem::file_type::not_found) {
        std::filesystem::remove_all(REPOSITORY_CACHE, ec);
        if (ec)
            Debug::die("Failed to remove the local repository cache");
        removed = true;
    }

    if (!removed)
        std::println("{}", infoString("Nothing to do"));
}
