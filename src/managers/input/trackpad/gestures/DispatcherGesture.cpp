#include "DispatcherGesture.hpp"

#include "../../../../managers/KeybindManager.hpp"

CDispatcherTrackpadGesture::CDispatcherTrackpadGesture(const std::string& dispatcher, const std::string& data) : m_dispatcher(dispatcher), m_data(data) {
    ;
}

void CDispatcherTrackpadGesture::begin(const ITrackpadGesture::STrackpadGestureBegin& e) {
    ; // intentionally blank
}

void CDispatcherTrackpadGesture::update(const ITrackpadGesture::STrackpadGestureUpdate& e) {
    ; // intentionally blank
}

void CDispatcherTrackpadGesture::end(const ITrackpadGesture::STrackpadGestureEnd& e) {
    if (!g_pKeybindManager->m_dispatchers.contains(m_dispatcher))
        return;

    g_pKeybindManager->m_dispatchers.at(m_dispatcher)(m_data);
}
