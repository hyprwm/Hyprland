#pragma once

#include "PassElement.hpp"

struct SBackdropScope {
    bool    required = false;
    CRegion damage;
};

class CBackdropScopePlanner {
  public:
    void begin(SP<SBackdropScope> scope);
    void addLiveBlur(const CRegion& damage);
    void end(SP<SBackdropScope> scope, const CBox& bounds);
    bool empty() const;

  private:
    std::vector<SP<SBackdropScope>> m_scopes;
};

class CBackdropScopePassElement : public IPassElement {
  public:
    enum class eAction : uint8_t {
        BEGIN = 0,
        END,
    };

    CBackdropScopePassElement(eAction action, SP<SBackdropScope> scope);
    virtual ~CBackdropScopePassElement() = default;

    virtual std::vector<UP<IPassElement>> draw();
    virtual bool                          needsLiveBlur();
    virtual bool                          needsPrecomputeBlur();
    virtual bool                          undiscardable();

    virtual const char*                   passName();
    virtual ePassElementType              type();

    eAction                               action() const;
    SP<SBackdropScope>                    scope() const;

  private:
    eAction            m_action = eAction::BEGIN;
    SP<SBackdropScope> m_scope;
};
