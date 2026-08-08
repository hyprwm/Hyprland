#pragma once

namespace Desktop::View {
    class IClientMappable {
      public:
        virtual ~IClientMappable() = default;

        virtual bool isMapped() const = 0;

      protected:
        IClientMappable() = default;
    };
}
