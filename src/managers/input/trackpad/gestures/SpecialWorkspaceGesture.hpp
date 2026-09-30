#pragma once

#include "ITrackpadGesture.hpp"

#include "../../../../desktop/DesktopTypes.hpp"
#include "../../../../helpers/signal/Signal.hpp"

class CSpecialWorkspaceGesture : public ITrackpadGesture {
  public:
    CSpecialWorkspaceGesture(const std::string& workspaceName);
    virtual ~CSpecialWorkspaceGesture();

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    bool          ownsActiveSpecial() const;
    void          cancel();
    void          finish(bool close);
    void          resetSession();

    std::string   m_specialWorkspaceName;
    PHLWORKSPACE  m_specialWorkspace;
    PHLMONITORREF m_monitor;
    bool          m_sessionActive          = false;
    bool          m_forceRenderingAcquired = false;
    bool          m_animatingOut           = false;
    float         m_lastDelta              = 0.F;

    bool          m_hasPreview        = false;
    float         m_lastPreviewAlpha  = 0.F;
    Vector2D      m_lastPreviewOffset = {};

    struct {
        CHyprSignalListener monitorDisconnect;
        CHyprSignalListener monitorDestroy;
        CHyprSignalListener workspaceMonitorChanged;
        CHyprSignalListener workspaceActiveChanged;
    } m_listeners;

    // animated properties, kinda sucks
    float    m_monitorFadeFrom = 0.F, m_monitorFadeTo = 0.F;
    float    m_monitorDimFrom = 0.F, m_monitorDimTo = 0.F;
    float    m_monitorBlurFrom = 0.F, m_monitorBlurTo = 0.F;
    float    m_workspaceAlphaFrom = 0.F, m_workspaceAlphaTo = 0.F;
    Vector2D m_workspaceOffsetFrom = {}, m_workspaceOffsetTo = {};
};
