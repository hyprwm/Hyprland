#pragma once

struct ca_context;
struct ca_proplist;

class CBellSound {
  public:
    static void play();

  private:
    CBellSound();
    ~CBellSound();

    void         onNewConfig();

    int          validateCustomSound();
    void         initializeSoundContext();

    bool         m_muted   = false;
    ca_context*  m_context = nullptr;
    ca_proplist* m_sound   = nullptr;
};
