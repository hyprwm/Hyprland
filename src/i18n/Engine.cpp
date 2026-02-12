#include "Engine.hpp"

#include <hyprutils/i18n/I18nEngine.hpp>
#include "../config/ConfigValue.hpp"

#include "i18n_registry.hpp"

using namespace I18n;
using namespace Hyprutils::I18n;

static SP<Hyprutils::I18n::CI18nEngine> huEngine;
static std::string                      localeStr;

//
SP<I18n::CI18nEngine> I18n::i18nEngine() {
    static SP<I18n::CI18nEngine> engine = makeShared<I18n::CI18nEngine>();
    return engine;
}

I18n::CI18nEngine::CI18nEngine() {
    huEngine = makeShared<Hyprutils::I18n::CI18nEngine>();
    huEngine->setFallbackLocale("en_US");
    localeStr = huEngine->getSystemLocale().locale();

    I18n::register_all(huEngine.get());
}

std::string I18n::CI18nEngine::localize(eI18nKeys key, const translationVarMap& vars) const {
    static auto CONFIG_LOCALE = CConfigValue<std::string>("general:locale");
    std::string locale        = *CONFIG_LOCALE != "" ? *CONFIG_LOCALE : localeStr;
    return huEngine->localizeEntry(locale, key, vars);
}
