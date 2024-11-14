#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <gio/gio.h>
#include <gio/gsettingsschema.h>
#include "config/ConfigValue.hpp"
#include "helpers/CursorShapes.hpp"
#include "../managers/CursorManager.hpp"
#include "debug/Log.hpp"
#include "XCursorManager.hpp"

// clang-format off
static std::vector<uint32_t> HYPR_XCURSOR_PIXELS = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x1b001816, 0x01000101, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x8e008173, 0x5f00564d, 0x16001412, 0x09000807, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x2b002624, 0x05000404, 0x00000000, 0x35002f2b, 0xd400bead,
    0xc300b09e, 0x90008275, 0x44003e37, 0x04000403, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x67005a56,
    0x6f00615c, 0x00000000, 0x00000000, 0x8b007c72, 0xf200d7c6, 0xfa00e0cc, 0xe800d0bd, 0xa0009181, 0x44003e37, 0x1a001815, 0x06000505, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x8d007976, 0xd600b8b3, 0x2500201f, 0x00000000, 0x17001413, 0xbd00a79c, 0xf600dacb, 0xff00e3d1, 0xfc00e1ce, 0xe800d0bc, 0xbf00ac9b,
    0x95008778, 0x51004a41, 0x0f000e0c, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x92007b7b, 0xf500d0cf, 0x9e008685, 0x00000000, 0x00000000, 0x23001f1d, 0x64005853,
    0x9b008980, 0xd900bfb3, 0xfb00dfce, 0xff00e4d0, 0xfb00e1cd, 0xec00d5c0, 0xa7009788, 0x47004139, 0x1e001b18, 0x05000504, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xa200878a, 0xff00d6d9, 0xd600b4b5,
    0x0e000c0c, 0x00000000, 0x00000000, 0x02000202, 0x0c000b0a, 0x30002a28, 0x8e007d75, 0xd600bdb0, 0xef00d4c4, 0xfb00e0ce, 0xff00e4d0, 0xe600cfbb, 0xb800a695, 0x5f00564d,
    0x06000505, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x02000202, 0xc600a3aa, 0xff00d3da, 0xea00c3c8, 0x08000707, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x01000101, 0x2a002523, 0x61005550, 0x9500837b,
    0xd800bfb1, 0xfd00e1cf, 0xff00e5d0, 0xf500dcc7, 0x7c007065, 0x2a002622, 0x01000101, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x06000505, 0xd600aeb9, 0xff00d0dc, 0xcc00a7af, 0x04000303, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x01000101, 0x01000101, 0x2c002724, 0xa1008e85, 0xe600ccbd, 0xf800ddcb, 0xef00d6c3, 0xc300af9f, 0x2c002824, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x09000708, 0xd800adbc, 0xff00cdde, 0xb90095a0, 0x02000202, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x10000e0d, 0x4b00423e, 0xa4009088, 0xfd00dfd0, 0xff00e3d1,
    0xae009c8f, 0x42003b36, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x14001012, 0xf400c0d6,
    0xff00cadf, 0xb2008e9c, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x02000202, 0x1200100f, 0xa2008e86, 0xec00cfc3, 0xfc00ded0, 0xc300ada0, 0x15001311, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x2e002429, 0xfd00c4e0, 0xff00c7e2, 0x8f00707e, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x1e001a19, 0x75006662, 0xfb00dbd1, 0xf700d9cc, 0x9600847c, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3e002f37, 0xfc00c1e1, 0xff00c5e3, 0x60004b55, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x15001212, 0xa6008f8b, 0xff00ddd5,
    0xf800d8ce, 0x36002f2d, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x51003d49, 0xfe00bfe5, 0xfe00c1e4, 0x4c003a44,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x01000101, 0x1d001918, 0xf400d1cd, 0xfe00dad5, 0xb3009a96, 0x03000303, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x66004b5d, 0xff00bee7, 0xfd00bee5, 0x4500343f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02000202, 0x82006e6e, 0xff00d8d7, 0xd800b9b6, 0x33002c2b, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x70005267, 0xff00bbe9, 0xfa00b8e3, 0x29001e25, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3f003536, 0xea00c3c7, 0xf800d1d3,
    0x4a003e3f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x5f004458, 0xff00b8eb, 0xf400b1e0, 0x29001e26, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x1b001617, 0xe100bac0, 0xff00d4da, 0x82006c6f, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x5a004054, 0xfe00b4eb,
    0xfb00b3e8, 0x3b002a36, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0a000809, 0xc900a4ad, 0xff00d1dc, 0x88007075, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x44002f3f, 0xf300aae3, 0xfc00b1ea, 0x48003343, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x05000404, 0xdf00b3c2, 0xff00cedd, 0x8f00747c, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x25001a23, 0xf200a6e4, 0xff00b1ef, 0x84005c7c, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x09000708,
    0xe400b5c8, 0xff00cbdf, 0x78006068, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x12000c11, 0xc00082b6, 0xff00aef1, 0xaa0075a0,
    0x11000c10, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x1e00171b, 0xf700c1db, 0xff00c8e1, 0x4b003b42, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x05000305, 0x8d005f86, 0xfc00aaef, 0xed00a0e1, 0x26001a24, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6f005463, 0xff00c4e3, 0xf500beda, 0x0c00090b, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3e00293b, 0xed009de2, 0xff00aaf3, 0x8b005d84, 0x04000304, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x07000506, 0xeb00b1d4, 0xff00c1e6, 0xba008ea7,
    0x02000202, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xb20075ab, 0xff00a7f4, 0xf300a1e9, 0x35002333,
    0x06000406, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x8900647d, 0xff00bde8, 0xfb00bce2, 0x26001d22, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x2b001c29, 0xf1009ce8, 0xfe00a5f4, 0xc60082be, 0x3b002738, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x01000101, 0x54003c4d, 0xfd00b8e8, 0xff00baea, 0x96006f89, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x78004d74, 0xe20091da, 0xff00a5f6, 0xb60077af, 0x42002b3f, 0x0c00080c, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x9400688a, 0xff00b5ed, 0xff00b6ec, 0xcc0093bc, 0x02000102, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04000304, 0x8100527d, 0xeb0096e4, 0xf900a0f1, 0xdc008dd4,
    0x9b006595, 0x32002130, 0x0a00070a, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x2100161f, 0x5300394e, 0xe2009cd4, 0xff00b1ef, 0xff00b2ee, 0xc8008cba, 0x17001015,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x19001018, 0x5e003b5b, 0xcb0081c5, 0xff00a2f8, 0xfc00a1f4, 0xde0090d6, 0xa9006da3, 0x8300567e, 0x6f00496a, 0x76004e71, 0xbb007db2, 0xeb009edf, 0xfb00a9ee, 0xff00adf1,
    0xfd00adee, 0x9e006d95, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x04000304, 0x34002133, 0xa60069a1, 0xd70089d1, 0xf2009bea, 0xff00a3f7, 0xfb00a1f2, 0xfb00a2f2, 0xff00a6f5,
    0xff00a8f4, 0xfd00a7f2, 0xed009ee2, 0xcf008bc5, 0x14000e13, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x11000b11, 0x35002134, 0x5b003959,
    0x8b005887, 0xb30072ae, 0xc90080c3, 0xd10086ca, 0xa6006ba0, 0x65004261, 0x3c002839, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x01000101, 0x06000406, 0x0d00080d, 0x0f000a0f, 0x0a00060a, 0x01000101, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000};
