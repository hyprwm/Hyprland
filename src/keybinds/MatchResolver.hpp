#pragma once

#include "Registry.hpp"

#include <span>
#include <vector>

namespace Keybinds {

    struct SBindMatchCandidate {
        PBind      bind;
        eBindMatch match = BIND_MATCH_NONE;
    };

    struct SChordMatchResolution {
        std::vector<PBind> immediate;
        std::vector<PBind> deferred;
        std::vector<PBind> partial;
    };

    SChordMatchResolution resolveChordMatches(std::span<const SBindMatchCandidate> candidates, const SBindEventContext& context);
}
