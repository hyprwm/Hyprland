#include "WorkspaceFilter.hpp"

#include "../AbstractWorkspace.hpp"
#include "helpers/string/StringUtils.hpp"
#include "statement/DisplayNamePrefixStatement.hpp"
#include "statement/DisplayNameSuffixStatement.hpp"
#include "statement/FullscreenStatement.hpp"
#include "statement/IDStatement.hpp"
#include "statement/MonitorStatement.hpp"
#include "statement/NamedAddressableNameStatement.hpp"
#include "statement/NamedStatement.hpp"
#include "statement/RangeStatement.hpp"
#include "statement/Statement.hpp"
#include "statement/SpecialAddressableNameStatement.hpp"
#include "statement/SpecialStatement.hpp"
#include "statement/WindowCountStatement.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <expected>
#include <format>
#include <string_view>

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/Numeric.hpp>

using namespace Workspace;
using namespace Workspace::Filter;
using namespace Hyprutils::String;

static std::expected<std::pair<uint32_t, uint32_t>, std::string> parsePositiveRange(std::string_view value, char statement) {
    const auto DASH = value.find('-');
    if (DASH == std::string_view::npos || DASH == 0 || DASH == value.size() - 1 || value.find('-', DASH + 1) != std::string_view::npos)
        return std::unexpected(std::format("{} statement expects a range such as {}[1-5]", statement, statement));

    const auto FROM = strToNumber<uint32_t>(value.substr(0, DASH));
    const auto TO   = strToNumber<uint32_t>(value.substr(DASH + 1));
    if (!FROM)
        return std::unexpected(StringUtils::huParseErrorToString(FROM.error()));
    if (!TO)
        return std::unexpected(StringUtils::huParseErrorToString(TO.error()));
    if (*FROM < 1 || *TO < *FROM)
        return std::unexpected(std::format("{} statement range must be positive and ordered", statement));

    return std::pair{*FROM, *TO};
}

static std::expected<bool, std::string> parseBoolean(std::string_view value, char statement) {
    if (value.starts_with("true") || value.starts_with("yes") || value.starts_with("on"))
        return true;
    if (value.starts_with("false") || value.starts_with("no") || value.starts_with("off"))
        return false;

    const auto NUMBER = strToNumber<int64_t>(value);
    if (NUMBER)
        return *NUMBER != 0;

    return std::unexpected(std::format("{} statement expects a boolean", statement));
}

static std::expected<UP<IStatement>, std::string> parseStatement(char type, std::string_view value) {
    if (value.empty())
        return std::unexpected(std::format("{} statement cannot be empty", type));

    if (type == 'r') {
        const auto RANGE = parsePositiveRange(value, type);
        if (!RANGE)
            return std::unexpected(RANGE.error());

        return makeUnique<CRangeStatement>(RANGE->first, RANGE->second);
    }

    if (type == 's') {
        const auto SPECIAL = parseBoolean(value, type);
        if (!SPECIAL)
            return std::unexpected(SPECIAL.error());

        return makeUnique<CSpecialStatement>(*SPECIAL);
    }

    if (type == 'n') {
        if (value.starts_with("s:") && value.size() == 2)
            return std::unexpected("n prefix statement cannot be empty");
        if (value.starts_with("e:") && value.size() == 2)
            return std::unexpected("n suffix statement cannot be empty");
        if (value.starts_with("s:"))
            return makeUnique<CDisplayNamePrefixStatement>(std::string{value.substr(2)});
        if (value.starts_with("e:"))
            return makeUnique<CDisplayNameSuffixStatement>(std::string{value.substr(2)});

        const auto NAMED = parseBoolean(value, type);
        if (!NAMED)
            return std::unexpected(NAMED.error());

        return makeUnique<CNamedStatement>(*NAMED);
    }

    if (type == 'm')
        return makeUnique<CMonitorStatement>(std::string{value});

    if (type == 'w') {
        bool TILED = false, FLOATING = false, PINNED = false, GROUPS = false, VISIBLE = false;
        while (!value.empty()) {
            switch (value.front()) {
                case 't':
                    if (TILED || FLOATING)
                        return std::unexpected("w statement contains duplicate or conflicting flags");
                    TILED = true;
                    break;
                case 'f':
                    if (FLOATING || TILED)
                        return std::unexpected("w statement contains duplicate or conflicting flags");
                    FLOATING = true;
                    break;
                case 'p':
                    if (PINNED)
                        return std::unexpected("w statement contains duplicate flags");
                    PINNED = true;
                    break;
                case 'g':
                    if (GROUPS)
                        return std::unexpected("w statement contains duplicate flags");
                    GROUPS = true;
                    break;
                case 'v':
                    if (VISIBLE)
                        return std::unexpected("w statement contains duplicate flags");
                    VISIBLE = true;
                    break;
                default: break;
            }

            if (value.front() != 't' && value.front() != 'f' && value.front() != 'p' && value.front() != 'g' && value.front() != 'v')
                break;

            value.remove_prefix(1);
        }

        if (value.empty())
            return std::unexpected("w statement is missing a count");

        uint32_t FROM = 0, TO = 0;
        if (!value.contains('-')) {
            const auto COUNT = strToNumber<uint32_t>(value);
            if (!COUNT)
                return std::unexpected(std::format("invalid w statement count '{}'", value));
            FROM = *COUNT;
            TO   = *COUNT;
        } else {
            const auto RANGE = parsePositiveRange(value, type);
            if (!RANGE)
                return std::unexpected(RANGE.error());
            FROM = RANGE->first;
            TO   = RANGE->second;
        }

        return makeUnique<CWindowCountStatement>(SWindowCountOptions{.tiled   = TILED ? std::optional{true} :
                                                                         FLOATING     ? std::optional{false} :
                                                                                        std::nullopt,
                                                                     .pinned  = PINNED,
                                                                     .groups  = GROUPS,
                                                                     .visible = VISIBLE},
                                                 FROM, TO);
    }

    if (type == 'f') {
        const auto STATE = strToNumber<int>(value);
        if (!STATE || *STATE < -1 || *STATE > 2)
            return std::unexpected(std::format("f statement expects a fullscreen state from -1 to 2, got '{}'", value));

        return makeUnique<CFullscreenStatement>(*STATE);
    }

    return std::unexpected(std::format("unknown workspace filter statement '{}'", type));
}