// clang-format on

CXCursorManager::CXCursorManager() {
    hyprCursor = makeShared<SXCursors>();
    SXCursorImage image;
    image.size    = {32, 32};
    image.hotspot = {3, 2};
    image.pixels  = HYPR_XCURSOR_PIXELS;
    image.delay   = 0;

    hyprCursor->images.push_back(image);
    hyprCursor->shape = "left_ptr";
    defaultCursor     = hyprCursor;
}

void CXCursorManager::loadTheme(std::string const& name, int size, float scale) {
    if (lastLoadSize == (size * std::ceil(scale)) && themeName == name && lastLoadScale == scale)
        return;

    lastLoadSize  = size * std::ceil(scale);
    lastLoadScale = scale;
    themeName     = name.empty() ? "default" : name;
    defaultCursor.reset();
    cursors.clear();

    auto paths = themePaths(themeName);
    if (paths.empty()) {
        Debug::log(ERR, "XCursor librarypath is empty loading standard XCursors");
        cursors = loadStandardCursors(themeName, lastLoadSize);
    } else {
        for (auto const& p : paths) {
            try {
                auto dirCursors = loadAllFromDir(p, lastLoadSize);
                std::copy_if(dirCursors.begin(), dirCursors.end(), std::back_inserter(cursors),
                             [this](auto const& p) { return std::none_of(cursors.begin(), cursors.end(), [&p](auto const& dp) { return dp->shape == p->shape; }); });
            } catch (std::exception& e) { Debug::log(ERR, "XCursor path {} can't be loaded: threw error {}", p, e.what()); }
        }
    }

    if (cursors.empty()) {
        Debug::log(ERR, "XCursor failed finding any shapes in theme \"{}\".", themeName);
        defaultCursor = hyprCursor;
        return;
    }

    for (auto const& shape : CURSOR_SHAPE_NAMES) {
        auto legacyName = getLegacyShapeName(shape);
        if (legacyName.empty())
            continue;

        auto it = std::find_if(cursors.begin(), cursors.end(), [&legacyName](auto const& c) { return c->shape == legacyName; });

        if (it == cursors.end()) {
            Debug::log(LOG, "XCursor failed to find a legacy shape with name {}, skipping", legacyName);
            continue;
        }

        if (std::any_of(cursors.begin(), cursors.end(), [&shape](auto const& dp) { return dp->shape == shape; })) {
            Debug::log(LOG, "XCursor already has a shape {} loaded, skipping", shape);
            continue;
        }

        auto cursor    = makeShared<SXCursors>();
        cursor->images = it->get()->images;
        cursor->shape  = shape;

        cursors.emplace_back(cursor);
    }

    static auto SYNCGSETTINGS = CConfigValue<Hyprlang::INT>("cursor:sync_gsettings_theme");
    if (*SYNCGSETTINGS)
        syncGsettings();
}

