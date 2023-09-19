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
#include <unistd.h>
#include <ranges>
#include <algorithm>
#include <signal.h>

#include <iostream>
#include <string>
#include <fstream>
#include <string>
#include <vector>
#include <deque>
#include <filesystem>
#include <stdarg.h>

const std::string USAGE = R"#(usage:  hyprctl [flags] [<command> [args]]
        hyprctl --batch {<command 1> [args] ; <command 2> [args] ; ...}

LISTING COMMANDS:
    monitors:           List outputs
    workspaces:         List all workspaces
    activeworkspace:    Get currently active workspace
    clients:            List clients (e.g. windows)
    activewindow:       Get currently active window
    layers:             List layers
    animations:         List animations and bezier curves in use
    devices:            List devices
    binds:              List registered binds
    instances:          List running Hyprland instances
    globalshortcuts:    List global shortcuts
    version:            Print hyprland version

CONFIGURATION COMMANDS:
    keyword <keyword> [args]:   Execute a keyword
    getoption <option>:         Get value of <option>
    reload:                     Reload configurations

PLUGIN:
    plugin list:            List loaded plugins
    plugin load <path>:     Load plugin from <path>
    plugin unload <path>:   Unload plugin at <path>

THEMING:
    hyprpaper <keywords>        Issue hyprpaper keywords using IPC
    splash:                     Prints the current random splash
    cursorpos:                  Get the current cursor position in global layout coordinates
    setcursor <theme> <size>:   Set cursor theme and size, (except for GTK)

ADDITIONAL COMMANDS:
    dispatch <name> [args]:     Run a dispatcher
    kill:                       Enter kill mode, where you can kill an app by clicking on it,
                                use ESCAPE to quit kill mode
    switchxkblayout <args>:     Sets the xkb layout index for a keyboard, see wiki for details
    setprop <window> <prop>:    Set window property, see wiki for details
    seterror <color> <msg>:     Display <msg> as a error message, will reset upon reloading config
    seterror disable:           Clear error message
    notify <icon> <time_ms> <color> <message>:
                                Sends a notification using the built-in Hyprland notification system.
    output <args>:              Add and remove fake outputs to specified backend, see wiki for details.

FLAGS:
    -j -> output in JSON
    --help -> display this help
    --batch -> execute a batch of commands, separated by ';'
    --instance (-i) -> use a specific instance. Can be either signature or index in hyprctl instances (0, 1, etc)
)#";

#define PAD

std::string instanceSignature;

struct SInstanceData {
    std::string id;
    uint64_t    time;
    uint64_t    pid;
    std::string wlSocket;
    bool        valid = true;
};

std::vector<SInstanceData> instances() {
    std::vector<SInstanceData> result;

    for (const auto& el : std::filesystem::directory_iterator("/tmp/hypr")) {
        if (el.is_directory())
            continue;

        // read lock
        SInstanceData* data = &result.emplace_back();
        data->id            = el.path().string();
        data->id            = data->id.substr(data->id.find_last_of('/') + 1, data->id.find(".lock") - data->id.find_last_of('/') - 1);

        data->time = std::stoull(data->id.substr(data->id.find_first_of('_') + 1));

        // read file
        std::ifstream ifs(el.path().string());

        int           i = 0;
        for (std::string line; std::getline(ifs, line); ++i) {
            if (i == 0) {
                data->pid = std::stoull(line);
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

std::string getFormat(const char* fmt, ...) {
    char*   outputStr = nullptr;

    va_list args;
    va_start(args, fmt);
    vasprintf(&outputStr, fmt, args);
    va_end(args);

    std::string output = std::string(outputStr);
    free(outputStr);

    return output;
}

void request(std::string arg, int minArgs = 0) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    const auto ARGS = std::count(arg.begin(), arg.end(), ' ');

    if (ARGS < minArgs) {
        std::cout << "Not enough arguments, expected at least " << minArgs;
        return;
    }

    if (SERVERSOCKET < 0) {
        std::cout << "Couldn't open a socket (1)";
        return;
    }

    if (instanceSignature.empty()) {
        std::cout << "HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)";
        return;
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = "/tmp/hypr/" + instanceSignature + "/.socket.sock";

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)) < 0) {
        std::cout << "Couldn't connect to " << socketPath << ". (3)";
        return;
    }

    auto sizeWritten = write(SERVERSOCKET, arg.c_str(), arg.length());

    if (sizeWritten < 0) {
        std::cout << "Couldn't write (4)";
        return;
    }

    std::string reply        = "";
    char        buffer[8192] = {0};

    sizeWritten = read(SERVERSOCKET, buffer, 8192);

    if (sizeWritten < 0) {
        std::cout << "Couldn't read (5)";
        return;
    }

    reply += std::string(buffer, sizeWritten);

    while (sizeWritten == 8192) {
        sizeWritten = read(SERVERSOCKET, buffer, 8192);
        if (sizeWritten < 0) {
            std::cout << "Couldn't read (5)";
            return;
        }
        reply += std::string(buffer, sizeWritten);
    }

    close(SERVERSOCKET);

    std::cout << reply;
}

void requestHyprpaper(std::string arg) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        std::cout << "Couldn't open a socket (1)";
        return;
    }

    if (instanceSignature.empty()) {
        std::cout << "HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)";
        return;
    }

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = "/tmp/hypr/" + instanceSignature + "/.hyprpaper.sock";

    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);

    if (connect(SERVERSOCKET, (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)) < 0) {
        std::cout << "Couldn't connect to " << socketPath << ". (3)";
        return;
    }

    arg = arg.substr(arg.find_first_of('/') + 1); // strip flags
    arg = arg.substr(arg.find_first_of(' ') + 1); // strip "hyprpaper"

    auto sizeWritten = write(SERVERSOCKET, arg.c_str(), arg.length());

    if (sizeWritten < 0) {
        std::cout << "Couldn't write (4)";
        return;
    }

    char buffer[8192] = {0};

    sizeWritten = read(SERVERSOCKET, buffer, 8192);

    if (sizeWritten < 0) {
        std::cout << "Couldn't read (5)";
        return;
    }

    close(SERVERSOCKET);

    std::cout << std::string(buffer);
}

