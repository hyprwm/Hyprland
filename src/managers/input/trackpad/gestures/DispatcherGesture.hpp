#pragma once

#include "ITrackpadGesture.hpp"

class CDispatcherTrackpadGesture : public ITrackpadGesture {
  public:
    CDispatcherTrackpadGesture(const std::string& dispatcher, const std::string& data);
    virtual ~CDispatcherTrackpadGesture() = default;

    virtual void begin(const ITrackpadGesture::STrackpadGestureBegin& e);
    virtual void update(const ITrackpadGesture::STrackpadGestureUpdate& e);
    virtual void end(const ITrackpadGesture::STrackpadGestureEnd& e);

  private:
    std::string m_dispatcher, m_data;
};
