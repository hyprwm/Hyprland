#pragma once

namespace Config {
    class CCssGapData;
}

namespace Hyprutils::Math {
    class Vector2D;
}

namespace Math {
    struct SCornerRadii {
        SCornerRadii(float radius = 0.F);
        SCornerRadii(float topLeft, float topRight, float bottomRight, float bottomLeft);
        explicit SCornerRadii(const Config::CCssGapData& value);

        float        topLeft = 0.F, topRight = 0.F, bottomRight = 0.F, bottomLeft = 0.F;

        bool         operator==(const SCornerRadii&) const = default;
        SCornerRadii operator*(float scale) const;
        SCornerRadii operator/(float scale) const;
        float        max() const;
        bool         isRounded() const;
        SCornerRadii clamped(const Hyprutils::Math::Vector2D& size) const;
        SCornerRadii inset(float amount) const;
        SCornerRadii expanded(float amount) const;
        float        at(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::Vector2D& size) const;
    };

    bool isInRoundedCorner(const Hyprutils::Math::Vector2D& point, const Hyprutils::Math::Vector2D& size, const SCornerRadii& radii, float power);
}
