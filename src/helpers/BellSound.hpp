#pragma once

#include <hyprutils/signal/Listener.hpp>

struct ca_context;
struct ca_proplist;

class CBellSound {
  public:
    static void play();

  private:
    CBellSound();
    ~CBellSound();

    void                                   onNewConfig();
    void                                   initializeSoundContext();

    bool                                   m_muted   = false;
    ca_proplist*                           m_sound   = nullptr;
    ca_context*                            m_context = nullptr;

    Hyprutils::Signal::CHyprSignalListener m_configListener;
};
