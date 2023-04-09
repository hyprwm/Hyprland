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

#include <iostream>
#include <string>
#include <fstream>
#include <string>
#include <deque>

const std::string USAGE = R"#(usage: hyprctl [(opt)flags] [command] [(opt)args]

commands:
    monitors
    workspaces
    clients
    activewindow
    layers
    devices
    binds
    dispatch
    keyword
    version
    kill
    splash
    hyprpaper
    reload
    setcursor
    getoption
    cursorpos
    switchxkblayout
    seterror
    setprop
    plugin
    notify
    globalshortcuts

flags:
    -j -> output in JSON
    --batch -> execute a batch of commands, separated by ';'
)#";

void              request(std::string arg, int minArgs = 0) {
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

    // get the instance signature
    auto instanceSig = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (!instanceSig) {
        std::cout << "HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)";
        return;
    }

    std::string instanceSigStr = std::string(instanceSig);

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = "/tmp/hypr/" + instanceSigStr + "/.socket.sock";

    strcpy(serverAddress.sun_path, socketPath.c_str());

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

    // get the instance signature
    auto instanceSig = getenv("HYPRLAND_INSTANCE_SIGNATURE");

    if (!instanceSig) {
        std::cout << "HYPRLAND_INSTANCE_SIGNATURE was not set! (Is Hyprland running?)";
        return;
    }

    std::string instanceSigStr = std::string(instanceSig);

    sockaddr_un serverAddress = {0};
    serverAddress.sun_family  = AF_UNIX;

    std::string socketPath = "/tmp/hypr/" + instanceSigStr + "/.hyprpaper.sock";

    strcpy(serverAddress.sun_path, socketPath.c_str());

    if (connect(SERVERSOCKET, (sockaddr*)&serverAddress, SUN_LEN(&serverAddress)) < 0) {
        std::cout << "Couldn't connect to " << socketPath << ". (3)";
        return;
    }

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

int dispatchRequest(int argc, char** argv) {

    if (argc < 3) {
        std::cout << "Usage: hyprctl dispatch <dispatcher> <arg>\n\
            Execute a hyprland keybind dispatcher with the given argument";
        return 1;
    }

    std::string rq = "/dispatch";

    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--"))
            continue;
        rq += " " + std::string(argv[i]);
    }

    request(rq);
    return 0;
}

int keywordRequest(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: hyprctl keyword <keyword> <arg>\n\
            Execute a hyprland keyword with the given argument";
        return 1;
    }

    std::string rq = "/keyword";

    for (int i = 2; i < argc; i++)
        rq += " " + std::string(argv[i]);

    request(rq);
    return 0;
}

int hyprpaperRequest(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: hyprctl hyprpaper <command> <arg>\n\
            Execute a hyprpaper command with the given argument";
        return 1;
    }

    std::string rq = std::string(argv[2]) + " " + std::string(argv[3]);

    requestHyprpaper(rq);
    return 0;
}

int setcursorRequest(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: hyprctl setcursor <theme> <size>\n\
            Sets the cursor theme for everything except GTK and reloads the cursor";
        return 1;
    }

    std::string rq = "setcursor " + std::string(argv[2]) + " " + std::string(argv[3]);

    request(rq);
    return 0;
}

int outputRequest(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: hyprctl output <mode> <name>\n\
            creates / destroys a fake output\n\
            with create, name is the backend name to use (available: auto, x11, wayland, headless)\n\
            with destroy, name is the output name to destroy";
        return 1;
    }

    std::string rq = "output " + std::string(argv[2]) + " " + std::string(argv[3]);

    request(rq);
    return 0;
}

void batchRequest(std::string arg) {
    std::string rq = "[[BATCH]]" + arg.substr(arg.find_first_of(" ") + 1);

    request(rq);
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
    int  bflag = 0, sflag = 0, index, c;
    bool parseArgs = true;

    if (argc < 2) {
        printf("%s\n", USAGE.c_str());
        return 1;
    }

    std::string fullRequest = "";
    std::string fullArgs    = "";
    const auto  ARGS        = splitArgs(argc, argv);

    for (auto i = 0; i < ARGS.size(); ++i) {
        if (ARGS[i] == "--") {
            // Stop parsing arguments after --
            parseArgs = false;
            continue;
        }
        if (parseArgs && (ARGS[i][0] == '-') && !isNumber(ARGS[i], true) /* For stuff like -2 */) {
            // parse
            if (ARGS[i] == "-j" && !fullArgs.contains("j")) {
                fullArgs += "j";
            } else if (ARGS[i] == "--batch") {
                fullRequest = "--batch ";
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

    int exitStatus = 0;

    if (fullRequest.contains("/--batch"))
        batchRequest(fullRequest);
    else if (fullRequest.contains("/monitors"))
        request(fullRequest);
    else if (fullRequest.contains("/clients"))
        request(fullRequest);
    else if (fullRequest.contains("/workspaces"))
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
        exitStatus = outputRequest(argc, argv);
    else if (fullRequest.contains("/setcursor"))
        exitStatus = setcursorRequest(argc, argv);
    else if (fullRequest.contains("/dispatch"))
        exitStatus = dispatchRequest(argc, argv);
    else if (fullRequest.contains("/keyword"))
        exitStatus = keywordRequest(argc, argv);
    else if (fullRequest.contains("/hyprpaper"))
        exitStatus = hyprpaperRequest(argc, argv);
    else if (fullRequest.contains("/--help"))
        printf("%s", USAGE.c_str());
    else {
        printf("%s\n", USAGE.c_str());
        return 1;
    }

    printf("\n");
    return exitStatus;
}
