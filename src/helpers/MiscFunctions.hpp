#pragma once

#include "../includes.hpp"

std::string absolutePath(const std::string&, const std::string&);
void        addWLSignal(wl_signal*, wl_listener*, void* pOwner, const std::string& ownerString);
std::string getFormat(const char* fmt, ...); // Basically Debug::log to a string
std::string escapeJSONStrings(const std::string& str);
void        scaleBox(wlr_box*, float);
std::string removeBeginEndSpacesTabs(std::string);
bool        isNumber(const std::string&, bool allowfloat = false);
bool        isDirection(const std::string&);
int         getWorkspaceIDFromString(const std::string&, std::string&);
float       vecToRectDistanceSquared(const Vector2D& vec, const Vector2D& p1, const Vector2D& p2);
void        logSystemInfo();
std::string execAndGet(const char*);
int64_t     getPPIDof(int64_t pid);
int64_t     configStringToInt(const std::string&);
float       getPlusMinusKeywordResult(std::string in, float relative);
void        matrixProjection(float mat[9], int w, int h, wl_output_transform tr);
double      normalizeAngleRad(double ang);
std::string replaceInString(std::string subject, const std::string& search, const std::string& replace);