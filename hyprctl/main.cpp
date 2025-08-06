#include <re2/re2.h>

#include <cctype>
#include <netdb.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <pwd.h>
#include <unistd.h>
#include <algorithm>
#include <csignal>
#include <ranges>
#include <optional>
#include <charconv>

#include <iostream>
#include <string>
#include <print>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cstdarg>
#include <hyprutils/string/String.hpp>
using namespace Hyprutils::String;

#include "Strings.hpp"

std::string instanceSignature;
bool        quiet = false;

struct SInstanceData {
    std::string id;
    uint64_t    time;
    uint64_t    pid;
    std::string wlSocket;
};

void log(const std::string_view str) {
    if (quiet)
        return;

    std::println("{}", str);
}

static int getUID() {
    const auto UID   = getuid();
    const auto PWUID = getpwuid(UID);
    return PWUID ? PWUID->pw_uid : UID;
}

std::string getRuntimeDir() {
    const auto XDG = getenv("XDG_RUNTIME_DIR");

    if (!XDG) {
        const std::string USERID = std::to_string(getUID());
        return "/run/user/" + USERID + "/hypr";
    }

    return std::string{XDG} + "/hypr";
}

static std::optional<uint64_t> toUInt64(const std::string_view str) {
    uint64_t value       = 0;
    const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec != std::errc() || ptr != str.data() + str.size())
        return std::nullopt;
    return value;
}

static std::optional<SInstanceData> parseInstance(const std::filesystem::directory_entry& entry) {
    if (!entry.is_directory())
        return std::nullopt;

    const auto    lockPath = entry.path() / "hyprland.lock";
    std::ifstream ifs(lockPath);
    if (!ifs.is_open())
        return std::nullopt;

    SInstanceData data;
    data.id = entry.path().filename().string();

    const auto first = std::string_view{data.id}.find_first_of('_');
    const auto last  = std::string_view{data.id}.find_last_of('_');
    if (first == std::string_view::npos || last == std::string_view::npos || last <= first)
        return std::nullopt;

    auto time = toUInt64(std::string_view{data.id}.substr(first + 1, last - first - 1));
    if (!time)
        return std::nullopt;
    data.time = *time;

    std::string line;
    if (!std::getline(ifs, line))
        return std::nullopt;

    auto pid = toUInt64(std::string_view{line});
    if (!pid)
        return std::nullopt;
    data.pid = *pid;

    if (!std::getline(ifs, data.wlSocket))
        return std::nullopt;

    if (std::getline(ifs, line) && !line.empty())
        return std::nullopt; // more lines than expected

    return data;
}

std::vector<SInstanceData> instances() {
    std::vector<SInstanceData> result;

    std::error_code            ec;
    const auto                 runtimeDir = getRuntimeDir();
    if (!std::filesystem::exists(runtimeDir, ec) || ec)
        return result;

    std::filesystem::directory_iterator it(runtimeDir, std::filesystem::directory_options::skip_permission_denied, ec);
    if (ec)
        return result;

    for (const auto& el : it) {
        if (auto instance = parseInstance(el))
            result.emplace_back(std::move(*instance));
    }

    std::erase_if(result, [](const auto& el) { return kill(el.pid, 0) != 0 && errno == ESRCH; });

    std::ranges::sort(result, {}, &SInstanceData::time);

    return result;
}

static volatile bool sigintReceived = false;
void                 intHandler(int sig) {
    sigintReceived = true;
    std::println("[hyprctl] SIGINT received, closing connection");
}

int rollingRead(const int socket) {
    sigintReceived = false;
    signal(SIGINT, intHandler);

    constexpr size_t              BUFFER_SIZE = 8192;
    std::array<char, BUFFER_SIZE> buffer      = {0};
    long                          sizeWritten = 0;
    std::println("[hyprctl] reading from socket following up log:");
    while (!sigintReceived) {
        sizeWritten = read(socket, buffer.data(), BUFFER_SIZE);
        if (sizeWritten < 0 && errno != EAGAIN) {
            if (errno != EINTR)
                std::println("Couldn't read (5): {}: {}", strerror(errno), errno);
            close(socket);
            return 5;
        }

        if (sizeWritten == 0)
            break;

        if (sizeWritten > 0) {
            std::println("{}", std::string(buffer.data(), sizeWritten));
            buffer.fill('\0');
        }

        usleep(100000);
    }
    close(socket);
    return 0;
}

