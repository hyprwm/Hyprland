#pragma once

#include "IHyprWindowDecoration.hpp"

#include <string>

class CHyprToolkitDecoration;

struct SToolkitDecorationSpec {
    eDecorationEdges edge     = DECORATION_EDGE_TOP;
    int              size     = 0; // logical px, away from the edge
    uint32_t         priority = 10;
    bool             reserved = true;
    eDecorationLayer layer    = DECORATION_LAYER_OVER;
    bool             input    = true;
};

// What a provider keeps per decoration. Destroyed with the decoration, or when its provider goes away.
class IToolkitDecorationClient {
  public:
    virtual ~IToolkitDecorationClient() = default;
};

// Describes a toolkit decoration, and builds its widgets for each window it's added to.
class IToolkitDecorationProvider {
  public:
    virtual ~IToolkitDecorationProvider() = default;

    virtual const std::string&            name() const = 0;
    virtual const SToolkitDecorationSpec& spec() const = 0;

    // called once the decoration is on its window, its surface root is ready to be populated
    virtual UP<IToolkitDecorationClient> createClient(const SP<CHyprToolkitDecoration>& decoration) = 0;
};
