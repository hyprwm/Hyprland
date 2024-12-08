#pragma once

#include "../../defines.hpp"
#include "PassElement.hpp"

class CRenderPass {
  public:
    bool empty() const;
    bool single() const;
    bool needsIntrospection() const;

    void add(SP<IPassElement> elem);
    void simplify();
    void clear();

    void render();

  private:
    std::vector<SP<IPassElement>> m_vPassElements;

    SP<IPassElement>             currentPassInfo = nullptr;

    friend class CHyprOpenGLImpl;
};