/* 
  THIS IS A TEST RUNNER!!

  This is a POC runner for a past change. This is here only for tests to be able to test various weird
  configs people might have.

  This runner is NOT added to CConfigManager.
*/

#include "Fixer12033.hpp"

#include <hyprutils/string/VarList2.hpp>
#include <hyprutils/string/String.hpp>

#include <format>

using namespace Config::Supplementary;
using namespace Config;
using namespace Hyprutils::String;

/*

This fixer fixes:
misc:new_window_takes_over_fullscreen -> on_focus_under_fullscreen
master:inherit_fullscreen -> removed
*/

bool CFixer12033::check(const std::string& fileContent) {
    bool fail = false;

    runForFile(fileContent, //
               [&fail, this](const std::vector<std::string_view>& cats, std::string_view line) -> bool {
                   if (isLineAssigningVar("master:inherit_fullscreen", cats, line) || isLineAssigningVar("misc:new_window_takes_over_fullscreen", cats, line)) {
                       fail = true;
                       return true;
                   }

                   return false;
               });

    return !fail;
}

std::string CFixer12033::run(const std::string& fileContent) {
    // first, get the preferred value
    const auto PREFERRED = std::string{getValueOf(fileContent, "misc:new_window_takes_over_fullscreen").value_or("0")};

    // now, remove all invalid operands
    std::string build = removeAssignmentOfVar(fileContent, "master:inherit_fullscreen");
    build             = removeAssignmentOfVar(build, "misc:new_window_takes_over_fullscreen");

    // add our own var at the end
    // TODO: this is le ugly

    build += std::format("\nmisc:new_window_takes_over_fullscreen = {}", PREFERRED);

    return build;
}