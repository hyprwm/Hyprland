#include "WorkspaceRule.hpp"

using namespace Config;

void CWorkspaceRule::mergeLeft(const CWorkspaceRule& other) {
    if (!other.m_monitor.empty())
        m_monitor = other.m_monitor;
    if (m_workspaceString.empty())
        m_workspaceString = other.m_workspaceString;
    if (m_workspaceName.empty())
        m_workspaceName = other.m_workspaceName;
    if (m_workspaceId == WORKSPACE_INVALID)
        m_workspaceId = other.m_workspaceId;

    if (other.m_isDefault.has_value())
        m_isDefault = other.m_isDefault;
    if (other.m_isPersistent.has_value())
        m_isPersistent = other.m_isPersistent;
    if (other.m_gapsIn.has_value())
        m_gapsIn = other.m_gapsIn;
    if (other.m_gapsOut.has_value())
        m_gapsOut = other.m_gapsOut;
    if (other.m_floatGaps)
        m_floatGaps = other.m_floatGaps;
    if (other.m_borderSize.has_value())
        m_borderSize = other.m_borderSize;
    if (other.m_noBorder.has_value())
        m_noBorder = other.m_noBorder;
    if (other.m_noRounding.has_value())
        m_noRounding = other.m_noRounding;
    if (other.m_decorate.has_value())
        m_decorate = other.m_decorate;
    if (other.m_noShadow.has_value())
        m_noShadow = other.m_noShadow;
    if (other.m_onCreatedEmptyRunCmd.has_value())
        m_onCreatedEmptyRunCmd = other.m_onCreatedEmptyRunCmd;
    if (other.m_defaultName.has_value())
        m_defaultName = other.m_defaultName;
    if (other.m_layout.has_value())
        m_layout = other.m_layout;
    if (!other.m_layoutopts.empty()) {
        for (const auto& layoutopt : other.m_layoutopts) {
            m_layoutopts[layoutopt.first] = layoutopt.second;
        }
    }
    if (other.m_animationStyle.has_value())
        m_animationStyle = other.m_animationStyle;
}
