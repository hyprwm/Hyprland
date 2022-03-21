#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <string>
#include <fstream>
#include <string>

const std::string USAGE = R"#(
usage: hyprctl [command] [(opt)args]
    
    monitors
    workspaces
    clients
)#";

void request(std::string arg) {
    const auto SERVERSOCKET = socket(AF_INET, SOCK_STREAM, 0);

    if (SERVERSOCKET < 0) {
        std::cout << "Couldn't open a socket (1)";
        return;
    }

    const auto SERVER = gethostbyname("localhost");

    if (!SERVER) {
        std::cout << "Couldn't get host (2)";
        return;
    }

    sockaddr_in serverAddress = {0};
    serverAddress.sin_family = AF_INET;
    bcopy((char*)SERVER->h_addr, (char*)&serverAddress.sin_addr.s_addr, SERVER->h_length);

    std::ifstream socketPortStream;
    socketPortStream.open("/tmp/hypr/.socket");
    
    if (!socketPortStream.good()) {
        std::cout << "No socket port file (2a)";
        return;
    }

    std::string port = "";
    std::getline(socketPortStream, port);
    socketPortStream.close();

    int portInt = 0;
    try {
        portInt = std::stoi(port.c_str());
    } catch (...) {
        std::cout << "Port not an int?! (2b)";
        return;
    }

    if (portInt == 0) {
        std::cout << "Port not an int?! (2c)";
        return;
    }
    
    serverAddress.sin_port = portInt;

    if (connect(SERVERSOCKET, (sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
        std::cout << "Couldn't connect to port " << port << " (3) Is Hyprland running?";
        return;
    }

    auto sizeWritten = write(SERVERSOCKET, arg.c_str(), arg.length());

    if (sizeWritten < 0) {
        std::cout << "Couldn't write (4)";
        return;
    }

    char buffer[8192] = {0};

    sizeWritten = read(SERVERSOCKET,buffer, 8192);

    if (sizeWritten < 0) {
        std::cout << "Couldn't read (5)";
        return;
    }

    close(SERVERSOCKET);

    std::cout << std::string(buffer);
}

int main(int argc, char** argv) {
    int bflag = 0, sflag = 0, index, c;

    if (argc < 2) {
        printf(USAGE.c_str());
        return 1;
    }

    if (!strcmp(argv[1], "monitors")) request("monitors");
    else if (!strcmp(argv[1], "clients")) request("clients");
    else if (!strcmp(argv[1], "workspaces")) request("workspaces");
    else {
        printf(USAGE.c_str());
        return 1;
    }

    return 0;
}