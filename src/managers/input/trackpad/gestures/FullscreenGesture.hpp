#pragma once

#include "ITrackpadGesture.hpp"

#include "../../../../desktop/DesktopTypes.hpp"
#include "../../../../desktop/Workspace.hpp"
#include "../../../../managers/fullscreen/FullscreenController.hpp"

class CFullscreenTrackpadGesture : public ITrackpadGesture {
  public:
    CFullscreenTrackpadGesture(const std::string_view& mode);
    virtual ~CFullscreenTrackpadGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    PHLWINDOWREF m_window;

    Vector2D     m_posFrom, m_posTo, m_sizeFrom, m_sizeTo;

    float        m_lastDelta = 0;

    enum eMode : uint8_t {
        MODE_FULLSCREEN = 0,
        MODE_MAXIMIZE,
    };

    eMode                       m_mode         = MODE_FULLSCREEN;
    Fullscreen::eFullscreenMode m_originalMode = Fullscreen::FSMODE_NONE;

    Fullscreen::eFullscreenMode fsModeForMode(eMode mode);
};
