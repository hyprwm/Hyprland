#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <pwd.h>
#include <unistd.h>
#include <ranges>
#include <algorithm>
#include <signal.h>
#include <format>

#include <iostream>
#include <string>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <filesystem>
#include <stdarg.h>
#include <regex>

#include "Strings.hpp"

#define PAD

std::string instanceSignature;

struct SInstanceData {
    std::string id;
    uint64_t    time;
    uint64_t    pid;
    std::string wlSocket;
    bool        valid = true;
};

std::string getRuntimeDir() {
    const auto XDG = getenv("XDG_RUNTIME_DIR");

    if (!XDG) {
        const std::string USERID = std::to_string(getpwuid(getuid())->pw_uid);
        return "/run/user/" + USERID + "/hypr";
    }

    return std::string{XDG} + "/hypr";
}

std::vector<SInstanceData> instances() {
    std::vector<SInstanceData> result;

    for (const auto& el : std::filesystem::directory_iterator(getRuntimeDir())) {
        if (!el.is_directory() || !std::filesystem::exists(el.path().string() + "/hyprland.lock"))
            continue;

        // read lock
        SInstanceData* data = &result.emplace_back();
        data->id            = el.path().filename().string();

        try {
            data->time = std::stoull(data->id.substr(data->id.find_first_of('_') + 1, data->id.find_last_of('_') - (data->id.find_first_of('_') + 1)));
        } catch (std::exception& e) { continue; }

        // read file
        std::ifstream ifs(el.path().string() + "/hyprland.lock");

        int           i = 0;
        for (std::string line; std::getline(ifs, line); ++i) {
            if (i == 0) {
                try {
                    data->pid = std::stoull(line);
                } catch (std::exception& e) { continue; }
            } else if (i == 1) {
                data->wlSocket = line;
            } else
                break;
        }

        ifs.close();
    }

    std::erase_if(result, [&](const auto& el) { return kill(el.pid, 0) != 0 && errno == ESRCH; });

    std::sort(result.begin(), result.end(), [&](const auto& a, const auto& b) { return a.time < b.time; });

    return result;
}

int request(std::string arg, int minArgs = 0) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    const auto ARGS = std::count(arg.begin(), arg.end(), ' ');

    if (ARGS < minArgs) {
        std::cout << "Not enough arguments, expected at least " << minArgs;
        return -1;
    }

    if (SERVERSOCKET < 0) {
        std::cout << "Couldn't open a socket (1)";
        return 1;
    }

    if (instanceSignature.empty()) {
        std::cout << "HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)";
        return 2;
    }

    const std::string USERID = std::to_string(getpwuid(getuid())->pw_uid);

    sockaddr_un       serverAddress = {0};
    serverAddress.sun_family        = AF_UNIX;

    std::string socketPath = getRuntimeDir() + "/" + instanceSignature + "/.socket.sock";

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)) < 0) {
        std::cout << "Couldn't connect to " << socketPath << ". (3)";
        return 3;
    }

    auto sizeWritten = write(SERVERSOCKET, arg.c_str(), arg.length());

    if (sizeWritten < 0) {
        std::cout << "Couldn't write (4)";
        return 4;
    }

    std::string reply        = "";
    char        buffer[8192] = {0};

    sizeWritten = read(SERVERSOCKET, buffer, 8192);

    if (sizeWritten < 0) {
        std::cout << "Couldn't read (5)";
        return 5;
    }

    reply += std::string(buffer, sizeWritten);

    while (sizeWritten == 8192) {
        sizeWritten = read(SERVERSOCKET, buffer, 8192);
        if (sizeWritten < 0) {
            std::cout << "Couldn't read (5)";
            return 5;
        }
        reply += std::string(buffer, sizeWritten);
    }

    close(SERVERSOCKET);

    std::cout << reply;

    return 0;
}

int requestHyprpaper(std::string arg) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        std::cout << "Couldn't open a socket (1)";
        return 1;
    }

    if (instanceSignature.empty()) {
        std::cout << "HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)";
        return 2;
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    const std::string USERID = std::to_string(getpwuid(getuid())->pw_uid);

    std::string       socketPath = getRuntimeDir() + "/" + instanceSignature + "/.hyprpaper.sock";

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)) < 0) {
        std::cout << "Couldn't connect to " << socketPath << ". (3)";
        return 3;
    }

    arg = arg.substr(arg.find_first_of('/') + 1); // strip flags
    arg = arg.substr(arg.find_first_of(' ') + 1); // strip "hyprpaper"

    auto sizeWritten = write(SERVERSOCKET, arg.c_str(), arg.length());

    if (sizeWritten < 0) {
        std::cout << "Couldn't write (4)";
        return 4;
    }

    char buffer[8192] = {0};

    sizeWritten = read(SERVERSOCKET, buffer, 8192);

    if (sizeWritten < 0) {
        std::cout << "Couldn't read (5)";
        return 5;
    }

    close(SERVERSOCKET);

    std::cout << std::string(buffer);

    return 0;
}

