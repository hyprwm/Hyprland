#include "ColorManagement.hpp"
#include "../math/Math.hpp"
#include <cstddef>
#include <fstream>

#include "../../debug/log/Logger.hpp"
#include "../../render/Texture.hpp"
#include "../../render/Renderer.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
using namespace Hyprutils::Utils;

#include <lcms2.h>

using namespace NColorManagement;

static std::vector<uint8_t> readBinary(const std::filesystem::path& file) {
    std::ifstream ifs(file, std::ios::binary);
    if (!ifs.good())
        return {};

    ifs.seekg(0, std::ios::end);
    size_t len = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    if (len <= 0)
        return {};

    std::vector<uint8_t> buf;
    buf.resize(len);
    ifs.read(reinterpret_cast<char*>(buf.data()), len);

    return buf;
}

static uint16_t bigEndianU16(const uint8_t* p) {
    return (uint16_t)((uint16_t)p[0] << 8 | (uint16_t)p[1]);
}

static uint32_t bigEndianU32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | (uint32_t)p[3];
}

static constexpr cmsTagSignature makeSig(char a, char b, char c, char d) {
    return sc<cmsTagSignature>(sc<uint32_t>(a) << 24 | sc<uint32_t>(b) << 16 | sc<uint32_t>(c) << 8 | sc<uint32_t>(d));
}

static constexpr cmsTagSignature VCGT_SIG = makeSig('v', 'c', 'g', 't');

//

static std::expected<std::optional<SVCGTTable16>, std::string> readVCGT16(cmsHPROFILE prof) {
    if (!cmsIsTag(prof, VCGT_SIG))
        return std::nullopt;

    cmsUInt32Number n = cmsReadRawTag(prof, VCGT_SIG, nullptr, 0);
    if (n < 8 + 4 + 2 + 2 + 2 + 2) // header + type + table header
        return std::unexpected("Malformed vcgt tag");

    std::vector<uint8_t> raw(n);
    if (cmsReadRawTag(prof, VCGT_SIG, raw.data(), n) != n)
        return std::unexpected("Malformed vcgt tag");

    // raw layout:
    // 0 ... 3: 'vcgt'
    // 4 ... 7: reserved
    // 8 ... 11: gammaType (0 = table)
    uint32_t gammaType = bigEndianU32(raw.data() + 8);
    if (gammaType != 0)
        return std::unexpected("VCGT formula type is not supported by Hyprland");

    SVCGTTable16 table;
    table.channels  = bigEndianU16(raw.data() + 12);
    table.entries   = bigEndianU16(raw.data() + 14);
    table.entrySize = bigEndianU16(raw.data() + 16);
    // raw+18: reserved u16

    Log::logger->log(Log::DEBUG, "readVCGT16: table has {} channels, {} entries, and entry size of {}", table.channels, table.entries, table.entrySize);

    if (table.channels != 3 || table.entrySize != 2 || table.entries == 0)
        return std::unexpected("invalid vcgt table size");

    size_t tableBytes = (size_t)table.channels * (size_t)table.entries * (size_t)table.entrySize;

    // VCGT is a piece of shit and some absolute fucking mongoloid idiots
    // decided it'd be great to have both 18 and 20
    // FUCK YOU
    size_t tableOff = 20;

    auto   readTable = [&] -> void {
        for (int c = 0; c < 3; ++c) {
            table.ch[c].resize(table.entries);
            for (uint16_t i = 0; i < table.entries; ++i) {
                const uint8_t* p = raw.data() + tableOff + static_cast<ptrdiff_t>((c * table.entries + i) * 2);
                table.ch[c][i]   = bigEndianU16(p); // 0 ... 65535
            }
        }
    };

    if (raw.size() < tableOff + tableBytes) {
        tableOff = 18;

        if (raw.size() < tableOff + tableBytes) {
            Log::logger->log(Log::ERR, "readVCGT16: table is too short, tag is invalid");
            return std::unexpected("table is too short");
        }

        Log::logger->log(Log::DEBUG, "readVCGT16: table is too short, but off = 18 fits. Attempting offset = 18");

        readTable();
    } else {
        readTable();

        // if the table's last entry is suspiciously low, we more than likely read an 18 as a 20.
        if (table.ch[0][table.entries - 1] < 30000) {
            Log::logger->log(Log::DEBUG, "readVCGT16: table is likely offset 18 not 20, re-reading");

            tableOff = 18;

            readTable();
        }
    }

    if (table.ch[0][table.entries - 1] < 30000) {
        Log::logger->log(Log::ERR, "readVCGT16: table is malformed, last value of a gamma ramp can't be {}", table.ch[0][table.entries - 1]);
        return std::unexpected("invalid table values");
    }

    Log::logger->log(Log::DEBUG, "readVCGT16: red channel: [{}, {}, ... {}, {}]", table.ch[0][0], table.ch[0][1], table.ch[0][table.entries - 2], table.ch[0][table.entries - 1]);
    Log::logger->log(Log::DEBUG, "readVCGT16: green channel: [{}, {}, ... {}, {}]", table.ch[1][0], table.ch[1][1], table.ch[1][table.entries - 2], table.ch[1][table.entries - 1]);
    Log::logger->log(Log::DEBUG, "readVCGT16: blue channel: [{}, {}, ... {}, {}]", table.ch[2][0], table.ch[2][1], table.ch[2][table.entries - 2], table.ch[2][table.entries - 1]);

    return table;
}

struct CmsProfileDeleter {
    void operator()(cmsHPROFILE p) const {
        if (p)
            cmsCloseProfile(p);
    }
};
struct CmsTransformDeleter {
    void operator()(cmsHTRANSFORM t) const {
        if (t)
            cmsDeleteTransform(t);
    }
};

