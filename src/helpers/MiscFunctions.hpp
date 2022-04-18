#pragma once

#include "../includes.hpp"

void addWLSignal(wl_signal*, wl_listener*, void* pOwner, std::string ownerString);
void wlr_signal_emit_safe(struct wl_signal *signal, void *data);
std::string getFormat(const char *fmt, ...); // Basically Debug::log to a string
void scaleBox(wlr_box*, float);
std::string removeBeginEndSpacesTabs(std::string);