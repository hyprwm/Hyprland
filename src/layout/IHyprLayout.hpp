#pragma once

#include "../defines.hpp"
#include "../Window.hpp"

interface IHyprLayout {
public:

    virtual void        onWindowCreated(CWindow*)   = 0;
    virtual void        onWindowRemoved(CWindow*)   = 0;

};