SP<SXCursors> CXCursorManager::getShape(std::string const& shape, int size, float scale) {
    // monitor scaling changed etc, so reload theme with new size.
    if ((size * std::ceil(scale)) != lastLoadSize || scale != lastLoadScale)
        loadTheme(themeName, size, scale);

    // try to get an icon we know if we have one
    for (auto const& c : cursors) {
        if (c->shape != shape)
            continue;

        return c;
    }

    Debug::log(WARN, "XCursor couldn't find shape {} , using default cursor instead", shape);
    return defaultCursor;
}

SP<SXCursors> CXCursorManager::createCursor(std::string const& shape, XcursorImages* xImages) {
    auto xcursor = makeShared<SXCursors>();

    for (int i = 0; i < xImages->nimage; i++) {
        auto          xImage = xImages->images[i];
        SXCursorImage image;
        image.size    = {(int)xImage->width, (int)xImage->height};
        image.hotspot = {(int)xImage->xhot, (int)xImage->yhot};
        image.pixels.resize(xImage->width * xImage->height);
        std::memcpy(image.pixels.data(), xImage->pixels, xImage->width * xImage->height * sizeof(uint32_t));
        image.delay = xImage->delay;

        xcursor->images.emplace_back(image);
    }

    xcursor->shape = shape;

    return xcursor;
}