void batchRequest(std::string arg, bool json) {
    std::string commands = arg.substr(arg.find_first_of(" ") + 1);

    if (json) {
        commands = "j/" + std::regex_replace(commands, std::regex(";\\s*"), ";j/");
    }

    std::string rq = "[[BATCH]]" + commands;
    request(rq);
}

void instancesRequest(bool json) {
    std::string result = "";

    // gather instance data
    std::vector<SInstanceData> inst = instances();

    if (!json) {
        for (auto& el : inst) {
            result += std::format("instance {}:\n\ttime: {}\n\tpid: {}\n\twl socket: {}\n\n", el.id, el.time, el.pid, el.wlSocket);
        }
    } else {
        result += '[';
        for (auto& el : inst) {
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

    std::cout << result << "\n";
}

std::deque<std::string> splitArgs(int argc, char** argv) {
    std::deque<std::string> result;

    for (auto i = 1 /* skip the executable */; i < argc; ++i)
        result.push_back(std::string(argv[i]));

    return result;
}

bool isNumber(const std::string& str, bool allowfloat) {
    if (str.empty())
        return false;
    return std::ranges::all_of(str.begin(), str.end(), [&](char c) { return isdigit(c) != 0 || c == '-' || (allowfloat && c == '.'); });
}

int main(int argc, char** argv) {
    bool parseArgs = true;

    if (argc < 2) {
        std::cout << USAGE << std::endl;
        return 1;
    }

    std::string fullRequest      = "";
    std::string fullArgs         = "";
    const auto  ARGS             = splitArgs(argc, argv);
    bool        json             = false;
    std::string overrideInstance = "";

    for (std::size_t i = 0; i < ARGS.size(); ++i) {
        if (ARGS[i] == "--") {
            // Stop parsing arguments after --
            parseArgs = false;
            continue;
        }
        if (parseArgs && (ARGS[i][0] == '-') && !isNumber(ARGS[i], true) /* For stuff like -2 */) {
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
            } else if (ARGS[i] == "--batch") {
                fullRequest = "--batch ";
            } else if (ARGS[i] == "--instance" || ARGS[i] == "-i") {
                ++i;

                if (i >= ARGS.size()) {
                    std::cout << USAGE << std::endl;
                    return 1;
                }

                overrideInstance = ARGS[i];
            } else if (ARGS[i] == "--help") {
                const std::string& cmd = ARGS[0];

                if (cmd == "hyprpaper") {
                    std::cout << HYPRPAPER_HELP << std::endl;
                } else if (cmd == "notify") {
                    std::cout << NOTIFY_HELP << std::endl;
                } else if (cmd == "output") {
                    std::cout << OUTPUT_HELP << std::endl;
                } else if (cmd == "plugin") {
                    std::cout << PLUGIN_HELP << std::endl;
                } else if (cmd == "setprop") {
                    std::cout << SETPROP_HELP << std::endl;
                } else if (cmd == "switchxkblayout") {
                    std::cout << SWITCHXKBLAYOUT_HELP << std::endl;
                } else {
                    std::cout << USAGE << std::endl;
                }

                return 1;
            } else {
                std::cout << USAGE << std::endl;
                return 1;
            }

            continue;
        }

        fullRequest += ARGS[i] + " ";
    }

    if (fullRequest.empty()) {
        std::cout << USAGE << std::endl;
        return 1;
    }

    fullRequest.pop_back(); // remove trailing space

    fullRequest = fullArgs + "/" + fullRequest;

    // instances is HIS-independent
    if (fullRequest.contains("/instances")) {
        instancesRequest(json);
        return 0;
    }

    if (overrideInstance.contains("_"))
        instanceSignature = overrideInstance;
    else if (!overrideInstance.empty()) {
        if (!isNumber(overrideInstance, false)) {
            std::cout << "instance invalid\n";
            return 1;
        }

        const auto INSTANCENO = std::stoi(overrideInstance);

        const auto INSTANCES = instances();

        if (INSTANCENO < 0 || static_cast<std::size_t>(INSTANCENO) >= INSTANCES.size()) {
            std::cout << "no such instance\n";
            return 1;
        }

        instanceSignature = INSTANCES[INSTANCENO].id;
    } else {
        const auto ISIG = getenv("HYPRLAND_INSTANCE_SIGNATURE");

        if (!ISIG) {
            std::cout << "HYPRLAND_INSTANCE_SIGNATURE not set! (is hyprland running?)\n";
            return 1;
        }

        instanceSignature = ISIG;
    }

    int exitStatus = 0;

    if (fullRequest.contains("/--batch"))
        batchRequest(fullRequest, json);
    else if (fullRequest.contains("/hyprpaper"))
        exitStatus = requestHyprpaper(fullRequest);
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
        std::cout << USAGE << std::endl;
    else {
        exitStatus = request(fullRequest);
    }

    std::cout << std::endl;
    return exitStatus;
}
