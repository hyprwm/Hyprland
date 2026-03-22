#pragma once

#include <string>
#include <optional>
#include <map>

#include "../complex/ComplexDataTypes.hpp"
#include "../../../defines.hpp"

namespace Config {
    class CWorkspaceRule {
      public:
        CWorkspaceRule()  = default;
        ~CWorkspaceRule() = default;

        CWorkspaceRule(const CWorkspaceRule&) = default;
        CWorkspaceRule(CWorkspaceRule&)       = default;
        CWorkspaceRule(CWorkspaceRule&&)      = default;

        CWorkspaceRule& operator=(const CWorkspaceRule&) = default;

        // merge other into us
        void                               mergeLeft(const CWorkspaceRule& other);

        bool                               m_enabled         = true;
        std::string                        m_monitor         = "";
        std::string                        m_workspaceString = "";
        std::string                        m_workspaceName   = "";
        WORKSPACEID                        m_workspaceId     = -1;
        bool                               m_isDefault       = false;
        bool                               m_isPersistent    = false;
        std::optional<CCssGapData>         m_gapsIn;
        std::optional<CCssGapData>         m_gapsOut;
        std::optional<CCssGapData>         m_floatGaps = m_gapsOut;
        std::optional<int64_t>             m_borderSize;
        std::optional<bool>                m_decorate;
        std::optional<bool>                m_noRounding;
        std::optional<bool>                m_noBorder;
        std::optional<bool>                m_noShadow;
        std::optional<std::string>         m_onCreatedEmptyRunCmd;
        std::optional<std::string>         m_defaultName;
        std::optional<std::string>         m_layout;
        std::map<std::string, std::string> m_layoutopts;
        std::optional<std::string>         m_animationStyle;
    };
};