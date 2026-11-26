#pragma once

#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"

namespace Hyprtoolkit {
    class IEmbeddedBackend;
}

namespace Toolkit {
    class CEventLoop;

    // Owns the embedded hyprtoolkit backend, which renders on the compositor's GL context.
    class CHost {
      public:
        CHost();
        ~CHost();

        CHost(const CHost&)            = delete;
        CHost& operator=(const CHost&) = delete;

        // Requires the GL renderer to be initialized.
        void init();

        // nullptr until init() succeeds
        SP<Hyprtoolkit::IEmbeddedBackend> backend() const;

        struct {
            CSignalT<> ready;
            // emitted before the backend is destroyed, users have to let go of it
            CSignalT<> destroy;
        } m_events;

      private:
        void                              destroy();

        SP<CEventLoop>                    m_eventLoop;
        SP<Hyprtoolkit::IEmbeddedBackend> m_backend;
    };

    UP<CHost>& host();
}
