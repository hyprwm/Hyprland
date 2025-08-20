#include "DispatcherGesture.hpp"

#include "../../../../managers/KeybindManager.hpp"

CDispatcherTrackpadGesture::CDispatcherTrackpadGesture(const std::string& dispatcher, const std::string& data) : m_dispatcher(dispatcher), m_data(data) {
    ;
}

void CDispatcherTrackpadGesture::begin(const IPointer::SSwipeUpdateEvent& e) {
    ; // intentionally blank
}

void CDispatcherTrackpadGesture::update(const IPointer::SSwipeUpdateEvent& e) {
    ; // intentionally blank
}

void CDispatcherTrackpadGesture::end(const IPointer::SSwipeEndEvent& e) {
    if (!g_pKeybindManager->m_dispatchers.contains(m_dispatcher))
        return;

    g_pKeybindManager->m_dispatchers.at(m_dispatcher)(m_data);
}
