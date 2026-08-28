#pragma once

#include "../macros/Class.hpp"
#include "../helpers/memory/Memory.hpp"

namespace Bell {

    class IBellImpl;

    class CBellPlayer {
      public:
        CBellPlayer();
        ~CBellPlayer() = default;

        NON_MOVABLE(CBellPlayer);

        void play() const;

      private:
        UP<IBellImpl> m_impl = nullptr;
    };

    UP<CBellPlayer>& player();
};
