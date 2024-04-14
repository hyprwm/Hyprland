#pragma once

int sd_booted(void);
int sd_notify(int unset_environment, const char* state);
