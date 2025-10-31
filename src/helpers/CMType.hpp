#include <cstdint>
#include <string>
#include <optional>

namespace NCMType {
    enum eCMType : uint8_t {
        CM_AUTO = 0, // subject to change. srgb for 8bpc, wide for 10bpc if supported
        CM_SRGB,     // default, sRGB primaries
        CM_WIDE,     // wide color gamut, BT2020 primaries
        CM_EDID,     // primaries from edid (known to be inaccurate)
        CM_HDR,      // wide color gamut and HDR PQ transfer function
        CM_HDR_EDID, // same as CM_HDR with edid primaries
        CM_DCIP3,    // movie theatre with greenish white point
        CM_DP3,      // applle P3 variant with blueish white point
        CM_ADOBE,    // adobe colorspace
    };

    std::optional<eCMType> fromString(const std::string cmType);
    std::string            toString(eCMType cmType);
}
