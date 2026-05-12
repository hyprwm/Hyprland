#include "CursorShapeOverrideController.hpp"

#include <ranges>

using namespace Cursor;

void CShapeOverrideController::setOverride(const std::string& name, eCursorShapeOverrideGroup group) {
    if (m_overrides[group] == name)
        return;

    m_overrides[group] = name;

    recheckOverridesResendIfChanged();
}

void CShapeOverrideController::unsetOverride(eCursorShapeOverrideGroup group) {
    if (m_overrides[group].empty())
        return;

    m_overrides[group] = "";

    recheckOverridesResendIfChanged();
}

void CShapeOverrideController::recheckOverridesResendIfChanged() {
    for (const auto& s : m_overrides | std::views::reverse) {
        if (s.empty())
            continue;

        if (s == m_overrideShape)
            return;

        m_overrideShape = s;
        m_events.overrideChanged.emit(s);
        return;
    }

    if (m_overrideShape.empty())
        return;

    m_overrideShape = "";
    m_events.overrideChanged.emit("");
}
