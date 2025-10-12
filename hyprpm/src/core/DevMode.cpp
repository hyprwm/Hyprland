#include "DevMode.hpp"
#include "PluginManager.hpp"
#include "Manifest.hpp"
#include "DataState.hpp"
#include "../helpers/Colors.hpp"
#include "../helpers/StringUtils.hpp"
#include "../helpers/FileWatcher.hpp"

#include <print>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <iostream>

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

static std::string getTempRoot() {
    static auto ENV = getenv("XDG_RUNTIME_DIR");
    if (!ENV) {
        std::cerr << "\nERROR: XDG_RUNTIME_DIR not set!\n";
        exit(1);
    }

    const auto STR = ENV + std::string{"/hyprpm/"};

    return STR;
}

CDevMode::CDevMode(CPluginManager* pluginManager) : m_pPluginManager(pluginManager) {}

std::string CDevMode::getNestedSessionPidFile() {
    return getTempRoot() + "dev-session.pid";
}

void CDevMode::killExistingNestedSession() {
    const auto pidFile = getNestedSessionPidFile();

    if (!std::filesystem::exists(pidFile))
        return;

    std::ifstream file(pidFile);
    if (!file.is_open())
        return;

    pid_t pid;
    file >> pid;
    file.close();

    // Check if process is still running
    if (kill(pid, 0) == 0) {
        std::println("{}", statusString("!", Colors::YELLOW, "Killing existing nested session (PID: {})", pid));
        kill(pid, SIGTERM);

        // Wait a bit for graceful shutdown
        for (int i = 0; i < 10; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (kill(pid, 0) != 0)
                break;
        }

        // Force kill if still running
        if (kill(pid, 0) == 0) {
            kill(pid, SIGKILL);
        }
    }

    // Remove stale pid file
    std::filesystem::remove(pidFile);
}