std::set<std::string> CXCursorManager::themePaths(std::string const& theme) {
    auto const* path = XcursorLibraryPath();

    auto        expandTilde = [](std::string const& path) {
        if (!path.empty() && path[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home)
                return std::string(home) + path.substr(1);
        }
        return path;
    };

    auto getInheritThemes = [](std::string const& indexTheme) {
        std::ifstream            infile(indexTheme);
        std::string              line;
        std::vector<std::string> themes;

        Debug::log(LOG, "XCursor parsing index.theme {}", indexTheme);

        while (std::getline(infile, line)) {
            if (line.empty())
                continue;

            // Trim leading and trailing whitespace
            auto pos = line.find_first_not_of(" \t\n\r");
            if (pos != std::string::npos)
                line.erase(0, pos);

            pos = line.find_last_not_of(" \t\n\r");
            if (pos != std::string::npos && pos < line.length()) {
                line.erase(pos + 1);
            }

            if (line.rfind("Inherits", 8) != std::string::npos) { // Check if line starts with "Inherits"
                std::string inheritThemes = line.substr(8);       // Extract the part after "Inherits"
                if (inheritThemes.empty())
                    continue;

                // Remove leading whitespace from inheritThemes and =
                pos = inheritThemes.find_first_not_of(" \t\n\r");
                if (pos != std::string::npos)
                    inheritThemes.erase(0, pos);

                if (inheritThemes.empty())
                    continue;

                if (inheritThemes.at(0) == '=')
                    inheritThemes.erase(0, 1);
                else
                    continue; // not correct formatted index.theme

                pos = inheritThemes.find_first_not_of(" \t\n\r");
                if (pos != std::string::npos)
                    inheritThemes.erase(0, pos);

                std::stringstream inheritStream(inheritThemes);
                std::string       inheritTheme;
                while (std::getline(inheritStream, inheritTheme, ',')) {
                    if (inheritTheme.empty())
                        continue;

                    // Trim leading and trailing whitespace from each theme
                    pos = inheritTheme.find_first_not_of(" \t\n\r");
                    if (pos != std::string::npos)
                        inheritTheme.erase(0, pos);

                    pos = inheritTheme.find_last_not_of(" \t\n\r");
                    if (pos != std::string::npos && pos < inheritTheme.length())
                        inheritTheme.erase(inheritTheme.find_last_not_of(" \t\n\r") + 1);

                    themes.push_back(inheritTheme);
                }
            }
        }
        infile.close();

        return themes;
    };

    std::set<std::string> paths;
    std::set<std::string> inherits;

    auto                  scanTheme = [&path, &paths, &expandTilde, &inherits, &getInheritThemes](auto const& t) {
        std::stringstream ss(path);
        std::string       line;

        Debug::log(LOG, "XCursor scanning theme {}", t);

        while (std::getline(ss, line, ':')) {
            auto p = expandTilde(line + "/" + t + "/cursors");
            if (std::filesystem::exists(p) && std::filesystem::is_directory(p)) {
                Debug::log(LOG, "XCursor using theme path {}", p);
                paths.insert(p);
            }

            auto inherit = expandTilde(line + "/" + t + "/index.theme");
            if (std::filesystem::exists(inherit) && std::filesystem::is_regular_file(inherit)) {
                auto inheritThemes = getInheritThemes(inherit);
                for (auto const& i : inheritThemes) {
                    Debug::log(LOG, "XCursor theme {} inherits {}", t, i);
                    inherits.insert(i);
                }
            }
        }
    };

    if (path) {
        scanTheme(theme);
        while (!inherits.empty()) {
            auto oldInherits = inherits;
            for (auto const& i : oldInherits)
                scanTheme(i);

            if (oldInherits.size() == inherits.size())
                break;
        }
    }

    return paths;
}

