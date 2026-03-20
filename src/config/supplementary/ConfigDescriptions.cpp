#include "ConfigDescriptions.hpp"

// FIXME: this NO NO NO!!!!
#include "../ConfigManager.hpp"
#include "../shared/complex/ComplexDataType.hpp"
#include "../../helpers/MiscFunctions.hpp"

#include <typeindex>
#include <hyprlang.hpp>

using namespace Config::Supplementary;

std::string SConfigOptionDescription::jsonify() const {
    auto parseData = [this]() -> std::string {
        return std::visit(
            [this](auto&& val) {
                const auto CONFIG_VAL = Config::mgr()->getConfigValue(value);
                if (!CONFIG_VAL.dataptr) {
                    Log::logger->log(Log::ERR, "invalid SConfigOptionDescription: no config option {} exists", value);
                    return std::string{""};
                }
                const char* const EXPLICIT = CONFIG_VAL.setByUser ? "true" : "false";

                std::string       currentValue = "undefined";

                if (typeid(Config::INTEGER) == std::type_index(*CONFIG_VAL.type))
                    currentValue = std::format("{}", **rc<Config::INTEGER* const*>(CONFIG_VAL.dataptr));
                else if (typeid(Config::FLOAT) == std::type_index(*CONFIG_VAL.type))
                    currentValue = std::format("{:.2f}", **rc<Config::FLOAT* const*>(CONFIG_VAL.dataptr));
                else if (typeid(Hyprlang::STRING) == std::type_index(*CONFIG_VAL.type))
                    currentValue = std::format("\"{}\"", *rc<Hyprlang::STRING const*>(CONFIG_VAL.dataptr));
                else if (typeid(Config::STRING) == std::type_index(*CONFIG_VAL.type))
                    currentValue = std::format("\"{}\"", **rc<Config::STRING* const*>(CONFIG_VAL.dataptr));
                else if (typeid(Config::VEC2) == std::type_index(*CONFIG_VAL.type)) {
                    const auto V = **rc<Config::VEC2* const*>(CONFIG_VAL.dataptr);
                    currentValue = std::format("\"{}, {}\"", V.x, V.y);
                } else if (typeid(Hyprlang::VEC2) == std::type_index(*CONFIG_VAL.type)) {
                    const auto V = **rc<Hyprlang::VEC2* const*>(CONFIG_VAL.dataptr);
                    currentValue = std::format("\"{}, {}\"", V.x, V.y);
                } else if (typeid(Config::IComplexConfigValue*) == std::type_index(*CONFIG_VAL.type)) {
                    const auto DATA = *rc<Config::IComplexConfigValue* const*>(CONFIG_VAL.dataptr);
                    currentValue    = std::format("\"{}\"", DATA->toString());
                } else if (typeid(void*) == std::type_index(*CONFIG_VAL.type)) {
                    // legacy hyprlang value
                    const auto DATA  = *rc<Hyprlang::CConfigCustomValueType* const*>(CONFIG_VAL.dataptr);
                    const auto DATA2 = rc<Config::IComplexConfigValue*>(DATA->getData());
                    currentValue     = std::format("\"{}\"", DATA2->toString());
                }

                try {
                    using T = std::decay_t<decltype(val)>;
                    if constexpr (std::is_same_v<T, SStringData>) {
                        return std::format(R"#(     "value": "{}",
        "current": {},
        "explicit": {})#",
                                           val.value, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SRangeData>) {
                        return std::format(R"#(     "value": {},
        "min": {},
        "max": {},
        "current": {},
        "explicit": {})#",
                                           val.value, val.min, val.max, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SFloatData>) {
                        return std::format(R"#(     "value": {},
        "min": {},
        "max": {},
        "current": {},
        "explicit": {})#",
                                           val.value, val.min, val.max, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SColorData>) {
                        return std::format(R"#(     "value": "{}",
        "current": {},
        "explicit": {})#",
                                           val.color.getAsHex(), currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SBoolData>) {
                        return std::format(R"#(     "value": {},
        "current": {},
        "explicit": {})#",
                                           val.value, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SChoiceData>) {
                        return std::format(R"#(     "value": "{}",
        "firstIndex": {},
        "current": {},
        "explicit": {})#",
                                           val.choices, val.firstIndex, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SVectorData>) {
                        return std::format(R"#(     "x": {},
        "y": {},
        "min_x": {},
        "min_y": {},
        "max_x": {},
        "max_y": {},
        "current": {},
        "explicit": {})#",
                                           val.vec.x, val.vec.y, val.min.x, val.min.y, val.max.x, val.max.y, currentValue, EXPLICIT);
                    } else if constexpr (std::is_same_v<T, SGradientData>) {
                        return std::format(R"#(     "value": "{}",
        "current": {},
        "explicit": {})#",
                                           val.gradient, currentValue, EXPLICIT);
                    }

                } catch (std::bad_any_cast& e) { Log::logger->log(Log::ERR, "Bad any_cast on value {} in descriptions", value); }
                return std::string{""};
            },
            data);
    };

    std::string json = std::format(R"#({{
    "value": "{}",
    "description": "{}",
    "type": {},
    "flags": {},
    "data": {{
        {}
    }}
}})#",
                                   value, escapeJSONStrings(description), sc<uint16_t>(type), sc<uint32_t>(flags), parseData());

    return json;
}