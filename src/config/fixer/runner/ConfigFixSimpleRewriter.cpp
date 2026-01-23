#include "ConfigFixSimpleRewriter.hpp"

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

#include <format>

using namespace Config::Supplementary;
using namespace Config;
using namespace Hyprutils::String;
using namespace Hyprutils::Utils;

void IConfigFixSimpleRewriter::runForFile(const std::string& content, const std::function<bool(const std::vector<std::string_view>&, std::string_view)>& fn) {
    std::vector<std::string_view> currentCat;

    CVarList2                     lines(std::string{content}, 0, '\n', true, true);

    for (const auto& line : lines) {
        std::string_view l = trim(line);
        if (l.ends_with(" {")) {
            // cat
            currentCat.emplace_back(l.substr(0, l.size() - 2));
            continue;
        }

        if (l == "}") {
            if (!currentCat.empty())
                currentCat.pop_back();
            continue;
        }

        if (fn(currentCat, l))
            break;
    }
}

bool IConfigFixSimpleRewriter::isLineAssigningVar(const std::string& var, const std::vector<std::string_view>& cats, std::string_view line) {
    if (!line.contains('='))
        return false;

    const auto LHS = trim(line.substr(0, line.find('=')));

    if (cats.empty())
        return LHS == var;

    std::string build = "";
    for (const auto& c : cats) {
        build += std::format("{}:", c);
    }

    build += LHS;

    return build == var;
}

std::optional<std::string_view> IConfigFixSimpleRewriter::getValueOf(const std::string& content, const std::string& var) {
    std::optional<std::string_view> wanted;

    runForFile(content, [&wanted, this, &var](const std::vector<std::string_view>& cats, std::string_view line) -> bool {
        if (isLineAssigningVar(var, cats, line))
            wanted = trim(line.substr(line.find('=') + 1));

        return false;
    });

    return wanted;
}

std::string IConfigFixSimpleRewriter::removeAssignmentOfVar(const std::string& content, const std::string& var) {
    std::vector<std::string_view> currentCat;

    // we need to manually do this rn

    std::string builder;
    builder.reserve(content.size());
    size_t lastCopyHead = 0;

    size_t lastLineBreak = 0;
    bool commentUntilClear = false;

    while (lastLineBreak != std::string::npos) {
        std::string_view line = std::string_view{content}.substr(lastLineBreak, content.find('\n', lastLineBreak) - lastLineBreak);

        CScopeGuard      x([&] {
            lastLineBreak = content.find('\n', lastLineBreak);
            if (lastLineBreak != std::string::npos)
                lastLineBreak++;
        });

        if (commentUntilClear) {
            builder += "\n#";
            builder += line;
            lastCopyHead = lastLineBreak + line.size();

            if (!line.ends_with('\\'))
                commentUntilClear = false;

            continue;
        }

        std::string_view l = trim(line);
        if (l.ends_with(" {")) {
            // cat
            currentCat.emplace_back(l.substr(0, l.size() - 2));
            continue;
        }

        if (l == "}") {
            if (!currentCat.empty())
                currentCat.pop_back();
            continue;
        }

        if (isLineAssigningVar(var, currentCat, l)) {
            builder += std::string_view{content}.substr(lastCopyHead, lastLineBreak - lastCopyHead);
            builder += "#";
            builder += line;

            if (line.ends_with('\\'))
                commentUntilClear = true;

            lastCopyHead = lastLineBreak + line.length();
        }
    }

    builder += std::string_view{content}.substr(lastCopyHead, lastLineBreak - lastCopyHead);

    return builder;
}
