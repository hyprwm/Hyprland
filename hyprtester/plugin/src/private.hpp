#pragma once

#include "state/workspace/Resolver.hpp"
#include <unistd.h>
#include <src/includes.hpp>
#include <sstream>
#include <any>
#include <cmath>
#include <vector>

#define private public
#include <src/managers/input/InputManager.hpp>
#include <src/pointer/PointerManager.hpp>
#include <src/pointer/PointerController.hpp>
#include <src/managers/SeatManager.hpp>
#include <src/managers/input/trackpad/TrackpadGestures.hpp>
#include <src/output/Monitor.hpp>
#include <src/desktop/rule/windowRule/WindowRuleEffectContainer.hpp>
#include <src/desktop/rule/layerRule/LayerRuleEffectContainer.hpp>
#include <src/desktop/rule/windowRule/WindowRuleApplicator.hpp>
#include <src/desktop/view/LayerSurface.hpp>
#include <src/desktop/view/window/WindowFullscreenPolicy.hpp>
#include <src/desktop/view/window/WindowPresentation.hpp>
#include <src/desktop/state/ViewState.hpp>
#include <src/desktop/state/WindowState.hpp>
#include <src/workspace/HLWorkspace.hpp>
#include <src/layout/target/Target.hpp>
#include <src/keybinds/Key.hpp>
#include <src/Compositor.hpp>
#include <src/desktop/state/FocusState.hpp>
#include <src/state/MonitorState.hpp>
#include <src/state/workspace/State.hpp>
#include <src/layout/LayoutManager.hpp>
#include <src/event/EventBus.hpp>
#undef private
