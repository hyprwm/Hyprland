#pragma once

#include "ConfigFixRunner.hpp"

#include <functional>
#include <vector>
#include <string_view>
#include <optional>

namespace Config::Supplementary {
    class IConfigFixSimpleRewriter : public IConfigFixRunner {
      protected:
        IConfigFixSimpleRewriter() = default;

      public:
        virtual ~IConfigFixSimpleRewriter() = default;

      protected:
        void                            runForFile(const std::string& content, const std::function<bool(const std::vector<std::string_view>&, std::string_view)>& fn);
        bool                            isLineAssigningVar(const std::string& var, const std::vector<std::string_view>& cats, std::string_view line);
        std::optional<std::string_view> getValueOf(const std::string& content, const std::string& var);
        std::string                     removeAssignmentOfVar(const std::string& content, const std::string& var);
    };
};