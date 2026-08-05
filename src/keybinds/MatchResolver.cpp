#include "MatchResolver.hpp"

#include <algorithm>

using namespace Keybinds;

SChordMatchResolution Keybinds::resolveChordMatches(std::span<const SBindMatchCandidate> candidates, const SBindEventContext& context) {
    SChordMatchResolution result;

    for (const auto& candidate : candidates) {
        if (candidate.match == BIND_MATCH_PARTIAL) {
            result.partial.emplace_back(candidate.bind);
            continue;
        }

        if (candidate.match != BIND_MATCH_FULL)
            continue;

        const bool SHADOWED_BY_LONGER_FULL = std::ranges::any_of(
            candidates, [&](const auto& other) { return other.match == BIND_MATCH_FULL && candidate.bind != other.bind && candidate.bind->isSubChordOf(*other.bind, context); });
        if (SHADOWED_BY_LONGER_FULL)
            continue;

        const bool EXTENDABLE = context.pressed &&
            std::ranges::any_of(candidates, [&](const auto& other) { return other.match == BIND_MATCH_PARTIAL && candidate.bind->isOrderedPrefixOf(*other.bind, context); });
        if (EXTENDABLE)
            result.deferred.emplace_back(candidate.bind);
        else
            result.immediate.emplace_back(candidate.bind);
    }

    return result;
}
