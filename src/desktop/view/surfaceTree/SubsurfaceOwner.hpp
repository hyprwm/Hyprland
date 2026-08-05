#pragma once

#include "../../../helpers/memory/Memory.hpp"

#include <cstddef>

namespace Desktop::View {
    class CSubsurface;

    class CSubsurfaceOwner {
      public:
        virtual ~CSubsurfaceOwner();

        const SP<CSubsurface>& subsurfaceHead() const;
        size_t                 subsurfaceTreeSize() const;
        size_t                 subsurfaceMappedTreeSize() const;

      protected:
        CSubsurfaceOwner();

        void setSubsurfaceHead(SP<CSubsurface> head);
        void resetSubsurfaceHead();

      private:
        SP<CSubsurface> m_subsurfaceHead;
    };
}
