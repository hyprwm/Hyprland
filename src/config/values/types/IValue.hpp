#pragma once

#include <type_traits>
#include <typeinfo>

#include "../../supplementary/propRefresher/PropRefresher.hpp"

namespace Config::Values {
    class IValue {
      protected:
        IValue(Supplementary::PropRefreshBits refreshProps);

        const char *                         m_name = nullptr, *m_description = nullptr;

        const Supplementary::PropRefreshBits m_refreshProps = 0;

      public:
        virtual ~IValue() = default;

        virtual const std::type_info*          underlying() const = 0;
        virtual const char*                    name() const;
        virtual const char*                    description() const;
        virtual Supplementary::PropRefreshBits refreshBits() const;
        virtual void                           commence() = 0;
    };
};
