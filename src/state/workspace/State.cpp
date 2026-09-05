#include "State.hpp"

#include "LifecyclePolicyAdapter.hpp"
#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"
#include "../../debug/log/Logger.hpp"
#include "../../workspace/RegularWorkspace.hpp"
#include "../../workspace/SpecialWorkspace.hpp"
#include "../../workspace/filter/WorkspaceFilter.hpp"

#include <algorithm>
#include <limits>

using namespace State::Workspace;

UP<CState>& State::Workspace::state() {
    static UP<CState> instance = makeUnique<CState>();
    return instance;
}

const std::vector<PHLWORKSPACEREF>& CState::workspaceRefs() const {
    return m_workspaces;
}

std::vector<PHLWORKSPACE> CState::workspacesCopy() const {
    std::vector<PHLWORKSPACE> result;
    auto                      RANGE = workspaces();
    result.reserve(m_workspaces.size());
    for (const auto& workspace : RANGE)
        result.emplace_back(workspace.lock());
    return result;
}

std::vector<PHLWORKSPACE> CState::filter(const std::string& selector, std::string* error) const {
    auto                                        result = workspacesCopy();
    const ::Workspace::Filter::CWorkspaceFilter filter{selector, &::Workspace::Filter::hlDataSource()};

    if (error)
        *error = filter.error();

    filter.transform(result);
    return result;
}

bool CState::add(PHLWORKSPACE workspace) {
    bool duplicate = false;
    if (workspace) {
        const auto ID = workspace->id();
        if (const auto NUMBERED = std::get_if<::Workspace::SWorkspaceNumberedID>(&ID); NUMBERED)
            duplicate = !!query().numbered(*NUMBERED).run();
        else
            duplicate = !!query().identity(ID, workspace->addressableName(), workspace->type()).run();
    }

    if (!workspace || duplicate) {
        LOG(Log::ERR, "Refusing duplicate workspace identity {}", workspace ? workspace->addressableName() : "<null>");
        return false;
    }

    m_workspaces.emplace_back(workspace);
    workspace->m_events.destroy.listenStatic([this, weak = PHLWORKSPACEREF{workspace}] { std::erase(m_workspaces, weak); });
    return true;
}

void CState::clear() {
    const auto WORKSPACES = workspacesCopy();
    for (const auto& workspace : WORKSPACES) {
        const auto REGULAR = dynamicPointerCast<::Workspace::CRegularWorkspace>(workspace);
        if (REGULAR)
            REGULAR->setPersistent(false);
    }

    m_workspaces.clear();
    clearLifecyclePolicy();
}

PHLWORKSPACE CState::find(const STarget& target) const {
    if (!target.valid())
        return nullptr;
    if (const auto NUMBERED = std::get_if<::Workspace::SWorkspaceNumberedID>(&*target.id); NUMBERED)
        return query().numbered(*NUMBERED).run();
    return query().identity(*target.id, target.address, target.type).run();
}

PHLWORKSPACE CState::create(const STarget& target, PHLMONITOR monitor, bool isEmpty) {
    if (!target.valid() || !monitor)
        return nullptr;

    if (const auto NUMBERED = std::get_if<::Workspace::SWorkspaceNumberedID>(&*target.id); NUMBERED)
        return createNumbered(*NUMBERED, std::move(monitor), target.displayName, isEmpty);
    if (target.type == ::Workspace::eWorkspaceType::SPECIAL)
        return createSpecial(target.address, std::move(monitor), isEmpty);
    return createNamed(target.address, std::move(monitor), target.displayName, isEmpty);
}

PHLWORKSPACE CState::createNumbered(::Workspace::SWorkspaceNumberedID id, PHLMONITOR monitor, std::string displayName, bool isEmpty) {
    const auto ADDRESS = std::to_string(id.value);
    if (query().numbered(id).run()) {
        LOG(Log::ERR, "Refusing duplicate numbered workspace {}", id.value);
        return nullptr;
    }

    monitor = Config::workspaceRuleMgr()->getBoundMonitorForWS(id, ::Workspace::eWorkspaceType::NORMAL, ADDRESS).value_or(std::move(monitor));
    if (!monitor) {
        LOG(Log::ERR, "No monitor for new numbered workspace {}", id.value);
        return nullptr;
    }

    auto workspace = ::Workspace::CRegularWorkspace::create(id, std::move(monitor), std::move(displayName), isEmpty);
    if (!workspace)
        return nullptr;

    if (!add(workspace))
        return nullptr;
    workspace->m_alpha->setValueAndWarp(0);
    return workspace;
}

PHLWORKSPACE CState::createNamed(std::string address, PHLMONITOR monitor, std::string displayName, bool isEmpty) {
    if (query().identity(::Workspace::SWorkspaceSpecialID{}, address, ::Workspace::eWorkspaceType::NORMAL).run()) {
        LOG(Log::ERR, "Refusing duplicate named workspace {}", address);
        return nullptr;
    }

    monitor = Config::workspaceRuleMgr()->getBoundMonitorForWS(::Workspace::SWorkspaceSpecialID{}, ::Workspace::eWorkspaceType::NORMAL, address).value_or(std::move(monitor));
    if (!monitor || address.empty()) {
        LOG(Log::ERR, "No monitor or address for new named workspace {}", address);
        return nullptr;
    }

    auto workspace = ::Workspace::CRegularWorkspace::createNamed(std::move(monitor), std::move(address), std::move(displayName), isEmpty);
    if (!workspace)
        return nullptr;

    if (!add(workspace))
        return nullptr;
    workspace->m_alpha->setValueAndWarp(0);
    return workspace;
}

PHLWORKSPACE CState::createSpecial(std::string address, PHLMONITOR monitor, bool isEmpty) {
    if (address == "special")
        address = "special:special";
    else if (!address.starts_with("special:"))
        address = "special:" + address;

    if (address.size() == 8 || query().identity(::Workspace::SWorkspaceSpecialID{}, address, ::Workspace::eWorkspaceType::SPECIAL).run())
        return nullptr;

    monitor = Config::workspaceRuleMgr()->getBoundMonitorForWS(::Workspace::SWorkspaceSpecialID{}, ::Workspace::eWorkspaceType::SPECIAL, address).value_or(std::move(monitor));
    if (!monitor)
        return nullptr;

    auto workspace = ::Workspace::CSpecialWorkspace::create(std::move(monitor), std::move(address), isEmpty);
    if (!workspace)
        return nullptr;

    if (!add(workspace))
        return nullptr;
    workspace->m_alpha->setValueAndWarp(0);
    return workspace;
}
