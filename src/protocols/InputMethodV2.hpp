#pragma once

#include <vector>
#include <cstdint>
#include "WaylandProtocol.hpp"
#include "input-method-unstable-v2.hpp"
#include "text-input-unstable-v3.hpp"
#include "../helpers/signal/Signal.hpp"
#include "../desktop/WLSurface.hpp"

class CInputMethodKeyboardGrabV2;
class CInputMethodPopupV2;
class IKeyboard;

class CInputMethodV2 {
  public:
    CInputMethodV2(SP<CZwpInputMethodV2> resource_);
    ~CInputMethodV2();

    struct {
        CSignal onCommit;
        CSignal destroy;
        CSignal newPopup;
    } events;

    struct SState {
        void reset();

        struct {
            std::string string;
            bool        committed = false;
        } committedString;

        struct {
            std::string string;
            int32_t     begin = 0, end = 0;
            bool        committed = false;
        } preeditString;

        struct {
            uint32_t before = 0, after = 0;
            bool     committed = false;
        } deleteSurrounding;
    };

    SState     pending, current;

    bool       good();
    void       activate();
    void       deactivate();
    void       surroundingText(const std::string& text, uint32_t cursor, uint32_t anchor);
    void       textChangeCause(zwpTextInputV3ChangeCause changeCause);
    void       textContentType(zwpTextInputV3ContentHint hint, zwpTextInputV3ContentPurpose purpose);
    void       done();
    void       unavailable();

    void       sendInputRectangle(const CBox& box);
    bool       hasGrab();
    void       sendKey(uint32_t time, uint32_t key, wl_keyboard_key_state state);
    void       sendMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);
    void       setKeyboard(SP<IKeyboard> keyboard);

    wl_client* client();
    wl_client* grabClient();

  private:
    SP<CZwpInputMethodV2>                       resource;
    std::vector<WP<CInputMethodKeyboardGrabV2>> grabs;
    std::vector<WP<CInputMethodPopupV2>>        popups;

    WP<CInputMethodV2>                          self;

    bool                                        active = false;

    CBox                                        inputRectangle;

    friend class CInputMethodPopupV2;
    friend class CInputMethodKeyboardGrabV2;
    friend class CInputMethodV2Protocol;
};

class CInputMethodKeyboardGrabV2 {
  public:
    CInputMethodKeyboardGrabV2(SP<CZwpInputMethodKeyboardGrabV2> resource_, SP<CInputMethodV2> owner_);
    ~CInputMethodKeyboardGrabV2();

    bool               good();
    SP<CInputMethodV2> getOwner();
    wl_client*         client();

    void               sendKey(uint32_t time, uint32_t key, wl_keyboard_key_state state);
    void               sendMods(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);
    void               sendKeyboardData(SP<IKeyboard> keyboard);

  private:
    SP<CZwpInputMethodKeyboardGrabV2> resource;
    WP<CInputMethodV2>                owner;

    WP<IKeyboard>                     pLastKeyboard;
};

class CInputMethodPopupV2 {
  public:
    CInputMethodPopupV2(SP<CZwpInputPopupSurfaceV2> resource_, SP<CInputMethodV2> owner_, SP<CWLSurfaceResource> surface);
    ~CInputMethodPopupV2();

    bool                   good();
    void                   sendInputRectangle(const CBox& box);
    SP<CWLSurfaceResource> surface();

    struct {
        CSignal map;
        CSignal unmap;
        CSignal commit;
        CSignal destroy;
    } events;

    bool mapped = false;

  private:
    SP<CZwpInputPopupSurfaceV2> resource;
    WP<CInputMethodV2>          owner;
    WP<CWLSurfaceResource>      pSurface;

    struct {
        CHyprSignalListener destroySurface;
        CHyprSignalListener commitSurface;
    } listeners;
};

class CInputMethodV2Protocol : public IWaylandProtocol {
  public:
    CInputMethodV2Protocol(const wl_interface* iface, const int& ver, const std::string& name);

    virtual void bindManager(wl_client* client, void* data, uint32_t ver, uint32_t id);

    struct {
        CSignal newIME; // SP<CInputMethodV2>
    } events;

  private:
    void onManagerResourceDestroy(wl_resource* res);
    void destroyResource(CInputMethodPopupV2* popup);
    void destroyResource(CInputMethodKeyboardGrabV2* grab);
    void destroyResource(CInputMethodV2* ime);

    void onGetIME(CZwpInputMethodManagerV2* mgr, wl_resource* seat, uint32_t id);

    //
    std::vector<UP<CZwpInputMethodManagerV2>>   m_vManagers;
    std::vector<SP<CInputMethodV2>>             m_vIMEs;
    std::vector<SP<CInputMethodKeyboardGrabV2>> m_vGrabs;
    std::vector<SP<CInputMethodPopupV2>>        m_vPopups;

    friend class CInputMethodPopupV2;
    friend class CInputMethodKeyboardGrabV2;
    friend class CInputMethodV2;
};

namespace PROTO {
    inline UP<CInputMethodV2Protocol> ime;
};