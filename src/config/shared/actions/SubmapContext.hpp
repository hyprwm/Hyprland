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

        bool                                         empty() const;
        bool                                         contains(const std::string& submap) const;
        std::optional<Keybinds::PSubmap>             find(const std::string& name, const SP<IHID>& device) const;
        void                                         reset();
        void                                         add(Keybinds::PSubmap&& submap);
        void                                         remove(const Keybinds::PSubmap& submap);
        void                                         remove(const std::string& submapName);
        void                                         toggle(Keybinds::PSubmap&& submap);
        void                                         uniset(Keybinds::PSubmap&& submap);

        const std::vector<Keybinds::PSubmap>& submaps() const;

      private:
        std::vector<Keybinds::PSubmap> m_submaps;
    };
}
