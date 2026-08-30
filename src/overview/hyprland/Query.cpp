#include "Query.hpp"

#include "mode/CombinedQueryMode.hpp"
#include "mode/WindowQueryMode.hpp"
#include "mode/WorkspaceQueryMode.hpp"

using namespace Overview::Hyprland;

CQuery::CQuery(std::string raw, const SQueryConfig& config) :
    m_raw(std::move(raw)), m_term(m_raw), m_config(config),
    m_modes{makeUnique<Mode::CCombinedQueryMode>(), makeUnique<Mode::CWindowQueryMode>(), makeUnique<Mode::CWorkspaceQueryMode>()} {
    if (!m_raw.empty() && m_raw.front() == m_config.windowPrefix)
        m_term = m_raw.substr(1);
    else if (!m_raw.empty() && m_raw.front() == m_config.workspacePrefix)
        m_term = m_raw.substr(1);
}

CQuery::~CQuery() = default;

Mode::eWorkspaceMatch CQuery::matchWorkspace(std::string_view name, const Mode::FWorkspaceSelector& selector) const {
    if (m_term.empty())
        return Mode::eWorkspaceMatch::MATCH;

    return m_modes[sc<size_t>(mode())]->matchWorkspace(name, m_term, selector);
}

bool CQuery::matchesWindow(std::string_view appID, std::string_view title) const {
    return !m_term.empty() && m_modes[sc<size_t>(mode())]->matchesWindow(appID, title, m_term);
}

bool CQuery::usesWindowMetadata() const {
    return !m_term.empty() && mode() != eQueryMode::WORKSPACE;
}

bool CQuery::empty() const {
    return m_term.empty();
}

eQueryMode CQuery::mode() const {
    return mode(m_raw);
}

eQueryMode CQuery::mode(std::string_view query) const {
    if (!query.empty() && query.front() == m_config.windowPrefix)
        return m_modes[sc<size_t>(eQueryMode::WINDOW)]->type();
    if (!query.empty() && query.front() == m_config.workspacePrefix)
        return m_modes[sc<size_t>(eQueryMode::WORKSPACE)]->type();

    const auto DEFAULT = sc<size_t>(m_config.defaultMode);
    return m_modes[DEFAULT < m_modes.size() ? DEFAULT : sc<size_t>(eQueryMode::ALL)]->type();
}

const std::string& CQuery::raw() const {
    return m_raw;
}

const std::string& CQuery::term() const {
    return m_term;
}
