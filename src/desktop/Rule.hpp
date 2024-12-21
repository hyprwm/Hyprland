#pragma once

#include <memory>

//NOLINTNEXTLINE
namespace re2 {
    class RE2;
};

class CRuleRegexContainer {
  public:
    CRuleRegexContainer() = default;

    CRuleRegexContainer(const std::string& regex);

    bool passes(const std::string& str) const;

  private:
    std::unique_ptr<re2::RE2> regex;
    bool                      negative = false;
};