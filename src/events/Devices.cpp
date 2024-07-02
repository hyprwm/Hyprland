#include "Events.hpp"

#include "../Compositor.hpp"
#include "../helpers/WLClasses.hpp"
#include "../managers/input/InputManager.hpp"
#include "../render/Renderer.hpp"

// ---------------------------------------------------- //
//   _____  ________      _______ _____ ______  _____   //
//  |  __ \|  ____\ \    / /_   _/ ____|  ____|/ ____|  //
//  | |  | | |__   \ \  / /  | || |    | |__  | (___    //
//  | |  | |  __|   \ \/ /   | || |    |  __|  \___ \   //
//  | |__| | |____   \  /   _| || |____| |____ ____) |  //
//  |_____/|______|   \/   |_____\_____|______|_____/   //
//                                                      //
// ---------------------------------------------------- //

void Events::listener_newInput(wl_listener* listener, void* data) {
    const auto DEVICE = (wlr_input_device*)data;

    switch (DEVICE->type) {
        case WLR_INPUT_DEVICE_KEYBOARD:
            Debug::log(LOG, "Attached a keyboard with name {}", DEVICE->name);
            //g_pInputManager->newKeyboard(DEVICE);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            Debug::log(LOG, "Attached a mouse with name {}", DEVICE->name);
            //g_pInputManager->newMouse(DEVICE);
            break;
        case WLR_INPUT_DEVICE_TOUCH:
            Debug::log(LOG, "Attached a touch device with name {}", DEVICE->name);
            // g_pInputManager->newTouchDevice(DEVICE);
            break;
        case WLR_INPUT_DEVICE_TABLET:
            Debug::log(LOG, "Attached a tablet with name {}", DEVICE->name);
            g_pInputManager->newTablet(DEVICE);
            break;
        case WLR_INPUT_DEVICE_TABLET_PAD:
            Debug::log(LOG, "Attached a tablet pad with name {}", DEVICE->name);
            g_pInputManager->newTabletPad(DEVICE);
            break;
        case WLR_INPUT_DEVICE_SWITCH:
            Debug::log(LOG, "Attached a switch device with name {}", DEVICE->name);
          //  g_pInputManager->newSwitch(DEVICE);
            break;
        default: Debug::log(WARN, "Unrecognized input device plugged in: {}", DEVICE->name); break;
    }

    g_pInputManager->updateCapabilities();
}
