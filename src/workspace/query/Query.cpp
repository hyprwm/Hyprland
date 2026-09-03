#include "Query.hpp"

#include <hyprutils/string/Numeric.hpp>
#include <hyprutils/string/String.hpp>
#include <format>
#include <utility>

using namespace Hyprutils::String;
using namespace Workspace;

std::string Workspace::selector(const IAbstractWorkspace& workspace) {
    if (std::holds_alternative<SWorkspaceSpecialID>(workspace.id()) && workspace.type() == eWorkspaceType::NORMAL)
        return std::format("name:{}", workspace.addressableName());

    return workspace.addressableName();
}

CQuery&& CQuery::numbered(SWorkspaceNumberedID id) && {
    m_identity = id;
    return std::move(*this);
}

CQuery&& CQuery::identity(WorkspaceID id, std::string_view address, eWorkspaceType type) && {
    m_identity = std::move(id);
    m_address  = address;
    m_type     = type;
    return std::move(*this);
}

CQuery&& CQuery::address(std::string_view address) && {
    m_address = address;
    return std::move(*this);
}

CQuery&& CQuery::input(std::string_view input) && {
    m_input = input;
    return std::move(*this);
}

bool CQuery::matches(const IAbstractWorkspace& workspace) const {
    auto                            identity = m_identity;
    std::optional<std::string_view> address  = m_address;
    std::optional<std::string_view> input    = m_input;
    auto                            type     = m_type;

    if (!input && address && !identity) {
        input = address;
        address.reset();
    }

    if (input) {
        if (input->starts_with("name:")) {
            if (input->size() == 5)
                return false;
            identity = SWorkspaceSpecialID{};
            address  = input->substr(5);
            type     = eWorkspaceType::NORMAL;
        } else if (*input == "special") {
            identity = SWorkspaceSpecialID{};
            address  = "special:special";
            type     = eWorkspaceType::SPECIAL;
        } else if (input->starts_with("special:")) {
            if (input->size() == 8)
                return false;
            identity = SWorkspaceSpecialID{};
            address  = *input;
            type     = eWorkspaceType::SPECIAL;
        } else if (isNumber(std::string{*input})) {
            const auto ID = strToNumber<WorkspaceIDContainer>(*input);
            if (!ID || *ID == 0)
                return false;

            identity = SWorkspaceNumberedID{*ID};
        } else {
            identity = SWorkspaceSpecialID{};
            address  = *input;
            type     = eWorkspaceType::NORMAL;
        }
    }

    if (identity && workspace.id() != *identity)
        return false;
    if (address && workspace.addressableName() != *address)
        return false;
    if (type && workspace.type() != *type)
        return false;

    return true;
}