int request(std::string_view arg, int minArgs = 0, bool needRoll = false) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        log("Couldn't open a socket (1)");
        return 1;
    }

    auto t = timeval{.tv_sec = 5, .tv_usec = 0};
    if (setsockopt(SERVERSOCKET, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(struct timeval)) < 0) {
        log("Couldn't set socket timeout (2)");
        return 2;
    }

    const auto ARGS = std::count(arg.begin(), arg.end(), ' ');

    if (ARGS < minArgs) {
        log(std::format("Not enough arguments in '{}', expected at least {}", arg, minArgs));
        return -1;
    }

    if (instanceSignature.empty()) {
        log("HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?) (3)");
        return 3;
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = getRuntimeDir() + "/" + instanceSignature + "/.socket.sock";

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, reinterpret_cast<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        log("Couldn't connect to " + socketPath + ". (4)");
        return 4;
    }

    auto sizeWritten = write(SERVERSOCKET, arg.data(), arg.size());

    if (sizeWritten < 0) {
        log("Couldn't write (5)");
        return 5;
    }

    if (needRoll)
        return rollingRead(SERVERSOCKET);

    std::string      reply               = "";
    constexpr size_t BUFFER_SIZE         = 8192;
    char             buffer[BUFFER_SIZE] = {0};

    sizeWritten = read(SERVERSOCKET, buffer, BUFFER_SIZE);

    if (sizeWritten < 0) {
        if (errno == EWOULDBLOCK)
            log("Hyprland IPC didn't respond in time\n");
        log("Couldn't read (6)");
        return 6;
    }

    reply += std::string(buffer, sizeWritten);

    while (sizeWritten == BUFFER_SIZE) {
        sizeWritten = read(SERVERSOCKET, buffer, BUFFER_SIZE);
        if (sizeWritten < 0) {
            log("Couldn't read (6)");
            return 6;
        }
        reply += std::string(buffer, sizeWritten);
    }

    close(SERVERSOCKET);

    log(reply);

    return 0;
}

int requestIPC(std::string_view filename, std::string_view arg) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        log("Couldn't open a socket (1)");
        return 1;
    }

    if (instanceSignature.empty()) {
        log("HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)");
        return 2;
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = getRuntimeDir() + "/" + instanceSignature + "/" + filename;

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, reinterpret_cast<sockaddr*>(&serverAddress), SUN_LEN(&serverAddress)) < 0) {
        log("Couldn't connect to " + socketPath + ". (3)");
        return 3;
    }

    arg = arg.substr(arg.find_first_of('/') + 1); // strip flags
    arg = arg.substr(arg.find_first_of(' ') + 1); // strip "hyprpaper"

    auto sizeWritten = write(SERVERSOCKET, arg.data(), arg.size());

    if (sizeWritten < 0) {
        log("Couldn't write (4)");
        return 4;
    }
    constexpr size_t BUFFER_SIZE         = 8192;
    char             buffer[BUFFER_SIZE] = {0};

    sizeWritten = read(SERVERSOCKET, buffer, BUFFER_SIZE);

    if (sizeWritten < 0) {
        log("Couldn't read (5)");
        return 5;
    }

    close(SERVERSOCKET);

    log(std::string(buffer));

    return 0;
}

int requestHyprpaper(std::string_view arg) {
    return requestIPC(".hyprpaper.sock", arg);
}

int requestHyprsunset(std::string_view arg) {
    return requestIPC(".hyprsunset.sock", arg);
}

void batchRequest(std::string_view arg, bool json) {
    std::string commands(arg.substr(arg.find_first_of(' ') + 1));

    if (json) {
        RE2::GlobalReplace(&commands, ";\\s*", ";j/");
        commands.insert(0, "j/");
    }

    std::string rq = "[[BATCH]]" + commands;
    request(rq);
}

void instancesRequest(bool json) {
    std::string result = "";

    // gather instance data
    std::vector<SInstanceData> inst = instances();

    if (!json) {
        for (auto const& el : inst) {
            result += std::format("instance {}:\n\ttime: {}\n\tpid: {}\n\twl socket: {}\n\n", el.id, el.time, el.pid, el.wlSocket);
        }
    } else {
        result += '[';
        for (auto const& el : inst) {
            result += std::format(R"#(
{{
    "instance": "{}",
    "time": {},
    "pid": {},
    "wl_socket": "{}"
}},)#",
                                  el.id, el.time, el.pid, el.wlSocket);
        }

        result.pop_back();
        result += "\n]";
    }

    log(result + "\n");
}

std::vector<std::string> splitArgs(int argc, char** argv) {
    std::vector<std::string> result;

    for (auto i = 1 /* skip the executable */; i < argc; ++i)
        result.emplace_back(argv[i]);

    return result;
}

