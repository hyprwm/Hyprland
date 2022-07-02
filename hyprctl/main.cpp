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

#include <iostream>
#include <string>
#include <fstream>
#include <string>

const std::string USAGE = R"#(usage: hyprctl [command] [(opt)args]
    
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
    hyprpaper
    reload)#";

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

    std::string rq = "dispatch " + std::string(argv[2]) + " " + std::string(argv[3]);

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

void batchRequest(int argc, char** argv) {
    std::string rq = "[[BATCH]]" + std::string(argv[2]);
    
    request(rq);
}

int main(int argc, char** argv) {
    int bflag = 0, sflag = 0, index, c;

    if (argc < 2) {
        printf("%s\n", USAGE.c_str());
        return 1;
    }

    if (!strcmp(argv[1], "monitors")) request("monitors");
    else if (!strcmp(argv[1], "clients")) request("clients");
    else if (!strcmp(argv[1], "workspaces")) request("workspaces");
    else if (!strcmp(argv[1], "activewindow")) request("activewindow");
    else if (!strcmp(argv[1], "layers")) request("layers");
    else if (!strcmp(argv[1], "version")) request("version");
    else if (!strcmp(argv[1], "kill")) request("kill");
    else if (!strcmp(argv[1], "devices")) request("devices");
    else if (!strcmp(argv[1], "reload")) request("reload");
    else if (!strcmp(argv[1], "dispatch")) dispatchRequest(argc, argv);
    else if (!strcmp(argv[1], "keyword")) keywordRequest(argc, argv);
    else if (!strcmp(argv[1], "hyprpaper")) hyprpaperRequest(argc, argv);
    else if (!strcmp(argv[1], "--batch")) batchRequest(argc, argv);
    else if (!strcmp(argv[1], "--help")) printf("%s", USAGE.c_str());
    else {
        printf("%s\n", USAGE.c_str());
        return 1;
    }

    printf("\n");
    return 0;
}
