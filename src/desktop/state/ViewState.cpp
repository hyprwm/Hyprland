#include "ViewState.hpp"
#include "LayerState.hpp"
#include "OtherViewState.hpp"
#include "WindowState.hpp"

using namespace Desktop;

const std::vector<PHLWINDOW>& CViewState::windows() const {
    return windowState()->windows();
}

const std::vector<PHLLS>& CViewState::layers() const {
    return layerState()->layers();
}

const std::vector<PHLVIEWREF>& CViewState::otherViews() const {
    return otherViewState()->views();
}

UP<CViewState>& Desktop::viewState() {
    static UP<CViewState> state = makeUnique<CViewState>();
    return state;
}
