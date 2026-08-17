#pragma once

#include <unordered_map>
#include <string>
#include <cstddef>
#include <optional>

#include "../../helpers/math/Math.hpp"
#include "../DesktopTypes.hpp"
#include "../view/window/Window.hpp"

namespace Desktop {
    struct SFloatCacheKey {
        size_t hash;

        SFloatCacheKey(PHLWINDOW window, bool initial) {
            // Base hash from class/title
            size_t baseHash = initial ? (std::hash<std::string>{}(window->metadata().initialAppID()) ^ (std::hash<std::string>{}(window->metadata().initialTitle()) << 1)) :
                                        (std::hash<std::string>{}(window->metadata().appID()) ^ (std::hash<std::string>{}(window->metadata().title()) << 1));

            // Use empty string as default tag value
            std::string tagValue = "";
            if (auto xdgTag = window->backend().metadata().tag)
                tagValue = xdgTag.value();

            // Combine hashes
            hash = baseHash ^ (std::hash<std::string>{}(tagValue) << 2);
        }

        bool operator==(const SFloatCacheKey& other) const {
            return hash == other.hash;
        }
    };
}

namespace std {
    template <>
    struct hash<Desktop::SFloatCacheKey> {
        size_t operator()(const Desktop::SFloatCacheKey& id) const {
            return id.hash;
        }
    };
}

namespace Desktop {
    class CFloatStateCache {
      public:
        CFloatStateCache()  = default;
        ~CFloatStateCache() = default;

        void                    remember(PHLWINDOW window, const Vector2D& size);
        std::optional<Vector2D> get(PHLWINDOW window);

      private:
        std::unordered_map<SFloatCacheKey, Vector2D> m_storedSizes;
    };

    UP<CFloatStateCache>& floatState();
}
