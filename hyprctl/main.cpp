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
    dispatch
    keyword
    version
    kill
    splash
    hyprpaper
    reload
    
flags:
    j -> output in JSON
)#";

void request(std::string arg) {
    const auto SERVERSOCKET = socket(AF_UNIX, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        std::cout << "Couldn't open a socket (1)";
        return;
    }

    const auto SERVER = gethostbyname("localhost");

    if (!SERVER) {
        std::cout << "Couldn't get host (2)";
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
    serverAddress.sun_family = AF_UNIX;

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

    char buffer[8192] = {0};

    sizeWritten = read(SERVERSOCKET, buffer, 8192);

    if (sizeWritten < 0) {
        std::cout << "Couldn't read (5)";
        return;
    }

    close(SERVERSOCKET);

    std::cout << std::string(buffer);
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
    serverAddress.sun_family = AF_UNIX;

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

void dispatchRequest(int argc, char** argv) {

    if (argc < 4) {
        std::cout << "dispatch requires 2 params";
        return;
    }

    std::string rq = "/dispatch " + std::string(argv[2]) + " " + std::string(argv[3]);

    request(rq);
}

void keywordRequest(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "keyword requires 2 params";
        return;
    }

    std::string rq = "keyword " + std::string(argv[2]) + " " + std::string(argv[3]);

    request(rq);
}

void hyprpaperRequest(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "hyprpaper requires 2 params";
        return;
    }

    std::string rq = std::string(argv[2]) + " " + std::string(argv[3]);

    requestHyprpaper(rq);
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
    return std::ranges::all_of(str.begin(), str.end(), [&](char c) { return isdigit(c) != 0 || c == '-' || (allowfloat && c == '.'); });
}

int main(int argc, char** argv) {
    int bflag = 0, sflag = 0, index, c;

    if (argc < 2) {
        printf("%s\n", USAGE.c_str());
        return 1;
    }

    std::string fullRequest = "";
    std::string fullArgs = "";
    const auto ARGS = splitArgs(argc, argv);

    for (auto i = 0; i < ARGS.size(); ++i) {
        if (ARGS[i][0] == '-' && !isNumber(ARGS[i], true) /* For stuff like -2 */) {
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

    if (fullRequest.contains("/--batch")) batchRequest(fullRequest);
    else if (fullRequest.contains("/monitors")) request(fullRequest);
    else if (fullRequest.contains("/clients")) request(fullRequest);
    else if (fullRequest.contains("/workspaces")) request(fullRequest);
    else if (fullRequest.contains("/activewindow")) request(fullRequest);
    else if (fullRequest.contains("/layers")) request(fullRequest);
    else if (fullRequest.contains("/version")) request(fullRequest);
    else if (fullRequest.contains("/kill")) request(fullRequest);
    else if (fullRequest.contains("/splash")) request(fullRequest);
    else if (fullRequest.contains("/devices")) request(fullRequest);
    else if (fullRequest.contains("/reload")) request(fullRequest);
    else if (fullRequest.contains("/dispatch")) dispatchRequest(argc, argv);
    else if (fullRequest.contains("/keyword")) keywordRequest(argc, argv);
    else if (fullRequest.contains("/hyprpaper")) hyprpaperRequest(argc, argv);
    else if (fullRequest.contains("/--help")) printf("%s", USAGE.c_str());
    else {
        printf("%s\n", USAGE.c_str());
        return 1;
    }

    printf("\n");
    return 0;
}
