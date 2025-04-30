#pragma once

#include <string>

namespace NSys {
    bool        isSuperuser();
    int         getUID();
    int         getEUID();
    std::string runAsSuperuser(const std::string& cmd);
    void        cacheSudo();
};