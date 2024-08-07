#pragma once
#include <string>
#include <vector>
#include <unordered_set>
#include <array>
#include <cstdint>
#include <hyprutils/math/Vector2D.hpp>
#include "helpers/memory/Memory.hpp"

extern "C" {
#include <X11/Xcursor/Xcursor.h>
}

// gangsta bootleg XCursor impl. adidas balkanized
struct SXCursorImage {
    Vector2D              size;
    Vector2D              hotspot;
    std::vector<uint32_t> pixels; // XPixel is a u32
    uint32_t              delay;  // animation delay to next frame (ms)
};

struct SXCursors {
    std::vector<SXCursorImage> images;
    std::string                shape;
};

class CXCursorManager {
  public:
    CXCursorManager();
    ~CXCursorManager() = default;

    void          loadTheme(const std::string& name, int size);
    SP<SXCursors> getShape(std::string const& shape, int size);

  private:
    SP<SXCursors>                   createCursor(std::string const& shape, XcursorImages* xImages);
    std::unordered_set<std::string> themePaths(std::string const& theme);
    std::string                     getLegacyShapeName(std::string const& shape);
    std::vector<SP<SXCursors>>      loadStandardCursors(std::string const& name, int size);
    std::vector<SP<SXCursors>>      loadAllFromDir(std::string const& path, int size);

    int                             lastLoadSize = 0;
    std::string                     themeName    = "";
    SP<SXCursors>                   defaultCursor;
    SP<SXCursors>                   hyprCursor;
    std::vector<SP<SXCursors>>      cursors;
};