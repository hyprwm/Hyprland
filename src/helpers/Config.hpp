#pragma once

#include "../SharedDefs.hpp"
#include "../desktop/DesktopTypes.hpp"
#include "../macros.hpp"

struct SWorkspaceIDName {
    WORKSPACEID id = WORKSPACE_INVALID;
    std::string name;
    bool        isAutoIDd = false;
};

bool                       isDirection(const std::string&);
bool                       isDirection(const char&);
std::optional<float>       getPlusMinusKeywordResult(std::string in, float relative);
SWorkspaceIDName           getWorkspaceIDNameFromString(const std::string&);
PHLWORKSPACE               resolveWorkspace(const std::string&);
PHLWORKSPACE               resolveWorkspaceForChange(const std::string&);
std::optional<std::string> cleanCmdForWorkspace(const std::string&, std::string);
bool                       truthy(const std::string& str);
