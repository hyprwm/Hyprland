#include "SpecialWorkspace.hpp"

using namespace Workspace;

CSpecialWorkspace::CSpecialWorkspace(PHLMONITOR monitor, std::string address) : CHLWorkspace(SWorkspaceSpecialID{}, std::move(monitor), address, address, eWorkspaceType::SPECIAL) {
    ;
}

PHLWORKSPACE CSpecialWorkspace::create(PHLMONITOR monitor, std::string address) {
    if (address == "special")
        address = "special:special";
    else if (!address.starts_with("special:"))
        address = "special:" + address;

    if (address.size() == 8)
        return nullptr;

    auto workspace = makeShared<CSpecialWorkspace>(std::move(monitor), std::move(address));
    workspace->init(workspace);
    return workspace;
}
