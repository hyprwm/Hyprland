#include "IHyprWindowDecoration.hpp"

#include "../../Window.hpp"

IHyprWindowDecoration::~IHyprWindowDecoration() {}

SWindowDecorationExtents IHyprWindowDecoration::getWindowDecorationReservedArea() {
    return SWindowDecorationExtents{};
}

bool IHyprWindowDecoration::allowsInput() {
    return false;
}