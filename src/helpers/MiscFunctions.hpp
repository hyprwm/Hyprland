#pragma once

#include "../includes.hpp"

void addWLSignal(wl_signal*, wl_listener*, void* pOwner, std::string ownerString);
void wlr_signal_emit_safe(struct wl_signal *signal, void *data);
std::string getFormat(const char *fmt, ...); // Basically Debug::log to a string
void scaleBox(wlr_box*, float);
std::string removeBeginEndSpacesTabs(std::string);
bool isNumber(const std::string&, bool allowfloat = false);
bool isDirection(const std::string&);
int getWorkspaceIDFromString(const std::string&, std::string&);
float vecToRectDistanceSquared(const Vector2D& vec, const Vector2D& p1, const Vector2D& p2);
void logSystemInfo();
std::string execAndGet(const char*);

float getPlusMinusKeywordResult(std::string in, float relative);

void matrixProjection(float mat[9], int w, int h, wl_output_transform tr);