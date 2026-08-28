#pragma once

#include "Impl.hpp"
#include "macros/Class.hpp"

#include "RotatingPlayerThread.hpp"

struct ca_context;
struct ca_proplist;

namespace Bell {
    class CCanberraImpl : public IBellImpl {
      public:
        CCanberraImpl();
        ~CCanberraImpl();

        NON_MOVABLE(CCanberraImpl);

        virtual void play() const override;

      private:
        void                          initializeSoundContext();
        void                          playThreaded(const std::string& x);

        mutable CRotatingPlayerThread m_thread;

        ca_proplist*                  m_sound   = nullptr;
        ca_context*                   m_context = nullptr;
    };
}
