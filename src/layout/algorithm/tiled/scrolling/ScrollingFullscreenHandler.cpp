#include "managers/fullscreen/handler/FullscreenHandler.hpp"
#include "layout/algorithm/tiled/scrolling/ScrollingFullscreenHandler.hpp"

using namespace Fullscreen;
using namespace Fullscreen::ScrollingFullscreenHandler;

CScrollingFullscreenHandler::CScrollingFullscreenHandler(Layout::IModeAlgorithm* algorithm) : IFullscreenHandler(algorithm) {}

bool CScrollingFullscreenHandler::isFullscreen(const PHLWINDOW window, const std::optional<bool> covering) {

}