int main(int argc, char** argv) {
    bool parseArgs = true;

    if (argc < 2) {
        std::println("{}", USAGE);
        return 1;
    }

    std::string fullRequest      = "";
    std::string fullArgs         = "";
    const auto  ARGS             = splitArgs(argc, argv);
    bool        json             = false;
    bool        needRoll         = false;
    std::string overrideInstance = "";

    for (std::size_t i = 0; i < ARGS.size(); ++i) {
        if (ARGS[i] == "--") {
            // Stop parsing arguments after --
            parseArgs = false;
            continue;
        }
        if (parseArgs && (ARGS[i][0] == '-') && !(isNumber(ARGS[i], true) || isNumber(ARGS[i].substr(0, ARGS[i].length() - 1), true)) /* For stuff like -2 or -2, */) {
            // parse
            if (ARGS[i] == "-j" && !fullArgs.contains("j")) {
                fullArgs += "j";
                json = true;
            } else if (ARGS[i] == "-r" && !fullArgs.contains("r")) {
                fullArgs += "r";
            } else if (ARGS[i] == "-a" && !fullArgs.contains("a")) {
                fullArgs += "a";
            } else if ((ARGS[i] == "-c" || ARGS[i] == "--config") && !fullArgs.contains("c")) {
                fullArgs += "c";
            } else if ((ARGS[i] == "-f" || ARGS[i] == "--follow") && !fullArgs.contains("f")) {
                fullArgs += "f";
                needRoll = true;
            } else if (ARGS[i] == "--batch") {
                fullRequest = "--batch ";
            } else if (ARGS[i] == "--instance" || ARGS[i] == "-i") {
                ++i;

                if (i >= ARGS.size()) {
                    std::println("{}", USAGE);
                    return 1;
                }

                overrideInstance = ARGS[i];
            } else if (ARGS[i] == "-q" || ARGS[i] == "--quiet") {
                quiet = true;
            } else if (ARGS[i] == "--help") {
                const std::string& cmd = ARGS[0];

                if (cmd == "hyprpaper") {
                    std::println("{}", HYPRPAPER_HELP);
                } else if (cmd == "hyprsunset") {
                    std::println("{}", HYPRSUNSET_HELP);
                } else if (cmd == "notify") {
                    std::println("{}", NOTIFY_HELP);
                } else if (cmd == "output") {
                    std::println("{}", OUTPUT_HELP);
                } else if (cmd == "plugin") {
                    std::println("{}", PLUGIN_HELP);
                } else if (cmd == "setprop") {
                    std::println("{}", SETPROP_HELP);
                } else if (cmd == "switchxkblayout") {
                    std::println("{}", SWITCHXKBLAYOUT_HELP);
                } else {
                    std::println("{}", USAGE);
                }

                return 1;
            } else {
                std::println("{}", USAGE);
                return 1;
            }

            continue;
        }

        fullRequest += ARGS[i] + " ";
    }

    if (fullRequest.empty()) {
        std::println("{}", USAGE);
        return 1;
    }

    fullRequest.pop_back(); // remove trailing space

    fullRequest = fullArgs + "/" + fullRequest;

    // instances is HIS-independent
    if (fullRequest.contains("/instances")) {
        instancesRequest(json);
        return 0;
    }

    if (needRoll && !fullRequest.contains("/rollinglog")) {
        log("only 'rollinglog' command supports '--follow' option");
        return 1;
    }

    if (overrideInstance.contains("_"))
        instanceSignature = overrideInstance;
    else if (!overrideInstance.empty()) {
        if (!isNumber(overrideInstance, false)) {
            log("instance invalid\n");
            return 1;
        }

        const auto INSTANCENO = std::stoi(overrideInstance);

        const auto INSTANCES = instances();

        if (INSTANCENO < 0 || static_cast<std::size_t>(INSTANCENO) >= INSTANCES.size()) {
            log("no such instance\n");
            return 1;
        }

        instanceSignature = INSTANCES[INSTANCENO].id;
    } else {
        const auto ISIG = getenv("HYPRLAND_INSTANCE_SIGNATURE");

        if (!ISIG) {
            log("HYPRLAND_INSTANCE_SIGNATURE not set! (is hyprland running?)\n");
            return 1;
        }

        instanceSignature = ISIG;
    }

    int exitStatus = 0;

    if (fullRequest.contains("/--batch"))
        batchRequest(fullRequest, json);
    else if (fullRequest.contains("/hyprpaper"))
        exitStatus = requestHyprpaper(fullRequest);
    else if (fullRequest.contains("/hyprsunset"))
        exitStatus = requestHyprsunset(fullRequest);
    else if (fullRequest.contains("/switchxkblayout"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/seterror"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/setprop"))
        exitStatus = request(fullRequest, 3);
    else if (fullRequest.contains("/plugin"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/dismissnotify"))
        exitStatus = request(fullRequest, 0);
    else if (fullRequest.contains("/notify"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/output"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/setcursor"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/dispatch"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/keyword"))
        exitStatus = request(fullRequest, 2);
    else if (fullRequest.contains("/decorations"))
        exitStatus = request(fullRequest, 1);
    else if (fullRequest.contains("/--help"))
        std::println("{}", USAGE);
    else if (fullRequest.contains("/rollinglog") && needRoll)
        exitStatus = request(fullRequest, 0, true);
    else {
        exitStatus = request(fullRequest);
    }

    std::cout << std::flush;
    return exitStatus;
}
