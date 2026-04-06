#pragma once

#include <type_traits>
#include <typeinfo>

namespace Config::Values {
    class IValue {
      protected:
        IValue() = default;

        const char *m_name = nullptr, *m_description = nullptr;

      public:
        virtual ~IValue() = default;

        virtual const std::type_info* underlying() const = 0;
        virtual const char*           name() const;
        virtual const char*           description() const;
        virtual void                  commence() = 0;
    };
};