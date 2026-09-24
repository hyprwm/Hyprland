#include "../../../keybinds/Submap.hpp"

namespace Config::Actions {

    namespace {
        using Keybinds::PSubmap;
        using SubmapList = std::unordered_set<PSubmap>;
    }

    class CSubmapContext {
      public:
        CSubmapContext()                                   = default;
        CSubmapContext(CSubmapContext&)                    = delete;
        CSubmapContext(CSubmapContext&&)                   = default;
        CSubmapContext&        operator=(CSubmapContext&)  = delete;
        CSubmapContext&        operator=(CSubmapContext&&) = default;

        SP<CSubmapContext>     snapshot();

        bool                   contains(const std::string& submap) const;
        bool                   contains(const PSubmap& submap) const;
        std::optional<PSubmap> find(const std::string& submap) const;
        std::optional<PSubmap> find(const PSubmap& submap) const;
        bool                   empty() const;
        void                   add(PSubmap&& submap);
        void                   remove(const std::string& submap);
        void                   remove(const PSubmap& submap);
        void                   toggle(PSubmap&& submap);
        void                   reset();

        const SubmapList&      submaps() const;

      private:
        SubmapList m_submaps;
    };
}
