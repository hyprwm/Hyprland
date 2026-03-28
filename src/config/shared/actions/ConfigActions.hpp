#pragma once

#include <expected>
#include <string>
#include <optional>

#include "../../../desktop/DesktopTypes.hpp"
#include "../../../desktop/Workspace.hpp"
#include "../../../helpers/math/Direction.hpp"

namespace Config::Actions {
    struct SActionResult {
        bool passEvent = false;
    };

    enum eTogglableAction : uint8_t {
        TOGGLE_ACTION_TOGGLE = 0,
        TOGGLE_ACTION_ENABLE,
        TOGGLE_ACTION_DISABLE,
    };

    using ActionResult = std::expected<SActionResult, std::string>;

    ActionResult closeWindow(std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult killWindow(std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult signalWindow(int sig, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult floatWindow(eTogglableAction action, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult pseudoWindow(eTogglableAction action, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult pinWindow(eTogglableAction action, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult fullscreenWindow(eFullscreenMode mode, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult fullscreenWindow(eFullscreenMode internalMode, eFullscreenMode clientMode, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult moveToWorkspace(PHLWORKSPACE ws, bool silent, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult moveFocus(Math::eDirection dir);
    ActionResult focus(PHLWINDOW window);
    ActionResult moveInDirection(Math::eDirection dir, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult swapInDirection(Math::eDirection dir, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult focusCurrentOrLast();
    ActionResult focusUrgentOrLast();
    ActionResult center(std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult moveCursorToCorner(int corner, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult resizeBy(const Vector2D& Δ, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult moveBy(const Vector2D& Δ, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult cycleNext(const bool next, std::optional<bool> onlyTiled, std::optional<bool> onlyFloating, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult tag(const std::string& tag, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult pass(std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult pass(uint32_t modMask, uint32_t key, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult sendKeyState(uint32_t modMask, uint32_t key, uint32_t state, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult swapNext(const bool next, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult alterZOrder(const std::string& mode, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult setProp(const std::string& prop, const std::string& val, std::optional<PHLWINDOW> window = std::nullopt /* Active */);

    ActionResult toggleGroup(std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult changeGroupActive(bool forward = true, std::optional<PHLWINDOW> window = std::nullopt /* Active */);

    ActionResult changeWorkspace(PHLWORKSPACE ws);
    ActionResult changeWorkspace(const std::string& ws);
    ActionResult renameWorkspace(PHLWORKSPACE ws, const std::string& s);
    ActionResult moveToMonitor(PHLWORKSPACE ws, PHLMONITOR mon);
    ActionResult changeWorkspaceOnCurrentMonitor(PHLWORKSPACE ws);
    ActionResult toggleSpecial(PHLWORKSPACE special);

    ActionResult focusMonitor(PHLMONITOR mon);
    ActionResult swapActiveWorkspaces(PHLMONITOR mon1, PHLMONITOR mon2);

    ActionResult layoutMessage(const std::string& msg);

    ActionResult moveCursor(const Vector2D& pos);
    ActionResult exit();
    ActionResult forceRendererReload();
    ActionResult toggleSwallow();
    ActionResult setSubmap(const std::string& submap);
    ActionResult dpms(eTogglableAction action, std::optional<PHLMONITOR> mon);
    ActionResult forceIdle(float seconds);
    ActionResult global(const std::string& action);
    ActionResult event(const std::string& data);

    ActionResult mouse(const std::string& action);

    ActionResult lockGroups(eTogglableAction action);
    ActionResult lockActiveGroup(eTogglableAction action);
    ActionResult moveIntoGroup(Math::eDirection direction, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult moveOutOfGroup(Math::eDirection direction, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult moveGroupWindow(bool forward = true);
    ActionResult moveWindowOrGroup(Math::eDirection direction, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult denyWindowFromGroup(eTogglableAction action);
    ActionResult moveIntoOrCreateGroup(Math::eDirection dir, std::optional<PHLWINDOW> window = std::nullopt /* Active */);

    class CActionState {
      public:
        CActionState()  = default;
        ~CActionState() = default;

        int         m_passPressed   = -1; // -1 = dynamic (press+release), 0 = released, 1 = pressed
        uint32_t    m_lastCode      = 0;  // last keycode (keyboard event), 0 if last was mouse
        uint32_t    m_lastMouseCode = 0;  // last mouse button code, 0 if last was keyboard
        uint32_t    m_timeLastMs    = 0;  // timestamp of last key/mouse event
        std::string m_currentSubmap = ""; // current keybind submap name
    };

    UP<CActionState>& state();
};