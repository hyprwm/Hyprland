#pragma once

#include <libeis.h>
#include <string>
#include <wayland-server-core.h>

struct SKeymap {
    int32_t  fd   = 0;
    uint32_t size = 0;
};

/*
 * Responsible to creating a socket for input communication
 */
class CEis {
  public:
    CEis(std::string socketPath);
    ~CEis();

    void startEmulating(int activationId);
    void stopEmulating();

    void resetKeyboard();
    void resetPointer();

    void sendMotion(double x, double y);
    void sendKey(uint32_t key, bool pressed);
    void sendModifiers(uint32_t modsDepressed, uint32_t modsLatched, uint32_t modsLocked, uint32_t group);
    void sendButton(uint32_t button, bool pressed);
    void sendScrollDelta(double x, double y);
    void sendScrollDiscrete(int32_t x, int32_t y);
    void sendScrollStop(bool stopX, bool stopY);
    void sendPointerFrame();

    int  getFileDescriptor();

    //

    std::string socketPath;

  private:
    int     onEvent(eis_event* e);
    int     pollEvents();
    void    ensurePointer();
    void    ensureKeyboard();
    SKeymap getKeymap();
    void    clearPointer();
    void    clearKeyboard();

    bool    m_stop   = false;
    eis*    m_eisCtx = nullptr;

    struct SClient {
        eis_client* handle = nullptr;
        eis_seat*   seat   = nullptr;

        eis_device* pointer  = nullptr;
        eis_device* keyboard = nullptr;
    } m_client;

    wl_event_source* m_eventSource;
};
