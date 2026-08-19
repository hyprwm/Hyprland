#pragma once

#include "../../helpers/memory/Memory.hpp"

namespace Hyprtoolkit {
    class IEmbeddedBackend;
    class IEmbeddedSurface;
    class IEventLoop;
}

namespace EmbeddedToolkit {
    class CManager {
      public:
        CManager();
        ~CManager();

        SP<Hyprtoolkit::IEmbeddedSurface> createSurface() const;
        bool                              available() const;

      private:
        SP<Hyprtoolkit::IEventLoop>       m_eventLoop;
        SP<Hyprtoolkit::IEmbeddedBackend> m_backend;
    };

    UP<CManager>& manager();
}
