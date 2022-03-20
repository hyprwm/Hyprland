#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <string.h>
#include <string>

const std::string USAGE = R"#(
usage: hyprctl [command] [(opt)args]
    
    monitors
    workspaces
    clients
)#";

void readReply() {
    std::ifstream ifs;

    while (1) {
        usleep(1000 * 25);

        ifs.open("/tmp/hypr/.hyprlandrq");

        if (ifs.good()) {
            std::string reply = "";
            std::getline(ifs, reply);

            if (reply.find("RPLY:") != std::string::npos) {
                reply = "";
                std::string temp = "";
                while (std::getline(ifs, temp))
                    reply += temp + '\n';

                std::cout << reply;

                unlink("/tmp/hypr/.hyprlandrq"); // cleanup
                break;
            }
        }
    }
}

void requestMonitors() {
    std::ofstream ofs;
    ofs.open("/tmp/hypr/.hyprlandrq", std::ios::trunc);

    ofs << "R>monitors";

    ofs.close();

    readReply();
}

void requestClients() {
    std::ofstream ofs;
    ofs.open("/tmp/hypr/.hyprlandrq", std::ios::trunc);

    ofs << "R>clients";

    ofs.close();

    readReply();
}

void requestWorkspaces() {
    std::ofstream ofs;
    ofs.open("/tmp/hypr/.hyprlandrq", std::ios::trunc);

    ofs << "R>workspaces";

    ofs.close();

    readReply();
}

int main(int argc, char** argv) {
    int bflag = 0, sflag = 0, index, c;

    if (argc < 2) {
        printf(USAGE.c_str());
        return 1;
    }

    if (!strcmp(argv[1], "monitors")) requestMonitors();
    else if (!strcmp(argv[1], "clients")) requestClients();
    else if (!strcmp(argv[1], "workspaces")) requestWorkspaces();
    else {
        printf(USAGE.c_str());
        return 1;
    }

    return 0;
}