#pragma once

#include <string>
#include <vector>

struct SPlugin {
    std::string name;
    std::string filename;
    bool        enabled = false;
    bool        failed  = false;
};

struct SPluginRepository {
    std::string          url;
    std::string          name;
    std::vector<SPlugin> plugins;
    std::string          hash;
};