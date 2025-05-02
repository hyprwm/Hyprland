#pragma once
#include <string>
#include <vector>
#include <set>
#include <array>
#include <cstdint>
#include <hyprutils/math/Vector2D.hpp>
#include "helpers/memory/Memory.hpp"

// gangsta bootleg XCursor impl. adidas balkanized
struct SXCursorImage {
    Hyprutils::Math::Vector2D size;
    Hyprutils::Math::Vector2D hotspot;
    std::vector<uint32_t>     pixels; // XPixel is a u32
    uint32_t                  delay;  // animation delay to next frame (ms)
};

struct SXCursors {
    std::vector<SXCursorImage> images;
    std::string                shape;
};

class CXCursorManager {
  public:
    CXCursorManager();
    ~CXCursorManager() = default;

    void          loadTheme(const std::string& name, int size, float scale);
    SP<SXCursors> getShape(std::string const& shape, int size, float scale);
    void          syncGsettings();

  private:
    SP<SXCursors>              createCursor(std::string const& shape, void* /* XcursorImages* */ xImages);
    std::set<std::string>      themePaths(std::string const& theme);
    std::string                getLegacyShapeName(std::string const& shape);
    std::vector<SP<SXCursors>> loadStandardCursors(std::string const& name, int size);
    std::vector<SP<SXCursors>> loadAllFromDir(std::string const& path, int size);

    int                        m_lastLoadSize  = 0;
    float                      m_lastLoadScale = 0;
    std::string                m_themeName     = "";
    SP<SXCursors>              m_defaultCursor;
    SP<SXCursors>              m_hyprCursor;
    std::vector<SP<SXCursors>> m_cursors;
};
