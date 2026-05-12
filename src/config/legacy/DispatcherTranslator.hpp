#pragma once

#include "../../SharedDefs.hpp"
#include "../../helpers/memory/Memory.hpp"

#include <functional>
#include <unordered_map>
#include <string>

namespace Config::Legacy {
    class CDispatcherTranslator {
      public:
        CDispatcherTranslator();
        ~CDispatcherTranslator() = default;

        SDispatchResult run(const std::string& dispatcher, const std::string& data);

      private:
        std::unordered_map<std::string, std::function<SDispatchResult(const std::string&)>> m_dispMap;
    };

    UP<CDispatcherTranslator>& translator();
}