void batchRequest(std::string arg) {
    std::string rq = "[[BATCH]]" + arg.substr(arg.find_first_of(" ") + 1);

    request(rq);
}

void instancesRequest(bool json) {
    std::string result = "";

    // gather instance data
    std::vector<SInstanceData> inst = instances();

    if (!json) {
        for (auto& el : inst) {
            result += getFormat("instance %s:\n\ttime: %llu\n\tpid: %llu\n\twl socket: %s\n\n", el.id.c_str(), el.time, el.pid, el.wlSocket.c_str());
        }
    } else {
        result += '[';
        for (auto& el : inst) {
            result += getFormat(R"#(
{
    "instance": "%s",
    "time": %llu,
    "pid": %llu,
    "wl_socket": "%s"
},)#",
                                el.id.c_str(), el.time, el.pid, el.wlSocket.c_str());
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
        printf("%s\n", USAGE.c_str());
        return 1;
    }

    std::string fullRequest      = "";
    std::string fullArgs         = "";
    const auto  ARGS             = splitArgs(argc, argv);
    bool        json             = false;
    std::string overrideInstance = "";

    for (size_t i = 0; i < ARGS.size(); ++i) {
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
            } else if (ARGS[i] == "--batch") {
                fullRequest = "--batch ";
            } else if (ARGS[i] == "--instance" || ARGS[i] == "-i") {
                ++i;

                if (i >= ARGS.size()) {
                    printf("%s\n", USAGE.c_str());
                    return 1;
                }

                overrideInstance = ARGS[i];
            } else {
                printf("%s\n", USAGE.c_str());
                return 1;
            }

            continue;
        }

        fullRequest += ARGS[i] + " ";
    }

    if (fullRequest.empty()) {
        printf("%s\n", USAGE.c_str());
        return 1;
    }

    fullRequest.pop_back(); // remove trailing space

    fullRequest = fullArgs + "/" + fullRequest;

    if (overrideInstance.contains("_"))
        instanceSignature = overrideInstance;
    else if (!overrideInstance.empty()) {
        if (!isNumber(overrideInstance, false)) {
            std::cout << "instance invalid\n";
            return 1;
        }

        const auto INSTANCENO = std::stoi(overrideInstance);

        const auto INSTANCES = instances();

        if (INSTANCENO < 0 || (size_t)INSTANCENO >= INSTANCES.size()) {
            std::cout << "no such instance\n";
            return 1;
        }

        instanceSignature = INSTANCES[INSTANCENO].id;
    } else {
        const auto ISIG = getenv("HYPRLAND_INSTANCE_SIGNATURE");

        if (!ISIG) {
            std::cout << "HYPRLAND_INSTANCE_SIGNATURE not set! (is hyprland running?)";
            return 1;
        }

        instanceSignature = ISIG;
    }

    int exitStatus = 0;

    if (fullRequest.contains("/--batch"))
        batchRequest(fullRequest);
    else if (fullRequest.contains("/monitors"))
        request(fullRequest);
    else if (fullRequest.contains("/clients"))
        request(fullRequest);
    else if (fullRequest.contains("/workspaces"))
        request(fullRequest);
    else if (fullRequest.contains("/activeworkspace"))
        request(fullRequest);
    else if (fullRequest.contains("/activewindow"))
        request(fullRequest);
    else if (fullRequest.contains("/layers"))
        request(fullRequest);
    else if (fullRequest.contains("/version"))
        request(fullRequest);
    else if (fullRequest.contains("/kill"))
        request(fullRequest);
    else if (fullRequest.contains("/splash"))
        request(fullRequest);
    else if (fullRequest.contains("/devices"))
        request(fullRequest);
    else if (fullRequest.contains("/reload"))
        request(fullRequest);
    else if (fullRequest.contains("/getoption"))
        request(fullRequest);
    else if (fullRequest.contains("/binds"))
        request(fullRequest);
    else if (fullRequest.contains("/cursorpos"))
        request(fullRequest);
    else if (fullRequest.contains("/animations"))
        request(fullRequest);
    else if (fullRequest.contains("/globalshortcuts"))
        request(fullRequest);
    else if (fullRequest.contains("/instances"))
        instancesRequest(json);
    else if (fullRequest.contains("/switchxkblayout"))
        request(fullRequest, 2);
    else if (fullRequest.contains("/seterror"))
        request(fullRequest, 1);
    else if (fullRequest.contains("/setprop"))
        request(fullRequest, 3);
    else if (fullRequest.contains("/plugin"))
        request(fullRequest, 1);
    else if (fullRequest.contains("/notify"))
        request(fullRequest, 2);
    else if (fullRequest.contains("/output"))
        request(fullRequest, 2);
    else if (fullRequest.contains("/setcursor"))
        request(fullRequest, 1);
    else if (fullRequest.contains("/dispatch"))
        request(fullRequest, 1);
    else if (fullRequest.contains("/keyword"))
        request(fullRequest, 2);
    else if (fullRequest.contains("/hyprpaper"))
        requestHyprpaper(fullRequest);
    else if (fullRequest.contains("/--help"))
        printf("%s", USAGE.c_str());
    else {
        printf("%s\n", USAGE.c_str());
        return 1;
    }

    printf("\n");
    return exitStatus;
}
