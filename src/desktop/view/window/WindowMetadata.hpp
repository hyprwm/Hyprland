#pragma once

#include <cstdint>
#include <string>

namespace Desktop::View {

    class CWindowMetadata {
      public:
        CWindowMetadata();
        CWindowMetadata(const CWindowMetadata&)              = delete;
        CWindowMetadata(CWindowMetadata&&)                   = delete;
        CWindowMetadata&   operator=(const CWindowMetadata&) = delete;
        CWindowMetadata&   operator=(CWindowMetadata&&)      = delete;

        const std::string& title() const;
        const std::string& appID() const;
        const std::string& initialTitle() const;
        const std::string& initialAppID() const;
        uint64_t           stableID() const;

        void               initializeOnFirstMap(const std::string& title, const std::string& appID);
        bool               updateTitle(const std::string& title);
        bool               updateAppID(const std::string& appID);

      private:
        std::string    m_title;
        std::string    m_appID;
        std::string    m_initialTitle;
        std::string    m_initialAppID;
        const uint64_t m_stableID;
    };
}