using UniqueProfile   = std::unique_ptr<std::remove_pointer_t<cmsHPROFILE>, CmsProfileDeleter>;
using UniqueTransform = std::unique_ptr<std::remove_pointer_t<cmsHTRANSFORM>, CmsTransformDeleter>;

static UniqueProfile createLinearSRGBProfile() {
    cmsCIExyYTRIPLE prim{};
    // sRGB / Rec.709 primaries
    prim.Red.x   = 0.6400;
    prim.Red.y   = 0.3300;
    prim.Red.Y   = 1.0;
    prim.Green.x = 0.3000;
    prim.Green.y = 0.6000;
    prim.Green.Y = 1.0;
    prim.Blue.x  = 0.1500;
    prim.Blue.y  = 0.0600;
    prim.Blue.Y  = 1.0;

    cmsCIExyY wp{};
    wp.x = 0.3127;
    wp.y = 0.3290;
    wp.Y = 1.0; // D65

    cmsToneCurve* lin       = cmsBuildGamma(nullptr, 1.0);
    cmsToneCurve* curves[3] = {lin, lin, lin};

    cmsHPROFILE   p = cmsCreateRGBProfile(&wp, &prim, curves);

    cmsFreeToneCurve(lin);
    return UniqueProfile{p};
}

static std::expected<void, std::string> buildIcc3DLut(cmsHPROFILE profile, SImageDescription& image) {
    UniqueProfile src = createLinearSRGBProfile();
    if (!src)
        return std::unexpected("Failed to create linear sRGB profile");

    // Rendering intent: RELATIVE_COLORIMETRIC is common for displays; add BPC to be safe.
    const int             intent = INTENT_RELATIVE_COLORIMETRIC;
    const cmsUInt32Number flags  = cmsFLAGS_BLACKPOINTCOMPENSATION | cmsFLAGS_HIGHRESPRECALC; // good quality precalc in LCMS

    // float->float transform (linear input, encoded output in dst device space)
    UniqueTransform xform{cmsCreateTransform(src.get(), TYPE_RGB_FLT, profile, TYPE_RGB_FLT, intent, flags)};
    if (!xform)
        return std::unexpected("Failed to create ICC transform");

    Log::logger->log(Log::DEBUG, "Building a {}³ 3D LUT", image.icc.lutSize);

    image.icc.present = true;
    image.icc.lutDataPacked.resize(image.icc.lutSize * image.icc.lutSize * image.icc.lutSize * 3);

    auto idx = [&image](int r, int g, int b) -> size_t {
        //
        return ((size_t)b * image.icc.lutSize * image.icc.lutSize + (size_t)g * image.icc.lutSize + (size_t)r) * 3;
    };

    for (size_t bz = 0; bz < image.icc.lutSize; ++bz) {
        for (size_t gy = 0; gy < image.icc.lutSize; ++gy) {
            for (size_t rx = 0; rx < image.icc.lutSize; ++rx) {
                float in[3] = {
                    rx / float(image.icc.lutSize - 1),
                    gy / float(image.icc.lutSize - 1),
                    bz / float(image.icc.lutSize - 1),
                };
                float outRGB[3];
                cmsDoTransform(xform.get(), in, outRGB, 1);

                outRGB[0] = std::clamp(outRGB[0], 0.F, 1.F);
                outRGB[1] = std::clamp(outRGB[1], 0.F, 1.F);
                outRGB[2] = std::clamp(outRGB[2], 0.F, 1.F);

                const size_t o                 = idx(rx, gy, bz);
                image.icc.lutDataPacked[o + 0] = outRGB[0];
                image.icc.lutDataPacked[o + 1] = outRGB[1];
                image.icc.lutDataPacked[o + 2] = outRGB[2];
            }
        }
    }

    Log::logger->log(Log::DEBUG, "3D LUT constructed, size {}", image.icc.lutDataPacked.size());

    // upload
    image.icc.lutTexture = g_pHyprRenderer->createTexture(image.icc.lutDataPacked, image.icc.lutSize);

    return {};
}

std::expected<SImageDescription, std::string> SImageDescription::fromICC(const std::filesystem::path& file) {
    static auto     PVCGTENABLED = CConfigValue<Hyprlang::INT>("render:icc_vcgt_enabled");

    std::error_code ec;
    if (!std::filesystem::exists(file, ec) || ec)
        return std::unexpected("Invalid file");

    SImageDescription image;
    image.rawICC = readBinary(file);

    if (image.rawICC.empty())
        return std::unexpected("Failed to read file");

    cmsHPROFILE prof = cmsOpenProfileFromFile(file.string().c_str(), "r");
    if (!prof)
        return std::unexpected("CMS failed to open icc file");

    CScopeGuard x([&prof] { cmsCloseProfile(prof); });

    // only handle RGB (typical display profiles)
    if (cmsGetColorSpace(prof) != cmsSigRgbData)
        return std::unexpected("Only RGB display profiles are supported");

    Log::logger->log(Log::DEBUG, "============= Begin ICC load =============");
    Log::logger->log(Log::DEBUG, "ICC size: {} bytes", image.rawICC.size());

    if (const auto RET = buildIcc3DLut(prof, image); !RET)
        return std::unexpected(RET.error());

    if (*PVCGTENABLED) {
        auto vcgtRes = readVCGT16(prof);
        if (!vcgtRes)
            return std::unexpected(vcgtRes.error());

        image.icc.vcgt = *vcgtRes;

        if (!*vcgtRes)
            Log::logger->log(Log::DEBUG, "ICC profile has no VCGT data");
    } else
        Log::logger->log(Log::DEBUG, "Skipping VCGT load, disabled by config");

    Log::logger->log(Log::DEBUG, "============= End ICC load =============");

    return image;
}