bool CDevMode::launchNestedSession(const std::string& pluginPath) {
    // Kill any existing nested session first
    killExistingNestedSession();

    std::println("{}", statusString("→", Colors::BLUE, "Launching nested Hyprland session..."));

    pid_t pid = fork();

    if (pid < 0) {
        std::println(stderr, "{}", failureString("Failed to fork process"));
        return false;
    }

    if (pid == 0) {
        // Child process - launch Hyprland
        execlp("Hyprland", "Hyprland", nullptr);

        // If exec fails
        std::println(stderr, "{}", failureString("Failed to launch Hyprland"));
        exit(1);
    }

    // Parent process - store PID
    const auto pidFile = getNestedSessionPidFile();
    std::filesystem::create_directories(std::filesystem::path(pidFile).parent_path());

    std::ofstream file(pidFile);
    if (file.is_open()) {
        file << pid;
        file.close();
    }

    std::println("{}", successString("Nested session launched (PID: {})", pid));
    std::println("{}", infoString("Waiting for Hyprland to initialize..."));

    // Wait for Hyprland to start up
    // Check if the process is still running and if the socket is available
    int attempts = 0;
    const int maxAttempts = 50; // 5 seconds max
    bool hyprlandReady = false;

    while (attempts < maxAttempts) {
        // Check if process is still alive
        if (kill(pid, 0) != 0) {
            std::println(stderr, "{}", failureString("Hyprland process died during startup"));
            return false;
        }

        // Check if the Hyprland socket exists for this instance
        // The socket path is based on the instance signature
        std::string runtimeDir = std::string(getenv("XDG_RUNTIME_DIR") ?: "/run/user/1000");

        // Try to find the socket by listing runtime directory
        std::string hyprDir = runtimeDir + "/hypr";
        if (std::filesystem::exists(hyprDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(hyprDir)) {
                if (entry.is_directory()) {
                    std::string socketPath = entry.path().string() + "/.socket.sock";
                    if (std::filesystem::exists(socketPath)) {
                        // Found a socket, try to use it
                        std::string instanceSig = entry.path().filename().string();

                        // Try to load the plugin using hyprctl
                        std::string loadCmd = "HYPRLAND_INSTANCE_SIGNATURE=" + instanceSig + " hyprctl plugin load " + pluginPath + " 2>&1";

                        FILE* pipe = popen(loadCmd.c_str(), "r");
                        if (pipe) {
                            char buffer[256];
                            std::string result;
                            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                                result += buffer;
                            }
                            int status = pclose(pipe);

                            if (status == 0 || result.find("Successfully") != std::string::npos) {
                                std::println("{}", successString("Plugin loaded successfully"));

                                // Reload config to register plugin dispatchers
                                std::string reloadCmd = "HYPRLAND_INSTANCE_SIGNATURE=" + instanceSig + " hyprctl reload 2>&1";
                                FILE* reloadPipe = popen(reloadCmd.c_str(), "r");
                                if (reloadPipe) {
                                    pclose(reloadPipe);
                                    std::println("{}", infoString("Configuration reloaded"));
                                }

                                hyprlandReady = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (hyprlandReady)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        attempts++;
    }

    if (!hyprlandReady) {
        std::println("{}", infoString("Could not automatically load plugin. Load it manually with: hyprctl plugin load {}", pluginPath));
    }

    std::println("{}", infoString("Plugin path: {}", pluginPath));
    std::println("{}", infoString("Exit the nested session to return to plugin development"));

    return true;
}

bool CDevMode::run(const std::string& path, bool hotReload) {
    if (hotReload) {
        std::println("{}", statusString("→", Colors::BLUE, "Starting development mode (hot-reload) in {}", std::filesystem::absolute(path).string()));
    } else {
        std::println("{}", statusString("→", Colors::BLUE, "Starting development mode (nested session) in {}", std::filesystem::absolute(path).string()));
    }

    // Check for manifest
    std::unique_ptr<CManifest> pManifest;

    if (std::filesystem::exists(path + "/hyprpm.toml")) {
        std::println("{}", successString("Found hyprpm manifest"));
        pManifest = std::make_unique<CManifest>(MANIFEST_HYPRPM, path + "/hyprpm.toml");
    } else if (std::filesystem::exists(path + "/hyprload.toml")) {
        std::println("{}", successString("Found hyprload manifest"));
        pManifest = std::make_unique<CManifest>(MANIFEST_HYPRLOAD, path + "/hyprload.toml");
    }

    if (!pManifest) {
        std::println(stderr, "{}", failureString("No hyprpm.toml or hyprload.toml found in current directory."));
        return false;
    }

    if (!pManifest->m_good) {
        std::println(stderr, "{}", failureString("Manifest is corrupted."));
        return false;
    }

    // Validate headers
    const auto HEADERSSTATUS = m_pPluginManager->headersValid();
    if (HEADERSSTATUS != HEADERS_OK) {
        std::println(stderr, "\n{}", m_pPluginManager->headerError(HEADERSSTATUS));
        std::println(stderr, "{}", infoString("Headers not found. Running without PKG_CONFIG_PATH."));
        // Continue without headers for dev mode
    } else {
        std::println("{}", successString("Headers valid"));
    }

    // Initial build
    std::println("\n{}", statusString("→", Colors::BLUE, "Performing initial build..."));

    if (m_pPluginManager->m_bVerbose) {
        std::println("{}", verboseString("Calling buildPlugin with path: {}", path));
    }

    if (!m_pPluginManager->buildPlugin(path, pManifest.get())) {
        std::println(stderr, "{}", failureString("Initial build failed. Fix errors and try again."));
        return false;
    }

    // Prepare plugin repository info
    const std::string absPath = std::filesystem::absolute(path).string();

    SPluginRepository repo;
    repo.name = pManifest->m_repository.name.empty() ? "dev-" + std::filesystem::path(absPath).filename().string() : pManifest->m_repository.name;
    repo.url  = absPath;
    repo.hash = "dev-mode";

    for (auto const& p : pManifest->m_plugins) {
        if (!p.failed) {
            repo.plugins.push_back(SPlugin{p.name, absPath + "/" + p.output, true, false});
        }
    }

    // Branch based on mode
    if (!hotReload) {
        // Nested session mode - don't register in parent session's state
        return runNestedSession(path, pManifest.get(), absPath, repo.name);
    } else {
        // Hot-reload mode - register in current session's state
        bool isUpdate = DataState::pluginRepoExists(absPath);

        if (isUpdate) {
            DataState::removePluginRepo(absPath);
        }

        DataState::addNewPluginRepo(repo);

        return runHotReload(path, pManifest.get(), absPath, repo.name);
    }
}

bool CDevMode::runNestedSession(const std::string& path, CManifest* pManifest, const std::string& absPath, const std::string& repoName) {
    // Nested session mode - launch Hyprland with the plugin
    // Build plugin path list for all successful plugins using the actual built .so files
    std::string pluginPaths;
    for (auto const& p : pManifest->m_plugins) {
        if (!p.failed) {
            // Construct and normalize the path
            std::filesystem::path pluginPath = std::filesystem::path(absPath) / p.output;
            pluginPath = std::filesystem::canonical(pluginPath);

            if (!pluginPaths.empty())
                pluginPaths += ":";
            pluginPaths += pluginPath.string();
        }
    }

    if (pluginPaths.empty()) {
        std::println(stderr, "{}", failureString("No plugins built successfully"));
        return false;
    }

    // Launch nested session with the plugin
    if (!launchNestedSession(pluginPaths)) {
        return false;
    }

    // Setup file watcher for nested session mode
    std::println("\n{}", statusString("👀", Colors::BLUE, "Watching for file changes (Press Ctrl+C to stop)..."));

    CFileWatcher watcher;
    if (!watcher.addWatch(path)) {
        std::println(stderr, "{}", failureString("Failed to setup file watcher"));
        killExistingNestedSession();
        return false;
    }

    auto      lastBuildTime     = std::chrono::steady_clock::now();
    const int DEBOUNCE_MS       = 500;
    bool      pendingBuild      = false;
    bool      isBuilding        = false;
    bool      buildInterrupted  = false;

    std::unique_ptr<CManifest> pManifestPtr = std::unique_ptr<CManifest>(pManifest);

    while (true) {
        watcher.waitForEvents(100);

        if (watcher.hasChanges()) {
            pendingBuild  = true;
            lastBuildTime = std::chrono::steady_clock::now();
            watcher.clearChanges();

            if (isBuilding) {
                buildInterrupted = true;
            }
        }

        if (pendingBuild && !isBuilding) {
            auto now     = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBuildTime).count();

            if (elapsed >= DEBOUNCE_MS) {
                pendingBuild     = false;
                isBuilding       = true;
                buildInterrupted = false;

                std::println("\n{}", statusString("🔨", Colors::YELLOW, "Change detected, rebuilding and restarting..."));

                auto buildStart = std::chrono::steady_clock::now();

                // Reload manifest
                if (std::filesystem::exists(path + "/hyprpm.toml")) {
                    pManifestPtr = std::make_unique<CManifest>(MANIFEST_HYPRPM, path + "/hyprpm.toml");
                } else if (std::filesystem::exists(path + "/hyprload.toml")) {
                    pManifestPtr = std::make_unique<CManifest>(MANIFEST_HYPRLOAD, path + "/hyprload.toml");
                }

                if (!pManifestPtr || !pManifestPtr->m_good) {
                    std::println(stderr, "{}", failureString("Manifest became invalid, skipping build."));
                    isBuilding = false;
                    continue;
                }

                auto shouldInterrupt = [&]() -> bool {
                    if (buildInterrupted)
                        return true;
                    if (watcher.waitForEvents(0)) {
                        if (watcher.hasChanges()) {
                            pendingBuild  = true;
                            lastBuildTime = std::chrono::steady_clock::now();
                            watcher.clearChanges();
                            return true;
                        }
                    }
                    return false;
                };

                bool buildSuccess = m_pPluginManager->buildPlugin(path, pManifestPtr.get(), shouldInterrupt);
                isBuilding        = false;

                if (!buildSuccess && (buildInterrupted || pendingBuild)) {
                    std::println("{}", infoString("Restarting build with latest changes..."));
                    continue;
                }

                if (buildSuccess) {
                    // Rebuild plugin path list using actual built .so files
                    pluginPaths.clear();
                    for (auto const& p : pManifestPtr->m_plugins) {
                        if (!p.failed) {
                            // Construct and normalize the path
                            std::filesystem::path pluginPath = std::filesystem::path(absPath) / p.output;
                            pluginPath = std::filesystem::canonical(pluginPath);

                            if (!pluginPaths.empty())
                                pluginPaths += ":";
                            pluginPaths += pluginPath.string();
                        }
                    }

                    // Relaunch nested session
                    if (launchNestedSession(pluginPaths)) {
                        auto buildEnd    = std::chrono::steady_clock::now();
                        auto buildTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(buildEnd - buildStart).count();
                        std::println("{}", successString("Session restarted ({:.1f}s)", buildTimeMs / 1000.0));
                    } else {
                        std::println(stderr, "{}", failureString("Failed to restart session"));
                    }
                } else {
                    std::println(stderr, "{}", failureString("Build failed, fix errors and save again."));
                }

                std::println("{}", statusString("👀", Colors::BLUE, "Watching for changes..."));
            }
        }
    }

    return true;
}

bool CDevMode::runHotReload(const std::string& path, CManifest* pManifest, const std::string& absPath, const std::string& repoName) {
    // Hot-reload mode - load plugins into current session and watch for changes
    // Load plugins
    std::println("\n{}", statusString("→", Colors::BLUE, "Loading plugins..."));

    const auto HYPRPMPATH = DataState::getDataStatePath();

    for (auto const& p : pManifest->m_plugins) {
        if (p.failed) {
            continue;
        }

        const std::string pluginPath = (HYPRPMPATH / repoName / (p.name + ".so")).string();

        bool loaded = m_pPluginManager->loadUnloadPlugin(pluginPath, true);

        if (loaded) {
            std::println("{}", successString("Loaded {}", p.name));
        } else {
            std::println("{}", infoString("{} will be loaded after restarting Hyprland", p.name));
        }
    }

    // Setup file watcher
    std::println("\n{}", statusString("👀", Colors::BLUE, "Watching for file changes (Press Ctrl+C to stop)..."));

    CFileWatcher watcher;
    if (!watcher.addWatch(path)) {
        std::println(stderr, "{}", failureString("Failed to setup file watcher"));
        return false;
    }

    auto      lastBuildTime    = std::chrono::steady_clock::now();
    const int DEBOUNCE_MS      = 500;
    bool      pendingBuild     = false;
    bool      isBuilding       = false;
    bool      buildInterrupted = false;

    std::unique_ptr<CManifest> pManifestPtr = std::unique_ptr<CManifest>(pManifest);

    while (true) {
        // Check for changes with a small timeout
        watcher.waitForEvents(100);

        if (watcher.hasChanges()) {
            pendingBuild  = true;
            lastBuildTime = std::chrono::steady_clock::now();
            watcher.clearChanges();

            // If we're currently building, mark that we want to interrupt
            if (isBuilding) {
                buildInterrupted = true;
            }
        }

        // If we have pending changes and enough time has passed (debounce)
        if (pendingBuild && !isBuilding) {
            auto now     = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastBuildTime).count();

            if (elapsed >= DEBOUNCE_MS) {
                pendingBuild     = false;
                isBuilding       = true;
                buildInterrupted = false;

                std::println("\n{}", statusString("🔨", Colors::YELLOW, "Change detected, rebuilding..."));

                auto buildStart = std::chrono::steady_clock::now();

                // Reload manifest (it might have changed)
                if (std::filesystem::exists(path + "/hyprpm.toml")) {
                    pManifestPtr = std::make_unique<CManifest>(MANIFEST_HYPRPM, path + "/hyprpm.toml");
                } else if (std::filesystem::exists(path + "/hyprload.toml")) {
                    pManifestPtr = std::make_unique<CManifest>(MANIFEST_HYPRLOAD, path + "/hyprload.toml");
                }

                if (!pManifestPtr || !pManifestPtr->m_good) {
                    std::println(stderr, "{}", failureString("Manifest became invalid, skipping build."));
                    isBuilding = false;
                    continue;
                }

                // Build with interruption support
                auto shouldInterrupt = [&]() -> bool {
                    if (buildInterrupted) {
                        return true;
                    }
                    if (watcher.waitForEvents(0)) {
                        if (watcher.hasChanges()) {
                            pendingBuild  = true;
                            lastBuildTime = std::chrono::steady_clock::now();
                            watcher.clearChanges();
                            return true;
                        }
                    }
                    return false;
                };

                bool buildSuccess = m_pPluginManager->buildPlugin(path, pManifestPtr.get(), shouldInterrupt);

                isBuilding = false;

                // If build was interrupted, immediately continue to handle new changes
                if (!buildSuccess && (buildInterrupted || pendingBuild)) {
                    std::println("{}", infoString("Restarting build with latest changes..."));
                    continue;
                }

                if (buildSuccess) {
                    // Reload plugins
                    for (auto const& p : pManifestPtr->m_plugins) {
                        if (p.failed)
                            continue;

                        const std::string pluginPath = (HYPRPMPATH / repoName / (p.name + ".so")).string();

                        // Try to reload (unload first if already loaded)
                        m_pPluginManager->loadUnloadPlugin(pluginPath, false); // unload
                        if (m_pPluginManager->loadUnloadPlugin(pluginPath, true)) { // reload
                            auto buildEnd    = std::chrono::steady_clock::now();
                            auto buildTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(buildEnd - buildStart).count();
                            std::println("{}", successString("Reloaded {} ({:.1f}s)", p.name, buildTimeMs / 1000.0));
                        } else {
                            std::println("{}", infoString("{} needs Hyprland restart", p.name));
                        }
                    }
                } else {
                    std::println(stderr, "{}", failureString("Build failed, fix errors and save again."));
                }

                std::println("{}", statusString("👀", Colors::BLUE, "Watching for changes..."));
            }
        }
    }

    return true;
}
