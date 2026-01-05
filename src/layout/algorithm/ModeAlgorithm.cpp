#include "ModeAlgorithm.hpp"

using namespace Layout;

std::expected<void, std::string> IModeAlgorithm::layoutMsg(const std::string_view& sv) {
    return {};
}

std::optional<Vector2D> IModeAlgorithm::predictSizeForNewTarget() {
    return std::nullopt;
}