CWorkspaceFilter::CWorkspaceFilter(const std::string& filter, const IDataSource* dataSource) : m_dataSource(dataSource) {
    const auto FILTER = trim(filter);
    if (FILTER.empty())
        return;

    if (FILTER.starts_with("name:")) {
        if (FILTER.size() == 5) {
            m_error = "named workspace address cannot be empty";
            return;
        }

        m_statements.emplace_back(makeUnique<CNamedAddressableNameStatement>(std::string{FILTER.substr(5)}));
        return;
    }

    if (FILTER == "special" || FILTER.starts_with("special:")) {
        if (FILTER == "special:") {
            m_error = "special workspace address cannot be empty";
            return;
        }

        m_statements.emplace_back(makeUnique<CSpecialAddressableNameStatement>(FILTER == "special" ? "special:special" : std::string{FILTER}));
        return;
    }

    const auto NUMERIC_ID = strToNumber<uint32_t>(FILTER);
    if (NUMERIC_ID) {
        if (*NUMERIC_ID == 0) {
            m_error = "workspace number must be positive";
            return;
        }

        m_statements.emplace_back(makeUnique<CIDStatement>(*NUMERIC_ID));
        return;
    }

    if (std::ranges::all_of(FILTER, [](const char c) { return c >= '0' && c <= '9'; })) {
        m_error = StringUtils::huParseErrorToString(NUMERIC_ID.error());
        return;
    }

    if (!FILTER.contains('[')) {
        m_statements.emplace_back(makeUnique<CNamedAddressableNameStatement>(std::string{FILTER}));
        return;
    }

    for (size_t i = 0; i < FILTER.size();) {
        if (std::isspace(sc<unsigned char>(FILTER[i]))) {
            ++i;
            continue;
        }

        const auto TYPE = FILTER[i];
        if (i + 1 >= FILTER.size() || FILTER[i + 1] != '[') {
            m_error = std::format("expected '[' after '{}' at offset {}", TYPE, i);
            m_statements.clear();
            return;
        }

        const auto CLOSING = FILTER.find(']', i + 2);
        if (CLOSING == std::string_view::npos) {
            m_error = std::format("unterminated '{}' statement at offset {}", TYPE, i);
            m_statements.clear();
            return;
        }

        auto statement = parseStatement(TYPE, FILTER.substr(i + 2, CLOSING - i - 2));
        if (!statement) {
            m_error = std::format("{} at offset {}", statement.error(), i);
            m_statements.clear();
            return;
        }

        m_statements.emplace_back(std::move(*statement));
        i = CLOSING + 1;
    }

    if (m_statements.empty())
        m_error = "non-empty workspace filter contains no statements";
}

CWorkspaceFilter::~CWorkspaceFilter() = default;

bool CWorkspaceFilter::matches(const IAbstractWorkspace& workspace) const {
    if (!m_error.empty())
        return false;

    return std::ranges::all_of(m_statements, [this, &workspace](const auto& statement) { return statement->matches(workspace, m_dataSource); });
}

template <typename T>
static void transformWorkspaces(const CWorkspaceFilter& filter, std::vector<SP<T>>& workspaces) {
    std::erase_if(workspaces, [&filter](const auto& workspace) { return !workspace || !filter.matches(*workspace); });
}

void CWorkspaceFilter::transform(std::vector<SP<IAbstractWorkspace>>& workspaces) const {
    transformWorkspaces(*this, workspaces);
}

const std::string& CWorkspaceFilter::error() const {
    return m_error;
}
