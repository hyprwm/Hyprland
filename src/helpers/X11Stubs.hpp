#pragma once

inline bool wlr_backend_is_x11(void*) {
    return false;
}

inline void wlr_x11_output_create(void*) {}