std::string CXCursorManager::getLegacyShapeName(std::string const& shape) {
    if (shape == "invalid")
        return std::string();
    else if (shape == "default")
        return "left_ptr";
    else if (shape == "context-menu")
        return "left_ptr";
    else if (shape == "help")
        return "left_ptr";
    else if (shape == "pointer")
        return "hand2";
    else if (shape == "progress")
        return "watch";
    else if (shape == "wait")
        return "watch";
    else if (shape == "cell")
        return "plus";
    else if (shape == "crosshair")
        return "cross";
    else if (shape == "text")
        return "xterm";
    else if (shape == "vertical-text")
        return "xterm";
    else if (shape == "alias")
        return "dnd-link";
    else if (shape == "copy")
        return "dnd-copy";
    else if (shape == "move")
        return "dnd-move";
    else if (shape == "no-drop")
        return "dnd-none";
    else if (shape == "not-allowed")
        return "crossed_circle";
    else if (shape == "grab")
        return "hand1";
    else if (shape == "grabbing")
        return "hand1";
    else if (shape == "e-resize")
        return "right_side";
    else if (shape == "n-resize")
        return "top_side";
    else if (shape == "ne-resize")
        return "top_right_corner";
    else if (shape == "nw-resize")
        return "top_left_corner";
    else if (shape == "s-resize")
        return "bottom_side";
    else if (shape == "se-resize")
        return "bottom_right_corner";
    else if (shape == "sw-resize")
        return "bottom_left_corner";
    else if (shape == "w-resize")
        return "left_side";
    else if (shape == "ew-resize")
        return "sb_h_double_arrow";
    else if (shape == "ns-resize")
        return "sb_v_double_arrow";
    else if (shape == "nesw-resize")
        return "fd_double_arrow";
    else if (shape == "nwse-resize")
        return "bd_double_arrow";
    else if (shape == "col-resize")
        return "sb_h_double_arrow";
    else if (shape == "row-resize")
        return "sb_v_double_arrow";
    else if (shape == "all-scroll")
        return "fleur";
    else if (shape == "zoom-in")
        return "left_ptr";
    else if (shape == "zoom-out")
        return "left_ptr";

    return std::string();
};

// Taken from https://gitlab.freedesktop.org/xorg/lib/libxcursor/-/blob/master/src/library.c
// clang-format off
static std::array<const char*, 77> XCURSOR_STANDARD_NAMES = {
    "X_cursor",
    "arrow",
    "based_arrow_down",
    "based_arrow_up",
    "boat",
    "bogosity",
    "bottom_left_corner",
    "bottom_right_corner",
    "bottom_side",
    "bottom_tee",
    "box_spiral",
    "center_ptr",
    "circle",
    "clock",
    "coffee_mug",
    "cross",
    "cross_reverse",
    "crosshair",
    "diamond_cross",
    "dot",
    "dotbox",
    "double_arrow",
    "draft_large",
    "draft_small",
    "draped_box",
    "exchange",
    "fleur",
    "gobbler",
    "gumby",
    "hand1",
    "hand2",
    "heart",
    "icon",
    "iron_cross",
    "left_ptr",
    "left_side",
    "left_tee",
    "leftbutton",
    "ll_angle",
    "lr_angle",
    "man",
    "middlebutton",
    "mouse",
    "pencil",
    "pirate",
    "plus",
    "question_arrow",
    "right_ptr",
    "right_side",
    "right_tee",
    "rightbutton",
    "rtl_logo",
    "sailboat",
    "sb_down_arrow",
    "sb_h_double_arrow",
    "sb_left_arrow",
    "sb_right_arrow",
    "sb_up_arrow",
    "sb_v_double_arrow",
    "shuttle",
    "sizing",
    "spider",
    "spraycan",
    "star",
    "target",
    "tcross",
    "top_left_arrow",
    "top_left_corner",
    "top_right_corner",
    "top_side",
    "top_tee",
    "trek",
    "ul_angle",
    "umbrella",
    "ur_angle",
    "watch",
    "xterm",
};
// clang-format on

std::vector<SP<SXCursors>> CXCursorManager::loadStandardCursors(std::string const& name, int size) {
    std::vector<SP<SXCursors>> newCursors;

    // load the default xcursor shapes that exist in the theme
    for (size_t i = 0; i < XCURSOR_STANDARD_NAMES.size(); ++i) {
        std::string shape{XCURSOR_STANDARD_NAMES.at(i)};
        auto        xImages = XcursorShapeLoadImages(i << 1 /* wtf xcursor? */, name.c_str(), size);

        if (!xImages) {
            Debug::log(WARN, "XCursor failed to find a shape with name {}, trying size 24.", shape);
            xImages = XcursorShapeLoadImages(i << 1 /* wtf xcursor? */, name.c_str(), 24);

            if (!xImages) {
                Debug::log(WARN, "XCursor failed to find a shape with name {}, skipping", shape);
                continue;
            }
        }

        auto cursor = createCursor(shape, xImages);
        newCursors.emplace_back(cursor);

        if (!defaultCursor && (shape == "left_ptr" || shape == "arrow"))
            defaultCursor = cursor;

        XcursorImagesDestroy(xImages);
    }

    // broken theme.. just set it.
    if (!newCursors.empty() && !defaultCursor)
        defaultCursor = newCursors.front();

    return newCursors;
}

