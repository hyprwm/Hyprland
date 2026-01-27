#pragma once

#include "ITrackpadGesture.hpp"

#include "../../../../desktop/DesktopTypes.hpp"

class CSpecialWorkspaceGesture : public ITrackpadGesture {
  public:
    CSpecialWorkspaceGesture(const std::string& workspaceName);
    virtual ~CSpecialWorkspaceGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    std::string   m_specialWorkspaceName;
    PHLWORKSPACE  m_specialWorkspace;
    PHLMONITORREF m_monitor;
    bool          m_animatingOut = false;
    float         m_lastDelta    = 0.F;

    // animated properties, kinda sucks
    float    m_monitorDimFrom = 0.F, m_monitorDimTo = 0.F;
    float    m_workspaceAlphaFrom = 0.F, m_workspaceAlphaTo = 0.F;
    Vector2D m_workspaceOffsetFrom = {}, m_workspaceOffsetTo = {};
};
