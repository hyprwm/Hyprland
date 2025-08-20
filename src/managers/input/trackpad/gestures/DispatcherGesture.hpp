#pragma once

#include "../ITrackpadGesture.hpp"

class CDispatcherTrackpadGesture : public ITrackpadGesture {
  public:
    CDispatcherTrackpadGesture(const std::string& dispatcher, const std::string& data);
    virtual ~CDispatcherTrackpadGesture() = default;

    virtual void begin(const IPointer::SSwipeUpdateEvent& e);
    virtual void update(const IPointer::SSwipeUpdateEvent& e);
    virtual void end(const IPointer::SSwipeEndEvent& e);

  private:
    std::string m_dispatcher, m_data;
};