std::vector<SP<SXCursors>> CXCursorManager::loadAllFromDir(std::string const& path, int size) {
    std::vector<SP<SXCursors>> newCursors;

    if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            std::error_code e1, e2;
            if ((!entry.is_regular_file(e1) && !entry.is_symlink(e2)) || e1 || e2) {
                Debug::log(WARN, "XCursor failed to load shape {}: {}", entry.path().stem().string(), e1 ? e1.message() : e2.message());
                continue;
            }

            auto const& full = entry.path().string();
            using PcloseType = int (*)(FILE*);
            const std::unique_ptr<FILE, PcloseType> f(fopen(full.c_str(), "r"), static_cast<PcloseType>(fclose));

            if (!f)
                continue;

            auto xImages = XcursorFileLoadImages(f.get(), size);

            if (!xImages) {
                Debug::log(WARN, "XCursor failed to load image {}, trying size 24.", full);
                xImages = XcursorFileLoadImages(f.get(), 24);

                if (!xImages) {
                    Debug::log(WARN, "XCursor failed to load image {}, skipping", full);
                    continue;
                }
            }

            auto const& shape  = entry.path().filename().string();
            auto        cursor = createCursor(shape, xImages);
            newCursors.emplace_back(cursor);

            if (!defaultCursor && (shape == "left_ptr" || shape == "arrow"))
                defaultCursor = cursor;

            XcursorImagesDestroy(xImages);
        }
    }

    // broken theme.. just set it.
    if (!newCursors.empty() && !defaultCursor)
        defaultCursor = newCursors.front();

    return newCursors;
}

void CXCursorManager::syncGsettings() {
    auto checkParamExists = [](std::string const& paramName, std::string const& category) {
        auto* gSettingsSchemaSource = g_settings_schema_source_get_default();

        if (!gSettingsSchemaSource) {
            Debug::log(WARN, "GSettings default schema source does not exist, cant sync GSettings");
            return false;
        }

        auto* gSettingsSchema = g_settings_schema_source_lookup(gSettingsSchemaSource, category.c_str(), true);
        bool  hasParam        = false;

        if (gSettingsSchema != NULL) {
            hasParam = gSettingsSchema && g_settings_schema_has_key(gSettingsSchema, paramName.c_str());
            g_settings_schema_unref(gSettingsSchema);
        }

        return hasParam;
    };

    using SettingValue = std::variant<std::string, int>;
    auto setValue      = [&checkParamExists](std::string const& paramName, const SettingValue& paramValue, std::string const& category) {
        if (!checkParamExists(paramName, category)) {
            Debug::log(WARN, "GSettings parameter doesnt exist {} in {}", paramName, category);
            return;
        }

        auto* gsettings = g_settings_new(category.c_str());

        if (!gsettings) {
            Debug::log(WARN, "GSettings failed to allocate new settings with category {}", category);
            return;
        }

        std::visit(
            [&](auto&& value) {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, std::string>)
                    g_settings_set_string(gsettings, paramName.c_str(), value.c_str());
                else if constexpr (std::is_same_v<T, int>)
                    g_settings_set_int(gsettings, paramName.c_str(), value);
            },
            paramValue);

        g_settings_sync();
        g_object_unref(gsettings);
    };

    int unscaledSize = lastLoadSize / std::ceil(lastLoadScale);
    setValue("cursor-theme", themeName, "org.gnome.desktop.interface");
    setValue("cursor-size", unscaledSize, "org.gnome.desktop.interface");
}
