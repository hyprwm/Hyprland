#include "../../../keybinds/Submap.hpp"

namespace Config::Actions {

    class CSubmapContext {
      public:
        CSubmapContext()                                                         = default;
        CSubmapContext(CSubmapContext&)                                          = delete;
        CSubmapContext(CSubmapContext&&)                                         = default;
        CSubmapContext&                              operator=(CSubmapContext&)  = delete;
        CSubmapContext&                              operator=(CSubmapContext&&) = default;

        SP<CSubmapContext>                           snapshot();

        bool                                         contains(const std::string& submap) const;
        bool                                         contains(const Keybinds::PSubmap& submap) const;
        std::optional<Keybinds::PSubmap>             find(const std::string& submap) const;
        std::optional<Keybinds::PSubmap>             find(const Keybinds::PSubmap& submap) const;
        bool                                         empty() const;
        void                                         add(Keybinds::PSubmap&& submap);
        void                                         remove(const std::string& submap);
        void                                         remove(const Keybinds::PSubmap& submap);
        void                                         toggle(Keybinds::PSubmap&& submap);
        void                                         reset();

        const std::unordered_set<Keybinds::PSubmap>& submaps() const;

      private:
        std::unordered_set<Keybinds::PSubmap> m_submaps;
    };
}
