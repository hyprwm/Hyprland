#pragma once

#include <vector>
#include <string>
#include <cstdint>

struct SInstanceData {
    std::string id;
    uint64_t    time;
    uint64_t    pid;
    std::string wlSocket;
    bool        valid = true;
};

std::vector<SInstanceData> instances();
std::string getFromSocket(const std::string& cmd);