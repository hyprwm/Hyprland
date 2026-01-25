#include "Fixer12890.hpp"

#include <hyprutils/string/VarList2.hpp>
#include <hyprutils/string/String.hpp>

using namespace Config::Supplementary;
using namespace Config;
using namespace Hyprutils::String;

/*

This fixer fixes:
binds:window_direction_monitor_fallback -> removed
general:float_gaps -> removed

*/

bool CFixer12890::check(const std::string& fileContent) {
    bool fail = false;

    runForFile(fileContent, //
               [&fail, this](const std::vector<std::string_view>& cats, std::string_view line) -> bool {
                   if (isLineAssigningVar("binds:window_direction_monitor_fallback", cats, line) || isLineAssigningVar("general:float_gaps", cats, line)) {
                       fail = true;
                       return true;
                   }

                   return false;
               });

    return !fail;
}

std::string CFixer12890::run(const std::string& fileContent) {
    std::string build = removeAssignmentOfVar(fileContent, "binds:window_direction_monitor_fallback");
    build             = removeAssignmentOfVar(build, "general:float_gaps");

    return build;
}

REGISTER(CFixer12890);
