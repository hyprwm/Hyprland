#pragma once

#include "../helpers/Memory.hpp"

#include <span>
#include <optional>

struct SState {
    std::span<const char*>     rawArgvNoBinPath;
    std::optional<std::string> customPath;
    bool                       noNixGl    = false;
    bool                       forceNixGl = false;
};

inline UP<SState> g_state = makeUnique<SState>();