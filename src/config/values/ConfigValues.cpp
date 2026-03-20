#include "ConfigValues.hpp"

using namespace Config;
using namespace Config::Values;

template <typename T>
static std::string opt(std::optional<T> x) {
    if (x)
        return std::format("{}", x.value());
    return "null";
}

template <>
std::string opt<Values::OptionMap>(std::optional<Values::OptionMap> x) {
    if (x) {
        std::string json = "[";
        for (const auto& [k, v] : *x) {
            json += std::format("{{ \"{}\": {} }},", k, v);
        }
        if (!json.empty())
            json.pop_back();
        json += "]";
        return json;
    }
    return "null";
}

static std::string jsonify(SP<IValue> v) {

    if (auto x = dc<CBoolValue*>(v.get()); x) {
        return std::format(
            R"#(
    {{
        "name": "{}",
        "description": "{}",
        "default": {},
        "current": {}
    }},)#",
            x->name(), x->description(), x->defaultVal(), x->value());
    }

    if (auto x = dc<CIntValue*>(v.get()); x) {
        return std::format(
            R"#(
    {{
        "name": "{}",
        "description": "{}",
        "default": {},
        "current": {},
        "min": {},
        "max": {},
        "map": {}
    }},)#",
            x->name(), x->description(), x->defaultVal(), x->value(), opt(x->m_min), opt(x->m_max), opt(x->m_map));
    }

    if (auto x = dc<CFloatValue*>(v.get()); x) {
        return std::format(
            R"#(
    {{
        "name": "{}",
        "description": "{}",
        "default": {},
        "current": {},
        "min": {},
        "max": {}
    }},)#",
            x->name(), x->description(), x->defaultVal(), x->value(), opt(x->m_min), opt(x->m_max));
    }

    if (auto x = dc<CCssGapValue*>(v.get()); x) {
        return std::format(
            R"#(
    {{
        "name": "{}",
        "description": "{}",
        "default": "{}",
        "current": "{}",
        "min": {},
        "max": {}
    }},)#",
            x->name(), x->description(), x->defaultVal().toString(), x->value().toString(), opt(x->m_min), opt(x->m_max));
    }

    if (auto x = dc<CFontWeightValue*>(v.get()); x) {
        return std::format(
            R"#(
    {{
        "name": "{}",
        "description": "{}",
        "default": "{}",
        "current": "{}"
    }},)#",
            x->name(), x->description(), x->defaultVal().toString(), x->value().toString());
    }

    if (auto x = dc<CGradientValue*>(v.get()); x) {
        return std::format(
            R"#(
    {{
        "name": "{}",
        "description": "{}",
        "default": "{}",
        "current": "{}"
    }},)#",
            x->name(), x->description(), x->defaultVal().toString(), x->value().toString());
    }

    if (auto x = dc<CStringValue*>(v.get()); x) {
        return std::format(
            R"#(
    {{
        "name": "{}",
        "description": "{}",
        "default": "{}",
        "current": "{}"
    }},)#",
            x->name(), x->description(), x->defaultVal(), x->value());
    }

    if (auto x = dc<CVec2Value*>(v.get()); x) {
        return std::format(
            R"#(
    {{
        "name": "{}",
        "description": "{}",
        "default": [{}, {}],
        "current": [{}, {}]
    }},)#",
            x->name(), x->description(), x->defaultVal().x, x->defaultVal().y, x->value().x, x->defaultVal().y);
    }

    if (auto x = dc<CColorValue*>(v.get()); x) {
        return std::format(
            R"#(
    {{
        "name": "{}",
        "description": "{}",
        "default": "{:x}",
        "current": "{:x}"
    }},)#",
            x->name(), x->description(), x->defaultVal(), x->value());
    }

    Log::logger->log(Log::ERR, "values/jsonify: invalid value {}", v->name());
    return "{},";
}

std::string Values::getAsJson() {
    std::string json = "[\n";
    for (const auto& v : CONFIG_VALUES) {
        json += jsonify(v);
    }
    json.pop_back();
    json += "\n]";
    return json;
}
