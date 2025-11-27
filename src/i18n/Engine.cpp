#include "Engine.hpp"

#include <hyprutils/i18n/I18nEngine.hpp>
#include "../config/ConfigValue.hpp"

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

    // be_BY (Belarusian)
    huEngine->registerEntry("be_BY", TXT_KEY_ANR_TITLE, "Праграма не адказвае");
    huEngine->registerEntry("be_BY", TXT_KEY_ANR_CONTENT, "Праграма {title} - {class} не адказвае.\nШто хочаце з ёй зрабіць?");
    huEngine->registerEntry("be_BY", TXT_KEY_ANR_OPTION_TERMINATE, "Прымусова спыніць");
    huEngine->registerEntry("be_BY", TXT_KEY_ANR_OPTION_WAIT, "Пачакаць");
    huEngine->registerEntry("be_BY", TXT_KEY_ANR_PROP_UNKNOWN, "(невядома)");

    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Праграма <b>{app}</b> запытвае невядомы дазвол.");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Праграма <b>{app}</b> спрабуе здымаць экран.\n\nЦі хочаце дазволіць?");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Праграма <b>{app}</b> спрабуе загрузіць плагін: <b>{plugin}</b>.\n\nХочаце дазволіць?");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Выяўленая новая клавіятура: <b>{keyboard}</b>.\n\nХочаце дазволіць яе выкарыстанне?");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(невядома)");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_TITLE, "Запыт дазволу");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Падказка: вы можаце задаць пастаянныя правілы для гэтага ў файле канфігурацыі Hyprland.");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_ALLOW, "Дазволіць");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Дазволіць і запомніць");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_ALLOW_ONCE, "Дазволіць аднойчы");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_DENY, "Забараніць");
    huEngine->registerEntry("be_BY", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Невядомая праграма (Ідэнтыфікатар кліента wayland {wayland_id})");

    huEngine->registerEntry("be_BY", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Выглядае, што вашая пераменная асяроддзя XDG_CURRENT_DESKTOP зададзеная звонку, цяперашняе значэнне: {value}.\nГэта можа выклікаць праблемы, калі "
                            "гэта не зроблена наўмысна.");
    huEngine->registerEntry("be_BY", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "У вашай сістэме не ўсталяваны hyprland-guiutils, што выкарыстоўваецца для некаторых дыялогавых вокнаў. Разгледзьце ўсталёўку пакета.");
    huEngine->registerEntry("be_BY", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland не змог загрузіць {count} важны рэсурс, вінавацьце ў гэтым адказнага за зборку пакетаў для свайго дыстрыбутыва!";
        return "Hyprland не змог загрузіць {count} важных рэсурсаў, вінавацьце ў гэтым адказнага за зборку пакетаў для свайго дыстрыбутыва!";
    });
    huEngine->registerEntry("be_BY", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Макет манітораў наладжаны некарэктна. Манітор {name} накладаецца на іншы(я) манітор(ы).\nДля падрабязнасцей звярніцеся да Wiki (Старонка Monitors). "
                            "Гэта <b>абавязкова</b> створыць праблемы.");
    huEngine->registerEntry("be_BY", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Манітор {name} не змог наладзіць ніводны з запатрабаваных рэжымаў, аварыйна ўжыты рэжым {mode}.");
    huEngine->registerEntry("be_BY", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Няверна зададзены маштаб для манітора {name}: {scale}, ужываецца прапанаваны маштаб: {fixed_scale}");
    huEngine->registerEntry("be_BY", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Не атрымалася загрузіць плагін {name}: {error}");
    huEngine->registerEntry("be_BY", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Не атрымалася перазагрузіць шэйдар CM, аварыйна ўжываецца rgba/rgbx.");
    huEngine->registerEntry("be_BY", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Манітор {name}: пашыраны каляровы дыяпазон уключаны, але экран не ў рэжыме 10-біт.");

    // en_US (English)
    huEngine->registerEntry("en_US", TXT_KEY_ANR_TITLE, "Application Not Responding");
    huEngine->registerEntry("en_US", TXT_KEY_ANR_CONTENT, "An application {title} - {class} is not responding.\nWhat do you want to do with it?");
    huEngine->registerEntry("en_US", TXT_KEY_ANR_OPTION_TERMINATE, "Terminate");
    huEngine->registerEntry("en_US", TXT_KEY_ANR_OPTION_WAIT, "Wait");
    huEngine->registerEntry("en_US", TXT_KEY_ANR_PROP_UNKNOWN, "(unknown)");

    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "An application <b>{app}</b> is requesting an unknown permission.");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "An application <b>{app}</b> is trying to capture your screen.\n\nDo you want to allow it to?");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "An application <b>{app}</b> is trying to load a plugin: <b>{plugin}</b>.\n\nDo you want to allow it to?");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "A new keyboard has been detected: <b>{keyboard}</b>.\n\nDo you want to allow it to operate?");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(unknown)");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_TITLE, "Permission request");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Hint: you can set persistent rules for these in the Hyprland config file.");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_ALLOW, "Allow");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Allow and remember");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_ALLOW_ONCE, "Allow once");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_DENY, "Deny");
    huEngine->registerEntry("en_US", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Unknown application (wayland client ID {wayland_id})");

    huEngine->registerEntry(
        "en_US", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Your XDG_CURRENT_DESKTOP environment seems to be managed externally, and the current value is {value}.\nThis might cause issues unless it's intentional.");
    huEngine->registerEntry("en_US", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Your system does not have hyprland-guiutils installed. This is a runtime dependency for some dialogs. Consider installing it.");
    huEngine->registerEntry("en_US", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland failed to load {count} essential asset, blame your distro's packager for doing a bad job at packaging!";
        return "Hyprland failed to load {count} essential assets, blame your distro's packager for doing a bad job at packaging!";
    });
    huEngine->registerEntry("en_US", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Your monitor layout is set up incorrectly. Monitor {name} overlaps with other monitor(s) in the layout.\nPlease see the wiki (Monitors page) for "
                            "more. This <b>will</b> cause issues.");
    huEngine->registerEntry("en_US", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} failed to set any requested modes, falling back to mode {mode}.");
    huEngine->registerEntry("en_US", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Invalid scale passed to monitor {name}: {scale}, using suggested scale: {fixed_scale}");
    huEngine->registerEntry("en_US", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Failed to load plugin {name}: {error}");
    huEngine->registerEntry("en_US", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader reload failed, falling back to rgba/rgbx.");
    huEngine->registerEntry("en_US", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: wide color gamut is enabled but the display is not in 10-bit mode.");

    // as_IN (Assamese)
    huEngine->registerEntry("as_IN", TXT_KEY_ANR_TITLE, "এপ্লিকেচনে উত্তৰ দিয়া নাই");
    huEngine->registerEntry("as_IN", TXT_KEY_ANR_CONTENT, "এপ্লিকেচন {title} - {class}-এ উত্তৰ দিয়া নাই।\nআপুনি এয়াৰ লগত কি কৰিব বিচাৰে?");
    huEngine->registerEntry("as_IN", TXT_KEY_ANR_OPTION_TERMINATE, "সমাপ্ত কৰক");
    huEngine->registerEntry("as_IN", TXT_KEY_ANR_OPTION_WAIT, "অপেক্ষা কৰক");
    huEngine->registerEntry("as_IN", TXT_KEY_ANR_PROP_UNKNOWN, "(অজ্ঞাত)");

    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "এপ্লিকেচন <b>{app}</b>-এ এটা অজ্ঞাত অনুমতি বিচাৰিছে।");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "এটা এপ্লিকেচন <b>{app}</b>-এ আপোনাৰ স্ক্ৰীণ কেপচাৰ কৰিবলৈ চেষ্টা কৰিছে।\n\nআপুনি ইয়াক অনুমতি দিব বিচাৰেনে?");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_REQUEST_PLUGIN,
                            "এপ্লিকেচন <b>{app}</b>-এ এটা প্লাগিন লোড কৰিবলৈ চেষ্টা কৰিছে: <b>{plugin}</b>।\n\nআপুনি ইয়াক অনুমতি দিব বিচাৰেনে?");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "এটা নতুন কিবৰ্ড ধৰা পৰিছে: <b>{keyboard}</b>।\n\nআপুনি ইয়াক চলাবলৈ অনুমতি দিব বিচাৰেনে?");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(অজ্ঞাত)");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_TITLE, "অনুমতিৰ অনুৰোধ");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "ইঙ্গিত: আপুনি হাইপাৰলেণ্ড কনফিগ ফাইলত এইবোৰৰ বাবে স্থায়ী নিয়ম স্থাপন কৰিব পাৰে।");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_ALLOW, "অনুমতি দিয়ক");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "অনুমতি দি মনত ৰাখক");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_ALLOW_ONCE, "এবাৰ অনুমতি দিয়ক");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_DENY, "অস্বীকাৰ কৰক");
    huEngine->registerEntry("as_IN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "অজ্ঞাত এপ্লিকেচন (ৱেইলেণ্ড ক্লায়েণ্ট আইডি {wayland_id})");

    huEngine->registerEntry(
        "as_IN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "আপোনাৰ XDG_CURRENT_DESKTOP পৰিৱেশটো বাহ্যিকভাৱে পৰিচালিত হোৱা যেন লাগিছে, আৰু বৰ্তমানৰ মান হৈছে {value}।\nযদি ই ইচ্ছাকৃতভাৱে নহয়, তেনে হলে সমস্যাৰ সৃষ্টি হ'ব পাৰে।");
    huEngine->registerEntry("as_IN", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "আপোনাৰ চিষ্টেমত hyprland-guiutils ইনষ্টল কৰা নাই। কিছুমান ডাইলগৰ বাবে ই এটা ৰানটাইম নিৰ্ভৰশীলতা। ইয়াক ইনষ্টল কৰাৰ কথা চিন্তা কৰক।");
    huEngine->registerEntry("as_IN", TXT_KEY_NOTIF_FAILED_ASSETS,
                            "হাইপাৰলেণ্ড {count}-টা প্ৰয়োজনীয় সম্পদ লোড কৰাত অসফল হৈছে, বেয়া পেকজিং কৰাৰ বাবে আপোনাৰ ডিষ্ট্ৰ'ৰ পেকেজাৰক দোষাৰোপ কৰক!");
    huEngine->registerEntry("as_IN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "আপোনাৰ মনিটৰৰ লেআউট ভুলকৈ ছেট কৰা হৈছে। মনিটৰ {name} লেআউটত আন মনিটৰ(সমূহ)ৰ সৈতে ওপৰা-উপৰি হৈ আছে।\nঅধিক তথ্যৰ বাবে অনুগ্ৰহ কৰি ৱিকি (মনিটৰ পৃষ্ঠা) চাওক। ই "
                            "<b>সমস্যাৰ</b> সৃষ্টি কৰিব।");
    huEngine->registerEntry("as_IN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "মনিটৰ {name}-এ কোনো অনুৰোধ কৰা মোড ছেট কৰাত অসফল হৈছে, মোড {mode}-লৈ ঘূৰি আহিছে।");
    huEngine->registerEntry("as_IN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "মনিটৰ {name}: {scale}-লৈ অবৈধ মাপন দিয়া হৈছে, পৰামৰ্শ দিয়া মাপন ব্যৱহাৰ কৰা যাব: {fixed_scale}");
    huEngine->registerEntry("as_IN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "প্লাগিন {name} লোড কৰাত অসফল হৈছে: {error}");
    huEngine->registerEntry("as_IN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM শ্বেডাৰ ৰিলোড কৰাত অসফল হৈছে, rgba/rgbx-লৈ ঘূৰি আহিছে।");
    huEngine->registerEntry("as_IN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "প্ৰসাৰিত ৰঙৰ বৰ্গ সক্ষম কৰা হৈছে কিন্তু ডিচপ্লে 10-বিট মোডত নাই।");

    // de_DE (German)
    huEngine->registerEntry("de_DE", TXT_KEY_ANR_TITLE, "Anwendung Reagiert Nicht");
    huEngine->registerEntry("de_DE", TXT_KEY_ANR_CONTENT, "Eine Anwendung {title} - {class} reagiert nicht.\nWas möchten Sie damit tun?");
    huEngine->registerEntry("de_DE", TXT_KEY_ANR_OPTION_TERMINATE, "Beenden");
    huEngine->registerEntry("de_DE", TXT_KEY_ANR_OPTION_WAIT, "Warten");
    huEngine->registerEntry("de_DE", TXT_KEY_ANR_PROP_UNKNOWN, "(unbekannt)");

    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Eine Anwendung <b>{app}</b> fordert eine unbekannte Berechtigung an.");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Eine Anwendung <b>{app}</b> versucht Ihren Bildschrim aufzunehmen.\n\nMöchten Sie dies erlauben?");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Eine Anwendung <b>{app}</b> versucht ein Plugin zu laden: <b>{plugin}</b>.\n\nMöchten Sie dies erlauben?");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Eine neue Tastatur wurde erkannt: <b>{keyboard}</b>.\n\nMöchten Sie diese in Betrieb nehmen?");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(unbekannt)");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_TITLE, "Berechtigungsanfrage");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tip: Sie können dafür permanente Regeln in der Hyprland-Konfigurationsdatei festlegen.");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_ALLOW, "Erlauben");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Erlauben und merken");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_ALLOW_ONCE, "Einmal erlauben");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_DENY, "Verweigern");
    huEngine->registerEntry("de_DE", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Unbekannte Anwendung (wayland client ID {wayland_id})");

    huEngine->registerEntry("de_DE", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Ihre XDG_CURRENT_DESKTOP umgebung scheint extern gemanagt zu werden, und der aktuelle Wert ist {value}.\nDies könnte zu Problemen führen sofern es "
                            "nicht absichtlich so ist.");
    huEngine->registerEntry("de_DE", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Ihr System hat hyprland-guiutils nicht installiert. Dies ist eine Laufzeitabhängigkeit für einige Dialoge. Es ist empfohlen diese zu installieren.");
    huEngine->registerEntry("de_DE", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland konnte {count} essentielle Ressource nicht laden, geben Sie dem Packager ihrer Distribution die Schuld für ein schlechtes Package!";
        return "Hyprland konnte {count} essentielle Ressroucen nicht laden, geben Sie dem Packager ihrer Distribution die Schuld für ein schlechtes Package!";
    });
    huEngine->registerEntry(
        "de_DE", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Ihr Bildschirmlayout ist fehlerhaft aufgesetzt. Der Bildschirm {name} überlappt mit anderen Bildschirm(en) im Layout.\nBitte siehe im Wiki (Monitors Seite) für "
        "mehr Informationen. Dies <b>wird</b> zu Problemen führen.");
    huEngine->registerEntry("de_DE", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Bildschirm {name} konnte keinen der angeforderten Modi setzen fällt auf den Modus {mode} zurück.");
    huEngine->registerEntry("de_DE", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
                            "Ungültiger Skalierungsfaktor {scale} für Bildschirm {name}, es wird der empfohlene Faktor {fixed_scale} verwendet.");
    huEngine->registerEntry("de_DE", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Plugin {name} konnte nicht geladen werden: {error}");
    huEngine->registerEntry("de_DE", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader konnte nicht neu geladen werden und es wird auf rgba/rgbx zurückgefallen.");
    huEngine->registerEntry("de_DE", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Bildschirm {name}: wide color gamut ist aktiviert aber der Bildschirm ist nicht im 10-bit Modus.");

    // de_CH (Swiss German)
    huEngine->registerEntry("de_CH", TXT_KEY_ANR_TITLE, "Aawändig Reagiert Ned");
    huEngine->registerEntry("de_CH", TXT_KEY_ANR_CONTENT, "En Aawändig {title} - {class} reagiert ned.\nWas wend Sie demet mache?");
    huEngine->registerEntry("de_CH", TXT_KEY_ANR_OPTION_TERMINATE, "Beände");
    huEngine->registerEntry("de_CH", TXT_KEY_ANR_OPTION_WAIT, "Warte");
    huEngine->registerEntry("de_CH", TXT_KEY_ANR_PROP_UNKNOWN, "(onbekannt)");

    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "En Aawändig <b>{app}</b> fordert en onbekannti Berächtigong aa.");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "En Aawändig <b>{app}</b> versuecht Ehre Beldscherm uufznäh.\n\nWend Sie das erlaube?");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "En Aawändig <b>{app}</b> versuecht es Plugin z'lade: <b>{plugin}</b>.\n\nWend Sie das erlaube?");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "En neui Tastatur esch erkönne worde: <b>{keyboard}</b>.\n\nWend sie die in Betreb nä?");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(onbekannt)");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_TITLE, "Berächtigongsaafrog");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tip: Sie chönd permanenti Regle deför i ehrere Hyprland-Konfigurationsdatei festlegge.");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_ALLOW, "Erlaube");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Erlaube ond merke");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_ALLOW_ONCE, "Einisch erlaube");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_DENY, "Verweigere");
    huEngine->registerEntry("de_CH", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Onbekannti Aawändig (wayland client ID {wayland_id})");

    huEngine->registerEntry(
        "de_CH", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Ehri XDG_CURRENT_DESKTOP omgäbig schiint extern gmanagt z'wärde, ond de aktuelli Wärt esch {value}.\nDas chönnt zo Problem füehre sofärn das ned absechtlech so esch.");
    huEngine->registerEntry("de_CH", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Ehres System hed hyprland-guiutils ned installiert. Das esch en Laufziitabhängigkeit för es paar Dialog. Es werd empfohle sie z'installiere.");
    huEngine->registerEntry("de_CH", TXT_KEY_NOTIF_FAILED_ASSETS,
                            "Hyprland hed {count} essentielli Ressource ned chönne lade, gäbed Sie im Packager vo ehrere Distribution schold för es schlächts Package!");
    huEngine->registerEntry("de_CH", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Ehres Beldschermlayout esch fählerhaft uufgsetzt. De Beldscherm {name} öberlappt met andere Beldscherm(e) im Layout.\nBitte lueged sie im Wiki "
                            "(Monitors Siite) för meh Informatione. Das <b>werd</b> zo Problem füehre.");
    huEngine->registerEntry("de_CH", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "De Beldscherm {name} hed keine vode aagforderete Modi chönne setze, ond fallt uf de Modus {mode} zrogg.");
    huEngine->registerEntry("de_CH", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
                            "Ongöltige Skalierigsfaktor {scale} för de Beldscherm {name}, es werd de empfohleni Faktor {fixed_scale} verwändet.");
    huEngine->registerEntry("de_CH", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "S Plugin {name} hed ned chönne glade wärde: {error}");
    huEngine->registerEntry("de_CH", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader hed ned chönne neu glade wärde, es werd uf rgba/rgbx zrogggfalle.");
    huEngine->registerEntry("de_CH", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Beldscherm {name}: wide color gamut esch aktiviert aber de Beldscherm esch ned im 10-bit Modus.");

    // pt_BR (Brazilian Portuguese)
    huEngine->registerEntry("pt_BR", TXT_KEY_ANR_TITLE, "O aplicativo não está respondendo");
    huEngine->registerEntry("pt_BR", TXT_KEY_ANR_CONTENT, "O aplicativo {title} - {class} não está respondendo.\nO que você deseja fazer?");
    huEngine->registerEntry("pt_BR", TXT_KEY_ANR_OPTION_TERMINATE, "Encerrar");
    huEngine->registerEntry("pt_BR", TXT_KEY_ANR_OPTION_WAIT, "Esperar");
    huEngine->registerEntry("pt_BR", TXT_KEY_ANR_PROP_UNKNOWN, "(Desconhecido)");

    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "O aplicativo <b>{app}</b> está pedindo uma permissão desconhecida.");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "O aplicativo <b>{app}</b> está tentando capturar sua tela.\n\nVocê deseja permitir?");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "O aplicativo <b>{app}</b> está tentando carregar um plugin: <b>{plugin}</b>.\n\nVocê deseja permitir?");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Um novo teclado foi detectado: <b>{keyboard}</b>.\n\nVocê deseja permitir seu uso?");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(Desconhecido)");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_TITLE, "Solicitação de permissão");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_PERSISTENCE_HINT,
                            "Dica: você pode definir regras persistentes para essas permissões no arquivo de configuração do Hyprland");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_ALLOW, "Permitir");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Permitir e lembrar");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_ALLOW_ONCE, "Permitir uma vez");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_DENY, "Negar");
    huEngine->registerEntry("pt_BR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Aplicativo desconhecido (wayland client ID {wayland_id})");

    huEngine->registerEntry("pt_BR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Seu XDG_CURRENT_DESKTOP parece estar sendo gerenciado externamente, e atualmente é {value}.\nIsso pode causar problemas caso não seja intencional.");
    huEngine->registerEntry("pt_BR", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Seu sistema não possui hyprland-guiutils instalado. Essa é uma dependência de execução para alguns diálogos. Considere instalá-lo.");
    huEngine->registerEntry("pt_BR", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "O Hyprland falhou ao carregar {count} recurso essencial, culpe o empacotador da sua distro por fazer um péssimo trabalho!";
        return "O Hyprland falhou ao carregar {count} recursos essenciais, culpe o empacotador da sua distro por fazer um péssimo trabalho!";
    });
    huEngine->registerEntry("pt_BR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Sua disposição de monitores está configurada incorretamente. O monitor {name} se sobrepõe a outro(s) monitor(es) na disposição.\nPor favor consulte "
                            "a wiki (Monitors page) para "
                            "mais informações. Isso <b>vai</b> causar problemas.");
    huEngine->registerEntry("pt_BR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "O monitor {name} falhou em definir qualquer um dos modos solicitados, voltando ao modo {mode}.");
    huEngine->registerEntry("pt_BR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
                            "Um fator de escala inválido foi passado para o monitor {name}: {scale}, usando o fator sugerido: {fixed_scale}");
    huEngine->registerEntry("pt_BR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Falha ao carregar o plugin {name}: {error}");
    huEngine->registerEntry("pt_BR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Falha ao carregar o shader CM, voltando para rgba/rgbx.");
    huEngine->registerEntry("pt_BR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: o modo de gama de cores amplo está ativado, mas a tela não está configurada para 10 bits.");

    // es (Spanish)
    huEngine->registerEntry("es", TXT_KEY_ANR_TITLE, "La aplicación no responde");
    huEngine->registerEntry("es", TXT_KEY_ANR_CONTENT, "La aplicación {title} - {class} no responde.\n¿Qué deseas hacer?");
    huEngine->registerEntry("es", TXT_KEY_ANR_OPTION_TERMINATE, "Forzar cierre");
    huEngine->registerEntry("es", TXT_KEY_ANR_OPTION_WAIT, "Esperar");
    huEngine->registerEntry("es", TXT_KEY_ANR_PROP_UNKNOWN, "(desconocido)");

    huEngine->registerEntry("es", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Una aplicación <b>{app}</b> está solicitando un permiso desconocido.");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Una aplicación <b>{app}</b> está intentando capturar la pantalla.\n\n¿Deseas permitirlo?");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Una aplicación <b>{app}</b> está intentando cargar un plugin: <b>{plugin}</b>.\n\n¿Deseas permitirlo?");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Se ha detectado un nuevo teclado: <b>{keyboard}</b>.\n\n¿Deseas permitir su uso?");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(desconocido)");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_TITLE, "Solicitud de permiso");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_PERSISTENCE_HINT,
                            "Sugerencia: puedes establecer reglas persistentes para estos permisos en el archivo de configuración de Hyprland.");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_ALLOW, "Permitir");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Permitir y recordar");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_ALLOW_ONCE, "Permitir una vez");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_DENY, "Denegar");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Aplicación desconocida (ID de cliente de Wayland: {wayland_id})");

    huEngine->registerEntry(
        "es", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "La variable de entorno XDG_CURRENT_DESKTOP parece gestionarse externamente; su valor actual es {value}.\nEsto podría causar problemas a menos que sea intencional.");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Tu sistema no tiene instalado 'hyprland-guiutils'. Es una dependencia en tiempo de ejecución para algunos diálogos. Considera instalarlo.");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "No se pudo cargar {count} recurso esencial. Contacta al empaquetador de tu distribución.";
        return "No se pudieron cargar {count} recursos esenciales. Contacta al empaquetador de tu distribución.";
    });
    huEngine->registerEntry("es", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "La configuración de tus monitores no es correcta. El monitor {name} se superpone con otros monitores en la disposición. Consulta la wiki (página "
                            "Monitors, en inglés) para más información. Esto <b>provocará</b> problemas.");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "El monitor {name} no pudo configurar ninguno de los modos solicitados y ha vuelto al modo {mode}.");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Se pasó una escala no válida al monitor {name}: {scale}; se usará la escala sugerida: {fixed_scale}");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Error al cargar el plugin {name}: {error}");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Error al recargar el shader CM; volviendo a rgba/rgbx.");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: la gama de color amplia está habilitada, pero la pantalla no está en modo de 10 bits.");

    // fa_IR (Persian)
    huEngine->registerEntry("fa_IR", TXT_KEY_ANR_TITLE, "برنامه پاسخ نمی‌دهد");
    huEngine->registerEntry("fa_IR", TXT_KEY_ANR_CONTENT, "برنامه {title} - {class} پاسخی نمی‌دهد.\nمی‌خواهید چه کاری انجام شود؟");
    huEngine->registerEntry("fa_IR", TXT_KEY_ANR_OPTION_TERMINATE, "بستن برنامه");
    huEngine->registerEntry("fa_IR", TXT_KEY_ANR_OPTION_WAIT, "صبر کنید");
    huEngine->registerEntry("fa_IR", TXT_KEY_ANR_PROP_UNKNOWN, "(نامشخص)");

    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "برنامه <b>{app}</b> در حال درخواست یک مجوز ناشناخته است.");

    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY,
                            "برنامه <b>{app}</b> می‌خواهد صفحه‌نمایش شما را ضبط کند.\n\nآیا اجازه می‌دهید؟");

    huEngine->registerEntry(
        "fa_IR", TXT_KEY_PERMISSION_REQUEST_PLUGIN,
        "برنامه <b>{app}</b> می‌خواهد پلاگین <b>{plugin}</b> را بارگذاری کند.\n\nآیا اجازه می‌دهید پلاگین بارگذاری "
        "شود؟");

    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD,
                            "یک کیبورد جدید شناسایی شد: <b>{keyboard}</b>.\n\nآیا اجازه استفاده از آن را صادر می‌کنید؟");

    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(نامشخص)");
    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_TITLE, "درخواست مجوز");

    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_PERSISTENCE_HINT,
                            "نکته: می‌توانید قوانین دائمی مرتبط را در فایل تنظیمات هایپرلند تعریف کنید.");

    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_ALLOW, "اجازه");
    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "اجازه و ذخیره");
    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_ALLOW_ONCE, "اجازه یک‌بار");
    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_DENY, "عدم اجازه");

    huEngine->registerEntry("fa_IR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "برنامه ناشناخته (شناسه Wayland: {wayland_id})");

    huEngine->registerEntry("fa_IR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "متغیر XDG_CURRENT_DESKTOP توسط محیطی خارجی تنظیم شده است و مقدار فعلی آن {value} است.\n"
                            "اگر این کار عمدی نباشد ممکن است باعث ایجاد مشکل شود.");

    huEngine->registerEntry(
        "fa_IR", TXT_KEY_NOTIF_NO_GUIUTILS,
        "بستهٔ hyprland-guiutils در سیستم نصب نیست. این بسته برای برخی از پنجره‌ها و دیالوگ‌ها لازم است. نصب "
        "آن "
        "پیشنهاد "
        "می‌شود.");

    huEngine->registerEntry("fa_IR", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "هایپرلند نتوانست یک فایل ضروری را بارگذاری کند؛ ممکن است بسته‌بندی توزیع مشکل داشته "
                   "باشد.";
        return "هایپرلند نتوانست {count} فایل ضروری را بارگذاری کند؛ ممکن است بسته‌بندی توزیع مشکل داشته "
               "باشد.";
    });

    huEngine->registerEntry("fa_IR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "چیدمان مانیتورها صحیح نیست. مانیتور {name} با یک یا چند مانیتور دیگر تداخل دارد.\n"
                            "برای اطلاعات بیشتر به صفحهٔ مانیتورها در ویکی مراجعه کنید. این موضوع <b>حتماً</b> باعث مشکل "
                            "می‌شود.");

    huEngine->registerEntry(
        "fa_IR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL,
        "مانیتور {name} نتوانست هیچ‌کدام از حالت‌های درخواستی را اعمال کند؛ بازگشت به حالت {mode}.");

    huEngine->registerEntry("fa_IR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "مقیاس واردشده برای مانیتور {name} نامعتبر است: {scale}. مقیاس پیشنهادی اعمال شد: {fixed_scale}");

    huEngine->registerEntry("fa_IR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "بارگذاری پلاگین {name} با خطا روبه‌رو شد: {error}");

    huEngine->registerEntry("fa_IR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "بارگذاری دوبارهٔ شیدر CM ناموفق بود؛ از حالت rgba/rgbx استفاده شد.");

    huEngine->registerEntry("fa_IR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "مانیتور {name}: گسترهٔ رنگ وسیع فعال است اما نمایشگر در حالت ۱۰ بیتی نیست.");

    // fr_FR (French)
    huEngine->registerEntry("fr_FR", TXT_KEY_ANR_TITLE, "L'application ne répond plus");
    huEngine->registerEntry("fr_FR", TXT_KEY_ANR_CONTENT, "L'application {title} - {class} ne répond plus.\nQue voulez-vous faire?");
    huEngine->registerEntry("fr_FR", TXT_KEY_ANR_OPTION_TERMINATE, "Forcer l'arrêt");
    huEngine->registerEntry("fr_FR", TXT_KEY_ANR_OPTION_WAIT, "Attendre");
    huEngine->registerEntry("fr_FR", TXT_KEY_ANR_PROP_UNKNOWN, "(inconnu)");

    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Une application <b>{app}</b> demande une autorisation inconnue.");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Une application <b>{app}</b> tente de capturer votre écran.\n\nVoulez-vous l'y autoriser?");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Une application <b>{app}</b> tente de charger un module : <b>{plugin}</b>.\n\nVoulez-vous l'y autoriser?");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Un nouveau clavier a été détecté : <b>{keyboard}</b>.\n\nVouslez-vous l'autoriser à fonctioner?");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(inconnu)");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_TITLE, "Demande d'autorisation");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Astuce: vous pouvez définir des règles persistantes dans le fichier de configuration de Hyprland.");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_ALLOW, "Autoriser");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Autoriser et mémoriser");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_ALLOW_ONCE, "Autoriser une fois");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_DENY, "Refuser");
    huEngine->registerEntry("fr_FR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Application inconnue (ID client wayland {wayland_id})");

    huEngine->registerEntry("fr_FR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Votre variable d'environnement XDG_CURRENT_DESKTOP semble être gérée de manière externe, et sa valeur actuelle est {value}.\nCela peut provoquer des "
                            "problèmes si ce n'est pas intentionnel.");
    huEngine->registerEntry("fr_FR", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Vous système n'a pas hyprland-guiutils installé. C'est une dépendance d'éxécution pour certains dialogues. Envisagez de l'installer.");
    huEngine->registerEntry("fr_FR", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland n'a pas pu charger {count} ressource essentielle, cela indique très probablement un problème dans le paquet de votre distribution.";
        return "Hyprland n'a pas pu charger {count} ressources essentielles, cela indique très probablement un problème dans le paquet de votre distribution.";
    });
    huEngine->registerEntry("fr_FR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Votre disposition d'écrans est incorrecte. Le moniteur {name} chevauche un ou plusieurs autres.\nVeuillez consulter le wiki (page Moniteurs) pour"
                            "en savoir plus. Cela <b>causera</> des problèmes.");
    huEngine->registerEntry("fr_FR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Le moniteur {name} n'a pu appliquer aucun des modes demandés, retour au mode {mode}.");
    huEngine->registerEntry("fr_FR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Échelle invalide pour le moniteur {name}: {scale}. Utilisation de l'échelle suggérée: {fixed_scale}.");
    huEngine->registerEntry("fr_FR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Échec du chargement du module {name} : {error}");
    huEngine->registerEntry("fr_FR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Le rechargement du shader CM a échoué, retour aux formats rgba/rgbx");
    huEngine->registerEntry("fr_FR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Moniteur {name} : l'espace colorimétrique étendu est activé, mais l'écran n'est pas en mode 10-bits.");

    // hi_IN (Hindi)
    huEngine->registerEntry("hi_IN", TXT_KEY_ANR_TITLE, "एप्लिकेशन प्रतिक्रिया नहीं दे रहा है");
    huEngine->registerEntry("hi_IN", TXT_KEY_ANR_CONTENT,
                            "एक एप्लिकेशन {title} - {class} प्रतिक्रिया नहीं दे रहा "
                            "है।\nआप इसके साथ क्या करना चाहेंगे?");
    huEngine->registerEntry("hi_IN", TXT_KEY_ANR_OPTION_TERMINATE, "समाप्त करें");
    huEngine->registerEntry("hi_IN", TXT_KEY_ANR_OPTION_WAIT, "इंतजार करें");
    huEngine->registerEntry("hi_IN", TXT_KEY_ANR_PROP_UNKNOWN, "(अज्ञात)");

    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "एक एप्लिकेशन <b>{app}</b> एक अज्ञात अनुमति का अनुरोध कर रहा है।");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY,
                            "एक एप्लिकेशन <b>{app}</b> आपकी स्क्रीन कैप्चर करने की "
                            "कोशिश कर रहा है।\n\nक्या आप इसे अनुमति देना चाहते हैं?");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_REQUEST_PLUGIN,
                            "एक एप्लिकेशन <b>{app}</b> एक प्लगइन लोड करने की कोशिश कर रहा है: "
                            "<b>{plugin}</b>.\n\nक्या आप इसे अनुमति देना चाहते हैं?");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD,
                            "नया कीबोर्ड पाया गया: <b>{keyboard}</b>.\n\nक्या आप "
                            "इसे काम करने की अनुमति देना चाहते हैं?");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(अज्ञात)");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_TITLE, "अनुमति अनुरोध");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "संकेत: आप Hyprland कॉन्फ़िग फ़ाइल में इनके लिए स्थायी नियम सेट कर सकते हैं।");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_ALLOW, "अनुमति दें");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "अनुमति दें और याद रखें");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_ALLOW_ONCE, "एक बार अनुमति दें");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_DENY, "अस्वीकार करें");
    huEngine->registerEntry("hi_IN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "अज्ञात एप्लिकेशन (wayland क्लाइंट ID {wayland_id})");

    huEngine->registerEntry("hi_IN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "आपका XDG_CURRENT_DESKTOP परिवेश बाहरी रूप से प्रबंधित लगता है, और वर्तमान मान "
                            "{value} है।\nयह समस्या पैदा कर सकता "
                            "है जब तक कि यह जानबूझकर न किया गया हो।");
    huEngine->registerEntry("hi_IN", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "आपके सिस्टम में hyprland-guiutils इंस्टॉल नहीं है। यह कुछ संवादों के लिए एक रनटाइम "
                            "निर्भरता है। इसे इंस्टॉल करने पर विचार करें।");
    huEngine->registerEntry("hi_IN", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland {count} आवश्यक संसाधन लोड करने में विफल रहा, अपने डिस्ट्रो "
                   "के पैकेजर को पैकेजिंग में खराब काम करने का दोष दें!";
        return "Hyprland {count} आवश्यक संसाधनों को लोड करने में विफल रहा, अपने "
               "डिस्ट्रो के पैकेजर को पैकेजिंग में खराब काम करने का दोष दें!";
    });
    huEngine->registerEntry("hi_IN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "आपका मॉनिटर लेआउट गलत तरीके से सेट है। मॉनिटर {name} लेआउट में अन्य मॉनिटर(ओं) के "
                            "साथ ओवरलैप कर रहा है।\nकृपया विकि "
                            " (Monitors पेज) देखें। यह <b>समस्याएँ</b> पैदा करेगा।");
    huEngine->registerEntry("hi_IN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL,
                            "मॉनिटर {name} ने किसी भी अनुरोधित मोड को सेट करने में "
                            "विफल रहा, मोड {mode} पर वापस जा रहा है।");
    huEngine->registerEntry("hi_IN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
                            "मॉनिटर {name} को अवैध स्केल दिया गया: {scale}, सुझाया "
                            "गया स्केल इस्तेमाल किया जा रहा है: {fixed_scale}");
    huEngine->registerEntry("hi_IN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "प्लगइन {name} लोड करने में विफल: {error}");
    huEngine->registerEntry("hi_IN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM शेडर रीलोड विफल हुआ, rgba/rgbx पर वापस जा रहा है।");
    huEngine->registerEntry("hi_IN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "मॉनिटर {name}: वाइड कलर गैम सक्षम है लेकिन डिस्प्ले 10-बिट मोड में नहीं है।");

    // hr_HR (Croatian)
    huEngine->registerEntry("hr_HR", TXT_KEY_ANR_TITLE, "Aplikacija ne reagira");
    huEngine->registerEntry("hr_HR", TXT_KEY_ANR_CONTENT, "Aplikacija {title} - {class} ne reagira.\nŠto želiš napraviti s njom?");
    huEngine->registerEntry("hr_HR", TXT_KEY_ANR_OPTION_TERMINATE, "Zaustavi");
    huEngine->registerEntry("hr_HR", TXT_KEY_ANR_OPTION_WAIT, "Pričekaj");
    huEngine->registerEntry("hr_HR", TXT_KEY_ANR_PROP_UNKNOWN, "(nepoznato)");

    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Aplikacija <b>{app}</b> zahtijeva nepoznatu dozvolu.");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Aplikacija <b>{app}</b> pokušava snimati vaš zaslon.\n\nŽeliš li dopustiti?");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Aplikacija <b>{app}</b> pokušava učitati dodatak: <b>{plugin}</b>.\n\nŽeliš li dopustiti?");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Otkrivena je nova tipkovnica: <b>{keyboard}</b>.\n\nŽeliš li omogućiti njen rad?");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(nepoznato)");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_TITLE, "Zahtjev za dozvolu");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Savjet: za ovo možeš postaviti trajna pravila u Hyprland konfiguracijskoj datoteci.");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_ALLOW, "Dozvoli");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Dozvoli i zapamti");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_ALLOW_ONCE, "Dozvoli samo ovaj put");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_DENY, "Uskrati");
    huEngine->registerEntry("hr_HR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Nepoznata aplikacija (ID wayland klijenta {wayland_id})");

    huEngine->registerEntry(
        "hr_HR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Izgleda da je tvoja XDG_CURRENT_DESKTOP okolina vanjski upravljana te je trenutna vrijednost {value}.\nOvo može izazvati problem, osim ako je namjerno.");
    huEngine->registerEntry("hr_HR", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Na tvojem sustavu nije instaliran hyprland-guiutils. Ovo je ovisnost tijekom pokretanja nekih dijaloga. Preporučeno je da je instaliraš.");
    huEngine->registerEntry("hr_HR", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo % 10 <= 1 && assetsNo % 100 != 11)
            return "Hyprland nije uspio učitati {count} neophodnu komponentu, krivi pakera svoje distribucije za loš posao pakiranja!";
        else if (assetsNo % 10 <= 4 && assetsNo % 100 > 14)
            return "Hyprland nije uspio učitati {count} neophodne komponente, krivi pakera svoje distribucije za loš posao pakiranja!";
        return "Hyprland nije uspio učitati {count} neophodnih komponenata, krivi pakera svoje distribucije za loš posao pakiranja!";
    });
    huEngine->registerEntry("hr_HR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Raspored tvojih monitora je krivo postavljen. Monitor {name} preklapa se s ostalim monitorom/ima u rasporedu.\nProvjeri wiki (Monitors stranicu) za "
                            "više informacija. Ovo <b>hoće</b> izazvati probleme.");
    huEngine->registerEntry("hr_HR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} nije uspio odrediti zatražene načine rada, povratak na zadani način rada: {mode}.");
    huEngine->registerEntry("hr_HR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Nevažeći razmjer proslijeđen monitoru {name}: {scale}, koristi se predloženi razmjer: {fixed_scale}");
    huEngine->registerEntry("hr_HR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Učitavanje dodatka {name} nije uspjelo: {error}");
    huEngine->registerEntry("hr_HR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Ponovno učitavanje CM shadera nije uspjelo, povratak na zadano: rgba/rgbx.");
    huEngine->registerEntry("hr_HR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: široki raspon boja je omogućen, ali ekran nije u 10-bitnom načinu rada.");

    // it_IT (Italian)
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_TITLE, "L'applicazione non risponde");
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_CONTENT, "Un'applicazione {title} - {class} non risponde.\nCosa vuoi fare?");
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_OPTION_TERMINATE, "Termina");
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_OPTION_WAIT, "Attendi");
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_PROP_UNKNOWN, "(sconosciuto)");

    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Un'applicazione <b>{app}</b> richiede un'autorizzazione sconosciuta.");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Un'applicazione <b>{app}</b> sta provando a catturare il tuo schermo.\n\nVuoi permetterglielo?");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_PLUGIN,
                            "Un'applicazione <b>{app}</b> sta provando a caricare un plugin: <b>{plugin}</b>.\n\nVuoi permetterglielo?");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "È stata rilevata una nuova tastiera: <b>{keyboard}</b>.\n\nLe vuoi permettere di operare?");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(sconosciuto)");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_TITLE, "Richiesta di autorizzazione");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Consiglio: Puoi impostare una regola persistente nel tuo file di configurazione di Hyprland.");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_ALLOW, "Permetti");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Permetti e ricorda");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_ALLOW_ONCE, "Permetti una volta");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_DENY, "Nega");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Applicazione sconosciuta (wayland client ID {wayland_id})");

    huEngine->registerEntry("it_IT", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "L'ambiente XDG_CURRENT_DESKTOP sembra essere gestito esternamente, il valore attuale è {value}.\nSe non è voluto, potrebbe causare problemi.");
    huEngine->registerEntry("it_IT", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Sembra che hyprland-guiutils non sia installato. È una dipendenza richiesta per alcuni dialoghi che potresti voler installare.");
    huEngine->registerEntry("it_IT", TXT_KEY_NOTIF_FAILED_ASSETS,
                            "Hyprland non ha potuto caricare {count} asset, dai la colpa al packager della tua distribuzione per il suo cattivo lavoro!");
    huEngine->registerEntry("it_IT", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "I tuoi schermi sono configurati incorrettamente. Lo schermo {name} si sovrappone con altri nel layout.\nConsulta la wiki (voce Schermi) per "
                            "altre informazioni. Questo <b>causerà</b> problemi.");
    huEngine->registerEntry("it_IT", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Lo schermo {name} non ha potuto impostare alcuna modalità richiesta, sarà usata la modalità {mode}.");
    huEngine->registerEntry("it_IT", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
                            "Fattore di scala non valido per lo schermo {name}: {scale}, utilizzando il fattore suggerito: {fixed_scale}");
    huEngine->registerEntry("it_IT", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Impossibile caricare il plugin {name}: {error}");
    huEngine->registerEntry("it_IT", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Impossibile ricaricare gli shader CM, sarà usato rgba/rgbx.");
    huEngine->registerEntry("it_IT", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Schermo {name}: la gamma di colori ampia è abilitata ma lo schermo non è in modalità 10-bit.");

    // ja_JP (Japanese)
    huEngine->registerEntry("ja_JP", TXT_KEY_ANR_TITLE, "アプリは応答しません");
    huEngine->registerEntry("ja_JP", TXT_KEY_ANR_CONTENT, "アプリ {title} ー {class}は応答しません。\n何をしたいですか？");
    huEngine->registerEntry("ja_JP", TXT_KEY_ANR_OPTION_TERMINATE, "強制終了");
    huEngine->registerEntry("ja_JP", TXT_KEY_ANR_OPTION_WAIT, "待機");
    huEngine->registerEntry("ja_JP", TXT_KEY_ANR_PROP_UNKNOWN, "（不明）");

    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "アプリ<b>{app}</b>は不明な許可を要求します。");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "アプリ<b>{app}</b>は画面へのアクセスを要求します。\n\n許可したいですか？");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "アプリ<b>{app}</b>は以下のプラグインをロード許可を要求します：<b>{plugin}</b>。\n\n許可したいですか？");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "新しいキーボードを見つけた：<b>{keyboard}</b>。\n\n稼働を許可したいですか？");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_UNKNOWN_NAME, "（不明）");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_TITLE, "許可要求");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "ヒント：Hyprlandのコンフィグで通常の許可や却下を設定できます。");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_ALLOW, "許可");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "保存して許可");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_ALLOW_ONCE, "一度許可");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_DENY, "却下");
    huEngine->registerEntry("ja_JP", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "不明なアプリ (waylandクライアントID {wayland_id})");

    huEngine->registerEntry("ja_JP", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "エンバイアロンメント変数「XDG_CURRENT_DESKTOP」は外部から「{value}」に設定しました。\n意図的ではなければ、問題は発生可能性があります。");
    huEngine->registerEntry("ja_JP", TXT_KEY_NOTIF_NO_GUIUTILS, "システムにhyprland-guiutilsはインストールしていません。このパッケージをインストールしてください。");
    huEngine->registerEntry("ja_JP", TXT_KEY_NOTIF_FAILED_ASSETS,
                            "{count}つの根本的なアセットをロードできませんでした。これはパッケージャーのせいだから、パッケージャーに文句してください。");
    huEngine->registerEntry(
        "ja_JP", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "画面の位置設定は誤用です。画面{name}は他の画面の区域と重ね合わせます。\nウィキのモニターページで詳細を確認してください。これは<b>絶対に</b>問題になります。");
    huEngine->registerEntry("ja_JP", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "画面{name}は設定したモードを正常に受け入れませんでした。{mode}を使いました。");
    huEngine->registerEntry("ja_JP", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "画面{name}のスケールは無効：{scale}、代わりにおすすめのスケール{fixed_scale}を使いました。");
    huEngine->registerEntry("ja_JP", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "プラグイン{name}のロード失敗: {error}");
    huEngine->registerEntry("ja_JP", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CMシェーダーのリロード失敗、rgba/rgbxを使いました。");
    huEngine->registerEntry("ja_JP", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "画面{name}：広い色域は設定していますけど、画面は10ビットモードに設定されていません。");

    // lv_LV (Latvian)
    huEngine->registerEntry("lv_LV", TXT_KEY_ANR_TITLE, "Lietotne nereaģē");
    huEngine->registerEntry("lv_LV", TXT_KEY_ANR_CONTENT, "Lietotne {title} - {class} nereaģē.\nKo jūs vēlaties darīt?");
    huEngine->registerEntry("lv_LV", TXT_KEY_ANR_OPTION_TERMINATE, "Beigt procesu");
    huEngine->registerEntry("lv_LV", TXT_KEY_ANR_OPTION_WAIT, "Gaidīt");
    huEngine->registerEntry("lv_LV", TXT_KEY_ANR_PROP_UNKNOWN, "(nezināms)");

    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Lietotne <b>{app}</b> pieprasa nezināmu atļauju.");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Lietotne <b>{app}</b> mēģina lasīt no jūsu ekrāna.\n\nVai vēlaties to atļaut?");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Lietotne <b>{app}</b> mēģina ielādēt spraudni: <b>{plugin}</b>.\n\nVai vēlaties to atļaut?");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Ir atrasta jauna tastatūra: <b>{keyboard}</b>.\n\nVai vēlaties atļaut tās darbību?");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(nezināms)");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_TITLE, "Atļaujas pieprasījums");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Padoms: Hyprland konfigurācijas failā varat arī iestatīt atļaujas.");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_ALLOW, "Atļaut");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Atļaut un atcerēties");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_ALLOW_ONCE, "Atļaut vienreiz");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_DENY, "Aizliegt");
    huEngine->registerEntry("lv_LV", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Nezināma lietotne (Wayland klienta ID {wayland_id})");

    huEngine->registerEntry("lv_LV", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Jūsu XDG_CURRENT_DESKTOP tiek ārēji pārvaldīts, tās vērtība ir {value}.\nTas var neapzināti izraisīt problēmas.");
    huEngine->registerEntry("lv_LV", TXT_KEY_NOTIF_NO_GUIUTILS, "Jums nav instalēts hyprland-guiutils. Šī pakotne ir nepieciešama dažiem dialogiem. Apsveriet tās instalēšanu.");
    huEngine->registerEntry("lv_LV", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland nevarēja ielādēt {count} būtisku resursu, vainojiet sava distro iepakotāju par sliktu iepakošanu!";
        return "Hyprland nevarēja ielādēt {count} būtiskus resursus, vainojiet sava distro iepakotāju par sliktu iepakošanu!";
    });
    huEngine->registerEntry(
        "lv_LV", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Jūsu monitora izkārtojums ir nepareizi iestatīts. Monitors {name} pārklājas ar citiem izkārtojumā iestatītajiem monitoriem.\nLūdzu apskatieties (Monitoru lapā),"
        "lai uzzinātu vairāk. Tas <b>radīs</b> problēmas.");
    huEngine->registerEntry("lv_LV", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitoram {name} neizdevās iestatīt nevienu no pieprasītajiem režīmiem, izmantojam {mode}.");
    huEngine->registerEntry("lv_LV", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Monitoram {name} ir nodots nederīgs mērogs: {scale}, izmantojam ieteikto mērogu: {fixed_scale}");
    huEngine->registerEntry("lv_LV", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Nevarēja ielādēt spraudni {name}: {error}");
    huEngine->registerEntry("lv_LV", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM šeiderus neizdevās pārlādēt, izmantojam rgba/rgbx.");
    huEngine->registerEntry("lv_LV", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitors {name}: Ir iespējota plaša krāsu gamma, bet displejs nav 10-bitu režīmā.");

    // hu_HU (Hungarian)
    huEngine->registerEntry("hu_HU", TXT_KEY_ANR_TITLE, "Az alkalmazás nem válaszol");
    huEngine->registerEntry("hu_HU", TXT_KEY_ANR_CONTENT, "A(z) {title} - {class} alkalmazás nem válaszol.\nMit szeretne tenni vele?");
    huEngine->registerEntry("hu_HU", TXT_KEY_ANR_OPTION_TERMINATE, "Leállítás");
    huEngine->registerEntry("hu_HU", TXT_KEY_ANR_OPTION_WAIT, "Várakozás");
    huEngine->registerEntry("hu_HU", TXT_KEY_ANR_PROP_UNKNOWN, "(ismeretlen)");

    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "A(z) <b>{app}</b> alkalmazás ismeretlen engedélyt kér.");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "A(z) <b>{app}</b> alkalmazás megpróbálja rögzíteni a képernyőjét.\n\nEngedélyezi?");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "A(z) <b>{app}</b> alkalmazás megpróbál egy bővítményt betölteni: <b>{plugin}</b>.\n\nEngedélyezi?");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Új billentyűzetet észleltünk: <b>{keyboard}</b>.\n\nEngedélyezi a használatát?");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(ismeretlen)");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_TITLE, "Engedélykérés");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tipp: Állandó szabályokat állíthat be a Hyprland konfigurációs fájlban.");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_ALLOW, "Engedélyezés");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Mindig engedélyez");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_ALLOW_ONCE, "Egyszeri engedélyezés");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_DENY, "Elutasítás");
    huEngine->registerEntry("hu_HU", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Ismeretlen alkalmazás (wayland kliens ID {wayland_id})");

    huEngine->registerEntry("hu_HU", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Úgy tűnik, hogy az XDG_CURRENT_DESKTOP környezetet külsőleg kezelik, és a jelenlegi érték {value}.\nEz problémákat okozhat, hacsak nem szándékos.");
    huEngine->registerEntry("hu_HU", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "A rendszerében nincs telepítve a hyprland-guiutils. Ez egy futásidejű függőség néhány párbeszédablakhoz. Fontolja meg a telepítését.");
    huEngine->registerEntry("hu_HU", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "A Hyprland nem tudta betölteni az 1 szükséges erőforrást. Kérjük, jelezze a hibát a disztribúció csomagolójának.";
        return "A Hyprland nem tudott betölteni {count} szükséges erőforrást. Kérjük, jelezze a hibát a disztribúció csomagolójának.";
    });
    huEngine->registerEntry(
        "hu_HU", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "A monitor elrendezése helytelenül van beállítva. A(z) {name} monitor átfedi a többi monitort az elrendezésben.\nKérjük, további információkért tekintse meg a wikit "
        "(Monitors oldal). Ez <b>problémákat</b> fog okozni.");
    huEngine->registerEntry("hu_HU", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "A(z) {name} monitor nem tudta beállítani a kért módokat, visszaáll a(z) {mode} módra.");
    huEngine->registerEntry("hu_HU", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Érvénytelen skálázás a(z) {name} monitorhoz: {scale}, a javasolt skálázás használata: {fixed_scale}");
    huEngine->registerEntry("hu_HU", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Nem sikerült betölteni a(z) {name} bővítményt: {error}");
    huEngine->registerEntry("hu_HU", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "A CM shader újratöltése sikertelen, visszaáll rgba/rgbx-re.");
    huEngine->registerEntry("hu_HU", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: A széles színtartomány engedélyezve van, de a kijelző nem 10 bites módban van.");

    // ml_IN (Malayalam)
    huEngine->registerEntry("ml_IN", TXT_KEY_ANR_TITLE, "ആപ്ലിക്കേഷൻ പ്രതികരിക്കുന്നില്ല");
    huEngine->registerEntry("ml_IN", TXT_KEY_ANR_CONTENT, "ആപ്ലിക്കേഷൻ {title} - {class} പ്രതികരിക്കുന്നില്ല.\nഇതിന് നിങ്ങൾ എന്ത് ചെയ്യാൻ ആഗ്രഹിക്കുന്നു?");
    huEngine->registerEntry("ml_IN", TXT_KEY_ANR_OPTION_TERMINATE, "അവസാനിപ്പിക്കുക");
    huEngine->registerEntry("ml_IN", TXT_KEY_ANR_OPTION_WAIT, "കാത്തിരിക്കുക");
    huEngine->registerEntry("ml_IN", TXT_KEY_ANR_PROP_UNKNOWN, "(അജ്ഞാതം)");

    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "ആപ്ലിക്കേഷൻ <b>{app}</b> ഒരു അജ്ഞാത അനുമതി അഭ്യർത്ഥിക്കുന്നു.");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "ആപ്ലിക്കേഷൻ <b>{app}</b> നിങ്ങളുടെ സ്ക്രീൻ പകർത്താൻ ശ്രമിക്കുന്നു.\n\nനിങ്ങൾ അത് അനുവദിക്കണോ?");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "ആപ്ലിക്കേഷൻ <b>{app}</b> ഒരു പ്ലഗിൻ ലോഡ് ചെയ്യാൻ ശ്രമിക്കുന്നു: <b>{plugin}</b>.\n\nഇത് അനുവദിക്കണോ?");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "പുതിയ കീബോർഡ് കണ്ടെത്തി: <b>{keyboard}</b>.\n\nഇത് പ്രവർത്തിക്കാൻ അനുവദിക്കണോ?");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(അജ്ഞാതം)");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_TITLE, "അനുമതി അഭ്യർത്ഥന");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "സൂചന: Hyprland കോൺഫിഗ് ഫയലിൽ സ്ഥിരനിയമങ്ങൾ സജ്ജമാക്കാം.");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_ALLOW, "അനുവദിക്കുക");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "അനുവദിച്ച് ഓർക്കുക");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_ALLOW_ONCE, "ഒന്നുതവണ അനുവദിക്കുക");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_DENY, "നിരസിക്കുക");
    huEngine->registerEntry("ml_IN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "അജ്ഞാത അപ്ലിക്കേഷൻ (wayland client ID {wayland_id})");

    huEngine->registerEntry("ml_IN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "നിങ്ങളുടെ XDG_CURRENT_DESKTOP പരിസ്ഥിതി പുറത്ത് നിന്ന് നിയന്ത്രിക്കപ്പെടുന്നു, ഇപ്പോഴത്തെ മൂല്യം "
                            "{value}.\nഇത് ഉദ്ദേശ്യമായല്ലെങ്കിൽ പ്രശ്നങ്ങൾ ഉണ്ടാകും.");
    huEngine->registerEntry("ml_IN", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "നിങ്ങളുടെ സിസ്റ്റത്തിൽ hyprland-guiutils ഇൻസ്റ്റാൾ ചെയ്തിട്ടില്ല. ഇത് ചില ഡയലോഗുകൾക്ക് ആവശ്യമായ "
                            "റൺടൈം ആശ്രയമാണ്. ഇൻസ്റ്റാൾ ചെയ്യുക.");
    huEngine->registerEntry("ml_IN", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland {count} പ്രധാന അസറ്റ് ലോഡുചെയ്യാൻ പരാജയപ്പെട്ടു, നിങ്ങളുടെ "
                   "ഡിസ്‌ട്രോ "
                   "പാക്കേജർ പിശക് ചെയ്തിരിക്കുന്നു!";
        return "Hyprland {count} പ്രധാന അസറ്റുകൾ ലോഡുചെയ്യാൻ പരാജയപ്പെട്ടു, നിങ്ങളുടെ "
               "ഡിസ്‌ട്രോ "
               "പാക്കേജർ പിശക് ചെയ്തിരിക്കുന്നു!";
    });
    huEngine->registerEntry("ml_IN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "മോണിറ്റർ ലേയൗട്ട് തെറ്റാണ്. മോണിറ്റർ {name} മറ്റുള്ളവയുമായ് ഒതുങ്ങുന്നു.\nകൂടുതൽ വിവരങ്ങൾക്ക് Wiki "
                            "(Monitors page) കാണുക. ഇത് <b>പ്രശ്നങ്ങൾ ഉണ്ടാക്കും</b>.");
    huEngine->registerEntry("ml_IN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "മോണിറ്റർ {name} ആവശ്യപ്പെട്ട മോഡുകൾ സജ്ജമാക്കാൻ പരാജയപ്പെട്ടു, ഇപ്പോൾ {mode} ഉപയോഗിക്കുന്നു.");
    huEngine->registerEntry("ml_IN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "മോണിറ്റർ {name} ന് അസാധുവായ സ്കെയിൽ: {scale}, നിർദ്ദേശിച്ച സ്കെയിൽ: {fixed_scale}");
    huEngine->registerEntry("ml_IN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "പ്ലഗിൻ {name} ലോഡ് ചെയ്യാൻ പരാജയപ്പെട്ടു: {error}");
    huEngine->registerEntry("ml_IN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM ഷേഡർ റീലോഡ് പരാജയപ്പെട്ടു, rgba/rgbx ലേക്ക് മാറുന്നു.");
    huEngine->registerEntry("ml_IN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "മോണിറ്റർ {name}: വൈഡ് കളർ ഗാമട്ട് പ്രവർത്തനക്ഷമമാണെങ്കിലും, മോഡ് 10-bit അല്ല.");

    // nb_NO (Norwegian Bokmål)
    huEngine->registerEntry("nb_NO", TXT_KEY_ANR_TITLE, "Applikasjonen svarer ikke");
    huEngine->registerEntry("nb_NO", TXT_KEY_ANR_CONTENT, "En applikasjon {title} - {class} svarer ikke.\nHva vil du gjøre med den?");
    huEngine->registerEntry("nb_NO", TXT_KEY_ANR_OPTION_TERMINATE, "Avslutt");
    huEngine->registerEntry("nb_NO", TXT_KEY_ANR_OPTION_WAIT, "Vent");
    huEngine->registerEntry("nb_NO", TXT_KEY_ANR_PROP_UNKNOWN, "(ukjent)");

    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "En applikasjon <b>{app}</b> ber om en ukjent tillatelse.");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "En applikasjon <b>{app}</b> prøver å fange skjermen din.\n\nVil du tillate den?");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "En applikasjon <b>{app}</b> prøver å laste en plugin: <b>{plugin}</b>.\n\nVil du tillate den?");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Et nytt tastatur er oppdaget: <b>{keyboard}</b>.\n\nVil du tillate at det opererer?");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(ukjent)");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_TITLE, "Tillatelsesforespørsel");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Hint: du kan angi vedvarende regler for disse i Hyprland konfigurasjonsfilen.");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_ALLOW, "Tillat");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Tillat og husk");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_ALLOW_ONCE, "Tillat en gang");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_DENY, "Nekte");
    huEngine->registerEntry("nb_NO", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Ukjent applikasjon (wayland client ID {wayland_id})");

    huEngine->registerEntry(
        "nb_NO", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Ditt XDG_CURRENT_DESKTOP miljø ser ut til å være eksternt administrert, og den nåværende verdien er {value}.\nDette kan forårsake problemer med mindre det er bevisst.");
    huEngine->registerEntry("nb_NO", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Ditt system har ikke hyprland-guiutils installert. Dette er en kjøretidsavhengighet for noen dialoger. Vurder å installere den.");
    huEngine->registerEntry("nb_NO", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland kunne ikke laste {count} essensiell ressurs, skyld på distroens pakkeansvarlig for å ha gjort en dårlig jobb med pakkingen!";
        return "Hyprland kunne ikke laste {count} essensielle ressurser, skyld på distroens pakkeansvarlig for å ha gjort en dårlig jobb med pakkingen!";
    });
    huEngine->registerEntry("nb_NO", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Skjermoppsettet ditt er satt opp feil. Skjerm {name} overlapper med skjerm(er) i oppsettet.\nSjekk wiki (Skjerm oppsett siden) for "
                            "mer. Dette <b>vil</b> skape problemer.");
    huEngine->registerEntry("nb_NO", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Skjerm {name} feilet å sette de forespurte modusene, faller tilbake til modus {mode}.");
    huEngine->registerEntry("nb_NO", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Ugyldig skala sendt til skjerm {name}: {scale}, bruker foreslått skala: {fixed_scale}");
    huEngine->registerEntry("nb_NO", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Feilet å laste plugin {name}: {error}");
    huEngine->registerEntry("nb_NO", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader omlading feilet, faller tilbake til rgba/rgbx.");
    huEngine->registerEntry("nb_NO", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Skjerm {name}: bredt fargespekter er aktivert, men skjermen er ikke i 10-bit modus.");

    // ne_NP (Nepali)
    huEngine->registerEntry("ne_NP", TXT_KEY_ANR_TITLE, "एपले रिस्पन्ड गरिरहेको छैन");
    huEngine->registerEntry("ne_NP", TXT_KEY_ANR_CONTENT, "{title} - {class} एपले रिस्पन्ड गरिरहेको छैन।\nयससँग के गर्न चहानुहुन्छ?");
    huEngine->registerEntry("ne_NP", TXT_KEY_ANR_OPTION_TERMINATE, "टर्मिनेट गर्नुहोस्");
    huEngine->registerEntry("ne_NP", TXT_KEY_ANR_OPTION_WAIT, "पर्खनुहोस्");
    huEngine->registerEntry("ne_NP", TXT_KEY_ANR_PROP_UNKNOWN, "(अज्ञात)");

    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "<b>{app}</b> एपले अज्ञात सुविधाको अनुमति मागिरहेको छ।");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "<b>{app}</b> एपले स्क्रिन क्याप्चर गर्न खोज्दै छ।\n\nयसलाई अनुमति दिन चहानुहुन्छ?");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "<b>{app}</b> एपले एउटा प्लगिन लोड गर्न खोज्दै छ: <b>{plugin}</b>।\n\nयसलाई अनुमति दिन चहानुहुन्छ?");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "एउटा नयाँ किबोर्ड डिटेक्ट गरिएको छ: <b>{keyboard}</b>।\n\nयसलाई चल्ने अनुमति दिन चहानुहुन्छ?");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(अज्ञात)");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_TITLE, "अनुमतिको माग");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "टिप: यसको लागि पर्सिस्टेन्ट नियमहरु तपाइँले हाइपरल्यान्डको कन्फीग्युरेसन फाइलमा राख्न सक्नुहुन्छ।");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_ALLOW, "अनुमति दिनुहोस्");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "अनुमति दिनुहोस् र सम्झनुहोस्");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_ALLOW_ONCE, "एकपटक अनुमति दिनुहोस्");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_DENY, "अनुमति नदिनुहोस्");
    huEngine->registerEntry("ne_NP", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "अज्ञात एप (wayland क्लाइन्ट आईडी {wayland_id})");

    huEngine->registerEntry("ne_NP", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "तपाईँको XDG_CURRENT_DESKTOP वातावरण बाहिरबाट व्यवस्थापन भइरहेको जस्तो देखिएको छ, अहिले {value} देखाइरहेको छ।\nजानीजानी नगरीएको भएमा यसले समस्याहरु निम्त्याउन सक्छ।");
    huEngine->registerEntry("ne_NP", TXT_KEY_NOTIF_NO_GUIUTILS, "तपाइँको सिस्टममा hyprland-guiutils इन्सटल गरिएको छैन। केहि डायलगहरुका लागि यो रनटाइम डिपेन्डेन्सी हो। कृपया इन्सटल गर्नुहोला।");
    huEngine->registerEntry("ne_NP", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "हाइपरल्यान्डले एउटा अत्यावश्यक एसेट लोड गर्न सकेन, तपाइँको डिस्ट्रोको प्याकेजरको प्याकेजिङ गतिलो छैन!";
        return "हाइपरल्यान्डले {count} अत्यावश्यक एसेटहरु लोड गर्न सकेन, तपाइँको डिस्ट्रोको प्याकेजरको प्याकेजिङ गतिलो छैन!";
    });
    huEngine->registerEntry("ne_NP", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "तपाइँको मनिटरको लेआउट गलत तरिकाले मिलाइएको छ। लेआउटमा {name} मनिटर अर्को मनिटर वा मनिटरहरुसङ्ग ओभरल्याप भएको छ।\nथप बुझ्नलाई कृपया विकिको मनिटर पेज हेर्नुहोस्।"
                            "यसले <b>निश्चित रुपमा</b> समस्या निम्त्याउने छ।");
    huEngine->registerEntry("ne_NP", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "{name} मनिटरले चाहेको कुनै पनि मोड सेट गर्न सकेन, {mode} मोडमा फर्कँदै।");
    huEngine->registerEntry("ne_NP", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "{name} मनिटरलाई अमान्य स्केल पठाइयो: {scale}, सजेस्ट गरिएको स्केल प्रयोग गर्दै: {fixed_scale}");
    huEngine->registerEntry("ne_NP", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "{name} प्लगिन लोेड गर्न सकिएन: {error}");
    huEngine->registerEntry("ne_NP", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader रिलोड गर्न सकिएन, rgba/rgbx मा फर्कँदै।");
    huEngine->registerEntry("ne_NP", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "{name} मनिटर: wide color gamut अन छ तर डिस्प्ले 10-bit मोड मा छैन।");

    // nl_NL (Dutch)
    huEngine->registerEntry("nl_NL", TXT_KEY_ANR_TITLE, "Applicatie Reageert Niet");
    huEngine->registerEntry("nl_NL", TXT_KEY_ANR_CONTENT, "Een applicatie {title} - {class} reageert niet.\nWat wilt u doen?");
    huEngine->registerEntry("nl_NL", TXT_KEY_ANR_OPTION_TERMINATE, "Beëindigen");
    huEngine->registerEntry("nl_NL", TXT_KEY_ANR_OPTION_WAIT, "Wachten");
    huEngine->registerEntry("nl_NL", TXT_KEY_ANR_PROP_UNKNOWN, "(onbekend)");

    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Een applicatie <b>{app}</b> vraagt om een onbekende machtiging.");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Een applicatie <b>{app}</b> probeert uw scherm op te nemen.\n\nWilt u dit toestaan?");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Een applicatie <b>{app}</b> probeert een plugin te laden: <b>{plugin}</b>.\n\nWilt u dit toestaan?");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_REQUEST_KEYBOARD,
                            "Een nieuw toetsenbord is gedetecteerd: <b>{keyboard}</b>.\n\nWilt u toestemming geven dat het wordt gebruikt?");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(onbekend)");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_TITLE, "Toestemmingsverzoek");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tip: U kunt hiervoor vaste regels instellen in het Hyprland-configuratiebestand.");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_ALLOW, "Toestaan");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Toestaan en onthouden");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_ALLOW_ONCE, "Één keer toestaan");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_DENY, "Weigeren");
    huEngine->registerEntry("nl_NL", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Onbekende applicatie (wayland client ID {wayland_id})");

    huEngine->registerEntry(
        "nl_NL", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "De XDG_CURRENT_DESKTOP omgevingsvariabele lijkt extern beheerd te worden en de huidige waarde is {value}.\nDit kan problemen veroorzaken, tenzij dit opzettelijk is.");
    huEngine->registerEntry("nl_NL", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Hyprland-guiutils is niet op uw systeem geïnstalleerd. Dit is een runtime-afhankelijkheid voor sommige dialogen. Overweeg het te installeren.");
    huEngine->registerEntry("nl_NL", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland kon {count} essentieel bestand niet laden, geef de pakketbeheerder van uw distro de schuld voor slecht verpakkingswerk!";
        return "Hyprland kon {count} essentiële bestanden niet laden, geef de pakketbeheerder van uw distro de schuld voor slecht verpakkingswerk!";
    });
    huEngine->registerEntry("nl_NL", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Uw monitorindeling is onjuist ingesteld. Monitor {name} overlapt met één of meerdere andere monitoren in de indeling.\n"
                            "Zie de wiki (Monitors pagina) voor meer informatie. Dit <b>zal</b> problemen veroorzaken.");
    huEngine->registerEntry("nl_NL", TXT_KEY_NOTIF_MONITOR_MODE_FAIL,
                            "Monitor {name} is er niet in geslaagd om een van de aangevraagde modi toe te passen en gebruikt nu de modus {mode}.");
    huEngine->registerEntry("nl_NL", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
                            "Ongeldige schaal opgegeven voor monitor {name}: {scale}, de voorgestelde schaal {fixed_scale} wordt gebruikt.");
    huEngine->registerEntry("nl_NL", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Plugin {name} kon niet worden geladen: {error}");
    huEngine->registerEntry("nl_NL", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Het opnieuw laden van de CM-shader is mislukt. Er wordt teruggevallen op rgba/rgbx.");
    huEngine->registerEntry("nl_NL", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: breed kleurbereik is ingeschakeld maar het scherm staat niet in 10-bitmodus.");

    // pl_PL (Polish)
    huEngine->registerEntry("pl_PL", TXT_KEY_ANR_TITLE, "Aplikacja Nie Odpowiada");
    huEngine->registerEntry("pl_PL", TXT_KEY_ANR_CONTENT, "Aplikacja {title} - {class} nie odpowiada.\nCo chcesz z nią zrobić?");
    huEngine->registerEntry("pl_PL", TXT_KEY_ANR_OPTION_TERMINATE, "Zakończ proces");
    huEngine->registerEntry("pl_PL", TXT_KEY_ANR_OPTION_WAIT, "Czekaj");
    huEngine->registerEntry("pl_PL", TXT_KEY_ANR_PROP_UNKNOWN, "(nieznane)");

    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Aplikacja <b>{app}</b> prosi o pozwolenie na nieznany typ operacji.");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Aplikacja <b>{app}</b> prosi o dostęp do twojego ekranu.\n\nCzy chcesz jej na to pozwolić?");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Aplikacja <b>{app}</b> próbuje załadować plugin: <b>{plugin}</b>.\n\nCzy chcesz jej na to pozwolić?");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Wykryto nową klawiaturę: <b>{keyboard}</b>.\n\nCzy chcesz jej pozwolić operować?");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(nieznane)");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_TITLE, "Prośba o pozwolenie");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Podpowiedź: możesz ustawić stałe zasady w konfiguracji Hyprland'a.");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_ALLOW, "Zezwól");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Zezwól i zapamiętaj");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_ALLOW_ONCE, "Zezwól raz");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_DENY, "Odmów");
    huEngine->registerEntry("pl_PL", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Nieznana aplikacja (ID klienta wayland {wayland_id})");

    huEngine->registerEntry("pl_PL", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Zmienna środowiska XDG_CURRENT_DESKTOP została ustawiona zewnętrznie na {value}.\nTo może sprawić problemy, chyba, że jest celowe.");
    huEngine->registerEntry("pl_PL", TXT_KEY_NOTIF_NO_GUIUTILS, "Twój system nie ma hyprland-guiutils zainstalowanych, co może sprawić problemy. Zainstaluj pakiet.");
    huEngine->registerEntry("pl_PL", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo == 1)
            return "Nie udało się załadować {count} kluczowego zasobu, wiń swojego packager'a za robienie słabej roboty!";

        return "Nie udało się załadować {count} kluczowych zasobów, wiń swojego packager'a za robienie słabej roboty!";
    });
    huEngine->registerEntry("pl_PL", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Pozycje twoich monitorów nie są ustawione poprawnie. Monitor {name} wchodzi na inne monitory.\nWejdź na wiki (stronę Monitory) "
                            "po więcej. To <b>będzie</b> sprawiać problemy.");
    huEngine->registerEntry("pl_PL", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} nie zaakceptował żadnego wybranego programu. Użyto {mode}.");
    huEngine->registerEntry("pl_PL", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Nieprawidłowa skala dla monitora {name}: {scale}, użyto proponowanej skali: {fixed_scale}");
    huEngine->registerEntry("pl_PL", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Nie udało się załadować plugin'a {name}: {error}");
    huEngine->registerEntry("pl_PL", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Nie udało się przeładować shader'a CM, użyto rgba/rgbx.");
    huEngine->registerEntry("pl_PL", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: skonfigurowano szeroką głębię barw, ale monitor nie jest w trybie 10-bit.");

    // pt_PT (Portuguese Portugal)
    huEngine->registerEntry("pt_PT", TXT_KEY_ANR_TITLE, "A aplicação não está a responder");
    huEngine->registerEntry("pt_PT", TXT_KEY_ANR_CONTENT, "Uma aplicação {title} - {class} não está a responder.\nO que pretendes fazer com ela?");
    huEngine->registerEntry("pt_PT", TXT_KEY_ANR_OPTION_TERMINATE, "Terminar");
    huEngine->registerEntry("pt_PT", TXT_KEY_ANR_OPTION_WAIT, "Esperar");
    huEngine->registerEntry("pt_PT", TXT_KEY_ANR_PROP_UNKNOWN, "(desconhecido)");

    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Uma aplicação <b>{app}</b> está a pedir uma permissão desconhecida.");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Uma aplicação <b>{app}</b> está a tentar fazer uma captura do ecrã.\n\nQueres permiti-lo?");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "A aplicação <b>{app}</b> está a tentar carregar o plugin: <b>{plugin}</b>.\n\nQueres permiti-lo?");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Um novo teclado foi detectado: <b>{keyboard}</b>.\n\nQueres permitir a sua operação?");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(desconhecido)");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_TITLE, "Pedido de permissão");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Dica: podes definir regras persistentes para estes no ficheiro de configuração do Hyprland.");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_ALLOW, "Permitir");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Permitir sempre");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_ALLOW_ONCE, "Permitir esta vez");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_DENY, "Recusar");
    huEngine->registerEntry("pt_PT", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Aplicação desconhecida (ID de cliente wayland {wayland_id})");

    huEngine->registerEntry(
        "pt_PT", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "O teu ambiente XDG_CURRENT_DESKTOP parece estar a ser gerido externamente, e o valor actual é {value}.\nIsto pode causar problemas a não ser que seja intencional.");
    huEngine->registerEntry("pt_PT", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "O teu sistema não tem o hyprland-guiutils instalado. Esta dependência de runtime é necessária para algumas caixas de diálogo, deverias instalá-la.");
    huEngine->registerEntry("pt_PT", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland não conseguiu carregar {count} asset essencial, podes culpar o gestor de dependências da tua distro por fazer um mau trabalho!";
        return "Hyprland não conseguiu carregar {count} assets essenciais, podes culpar o gestor de dependências da tua distro por fazer um mau trabalho!";
    });
    huEngine->registerEntry(
        "pt_PT", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "O layout do teu monitor não está configurado correctamente. Monitor {name} está em conflito com outro(s) monitor(es) no layout.\nProcura na wiki (página Monitores) para "
        "mais informações. Isto <b>vai</b> causar problemas.");
    huEngine->registerEntry("pt_PT", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} falhou ao configurar os modos requisitados, revertento para o modo {mode} de volta.");
    huEngine->registerEntry("pt_PT", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Resolução inválida para o monitor {name}: {scale}, revertendo para a resolução sugerida: {fixed_scale}");
    huEngine->registerEntry("pt_PT", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Falha ao carregar o plugin {name}: {error}");
    huEngine->registerEntry("pt_PT", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader falhou ao recarregar, revertendo para rgba/rgbx.");
    huEngine->registerEntry("pt_PT", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: gama de cores ampla está activada mas o monitor não está em modo 10-bits.");

    // zh_CN (Simplified Chinese)
    huEngine->registerEntry("zh_CN", TXT_KEY_ANR_TITLE, "应用程序未响应");
    huEngine->registerEntry("zh_CN", TXT_KEY_ANR_CONTENT, "应用程序 {title} - {class} 未响应。\n你想要采取什么行动？");
    huEngine->registerEntry("zh_CN", TXT_KEY_ANR_OPTION_TERMINATE, "终止");
    huEngine->registerEntry("zh_CN", TXT_KEY_ANR_OPTION_WAIT, "等待");
    huEngine->registerEntry("zh_CN", TXT_KEY_ANR_PROP_UNKNOWN, "（未知）");

    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "应用程序 <b>{app}</b> 正在请求一个未知的权限。");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "应用程序 <b>{app}</b> 想要捕获你的屏幕。\n\n允许它这么做吗？");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "应用程序 <b>{app}</b> 想要加载插件： <b>{plugin}</b>。\n\n允许它这么做吗？");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "检测到新的键盘 <b>{keyboard}</b> 接入了。\n\n允许这个键盘操作你的系统吗？");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "（未知）");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_TITLE, "权限请求");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "提示：你可以在Hyprland配置中为他们创建永久性的规则。");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_ALLOW, "允许");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "总是允许");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_ALLOW_ONCE, "允许一次");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_DENY, "阻止");
    huEngine->registerEntry("zh_CN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "未知的应用程序 （Wayland客户端ID {wayland_id}）");

    huEngine->registerEntry("zh_CN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "你的环境变量XDG_CURRENT_DESKTOP似乎被外部管理，且当前的值为{value}。如果你不是有意这么做，这可能会导致问题。");
    huEngine->registerEntry("zh_CN", TXT_KEY_NOTIF_NO_GUIUTILS, "你的系统似乎没有安装hyprland-guiutils。这是一个用于部分对话框的运行时依赖。请考虑安装。");
    huEngine->registerEntry("zh_CN", TXT_KEY_NOTIF_FAILED_ASSETS, "Hyprland无法加载{count}个重要资产，问问你发行版的打包者在打包个什么玩意！？");
    huEngine->registerEntry("zh_CN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "你的显示器没有被正确设置。显示器 {name} 和其他显示器的布局重叠了。请看wiki中的“显示器”一章获取更多信息。这<b>会</b>导致问题。");
    huEngine->registerEntry("zh_CN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "显示器 {name} 无法被设置为任何请求的模式，将使用 {mode} 兜底。");
    huEngine->registerEntry("zh_CN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "显示器 {name} 被设置了非法的缩放：{scale}，将使用建议的缩放：{fixed_scale}");
    huEngine->registerEntry("zh_CN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "无法加载插件 {name}：{error}");
    huEngine->registerEntry("zh_CN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "无法重新加载CM着色器，将使用rgba/rgbx兜底。");
    huEngine->registerEntry("zh_CN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "显示器 {name}：宽色域被启用了，但是显示器并不在10-bit模式。");

    // ar (Arabic - Modern Standard)
    huEngine->registerEntry("ar", TXT_KEY_ANR_TITLE, "التطبيق لا يستجيب");
    huEngine->registerEntry("ar", TXT_KEY_ANR_CONTENT, "التطبيق {title} - {class} لا يستجيب.\nما الذي تريد فعله؟");
    huEngine->registerEntry("ar", TXT_KEY_ANR_OPTION_TERMINATE, "إنهاء");
    huEngine->registerEntry("ar", TXT_KEY_ANR_OPTION_WAIT, "الانتظار");
    huEngine->registerEntry("ar", TXT_KEY_ANR_PROP_UNKNOWN, "(غير معروف)");

    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "يطلب التطبيق <b>{app}</b> صلاحية غير معروفة.");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "يحاول التطبيق <b>{app}</b> التقاط الشاشة.\n\nهل تريد السماح له بذلك؟");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "يحاول التطبيق <b>{app}</b> تحميل إضافة: <b>{plugin}</b>.\n\nهل تريد السماح له بذلك؟");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "تم اكتشاف لوحة مفاتيح جديدة: <b>{keyboard}</b>.\n\nهل تريد السماح لها بالعمل؟");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(غير معروف)");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_TITLE, "طلب الإذن");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "تلميح: يمكنك تعيين قواعد دائمة لهذه الطلبات في ملف إعدادات Hyprland.");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_ALLOW, "السماح");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "السماح مع تذكّر الاختيار");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_ALLOW_ONCE, "السماح لمرة واحدة");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_DENY, "الرفض");
    huEngine->registerEntry("ar", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "تطبيق غير معروف (معرّف عميل Wayland {wayland_id})");

    huEngine->registerEntry("ar", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "يبدو أنّ متغيّر البيئة XDG_CURRENT_DESKTOP يُدار من خارج النظام، والقيمة الحالية هي {value}.\n"
                            "قد يؤدي ذلك إلى مشكلات ما لم يكن مقصودًا.");

    huEngine->registerEntry("ar", TXT_KEY_NOTIF_NO_GUIUTILS, "لا يحتوي نظامك على الحزمة hyprland-guiutils مثبتة. هذه حزمة مطلوبة أثناء التشغيل لبعض مربعات الحوار. يُنصَح بتثبيتها.");

    huEngine->registerEntry("ar", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "فشل Hyprland في تحميل مورد أساسي ({count}). قد يكون السبب سوء تغليف الحزم في التوزيعة.";
        return "فشل Hyprland في تحميل {count} من الموارد الأساسية. قد يكون السبب سوء تغليف الحزم في التوزيعة.";
    });

    huEngine->registerEntry("ar", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "تم إعداد مخطط الشاشات لديك بشكل غير صحيح. الشاشة {name} تتداخل مع شاشة أو أكثر في المخطط.\n"
                            "يرجى مراجعة صفحة الشاشات في الويكي لمزيد من التفاصيل. هذا <b>سيسبب</b> مشكلات.");

    huEngine->registerEntry("ar", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "فشلت الشاشة {name} في ضبط أي من الأوضاع المطلوبة، وسيتم الرجوع إلى الوضع {mode}.");

    huEngine->registerEntry("ar", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "تم تمرير قيمة تحجيم غير صالحة إلى الشاشة {name}: {scale}. سيتم استخدام قيمة التحجيم المقترحة: {fixed_scale}.");

    huEngine->registerEntry("ar", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "فشل تحميل الإضافة {name}: {error}");

    huEngine->registerEntry("ar", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "فشلت إعادة تحميل نظام إدارة الألوان (CM). سيتم الرجوع إلى صيغة الألوان rgba/rgbx.");

    huEngine->registerEntry("ar", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "الشاشة {name}: تم تفعيل نطاق الألوان الواسع، لكن العرض ليس في وضع 10 بت.");

    // ru_RU (Russian)
    huEngine->registerEntry("ru_RU", TXT_KEY_ANR_TITLE, "Приложение не отвечает");
    huEngine->registerEntry("ru_RU", TXT_KEY_ANR_CONTENT, "Приложение {title} - {class} не отвечает.\nЧто вы хотите сделать?");
    huEngine->registerEntry("ru_RU", TXT_KEY_ANR_OPTION_TERMINATE, "Завершить");
    huEngine->registerEntry("ru_RU", TXT_KEY_ANR_OPTION_WAIT, "Подождать");
    huEngine->registerEntry("ru_RU", TXT_KEY_ANR_PROP_UNKNOWN, "(неизвестно)");

    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Приложение <b>{app}</b> запрашивает неизвестное разрешение.");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Приложение <b>{app}</b> пытается получить доступ к вашему экрану.\n\nРазрешить?");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Приложение <b>{app}</b> пытается загрузить плагин: <b>{plugin}</b>.\n\nРазрешить?");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Обнаружена новая клавиатура: <b>{keyboard}</b>.\n\nРазрешить ей работать?");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(неизвестно)");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_TITLE, "Запрос разрешения");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Подсказка: вы можете настроить постоянные правила для этого в конфигурационном файле Hyprland.");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_ALLOW, "Разрешить");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Разрешить и запомнить");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_ALLOW_ONCE, "Разрешить в этот раз");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_DENY, "Отклонить");
    huEngine->registerEntry("ru_RU", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Неизвестное приложение (wayland client ID {wayland_id})");

    huEngine->registerEntry(
        "ru_RU", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Переменная окружения XDG_CURRENT_DESKTOP установлена извне, текущее значение: {value}.\nЭто может вызвать проблемы, если только это не сделано намеренно.");
    huEngine->registerEntry("ru_RU", TXT_KEY_NOTIF_NO_GUIUTILS, "Пакет hyprland-guiutils не установлен. Он необходим для некоторых диалогов. Рекомендуется установить его.");
    huEngine->registerEntry("ru_RU", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Не удалось загрузить {count} критически важный ресурс, пожалуйтесь мейнтейнеру вашего дистрибутива за кривую сборку пакета!";
        return "Не удалось загрузить {count} критически важных ресурсов, пожалуйтесь мейнтейнеру вашего дистрибутива за кривую сборку пакета!";
    });
    huEngine->registerEntry(
        "ru_RU", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Неправильно настроен макет мониторов. Монитор {name} перекрывает другие.\nПодробнее см. в документации (страница Monitors). Это <b>обязательно</b> вызовет проблемы.");
    huEngine->registerEntry("ru_RU", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Монитор {name} не смог установить ни один из запрошенных режимов, выбран режим {mode}.");
    huEngine->registerEntry("ru_RU", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Недопустимый масштаб для монитора {name}: {scale}, используется предложенный масштаб: {fixed_scale}");
    huEngine->registerEntry("ru_RU", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Не удалось загрузить плагин {name}: {error}");
    huEngine->registerEntry("ru_RU", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Не удалось перезагрузить CM shader, используется rgba/rgbx.");
    huEngine->registerEntry("ru_RU", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Монитор {name}: расширенный цветовой охват включён, но дисплей не в 10-bit режиме.");

    // sl_SI (Slovenian)
    huEngine->registerEntry("sl_SI", TXT_KEY_ANR_TITLE, "Program se ne odziva");
    huEngine->registerEntry("sl_SI", TXT_KEY_ANR_CONTENT, "Program {title} - {class} se ne odziva.\nKaj želite storiti?");
    huEngine->registerEntry("sl_SI", TXT_KEY_ANR_OPTION_TERMINATE, "Prekini");
    huEngine->registerEntry("sl_SI", TXT_KEY_ANR_OPTION_WAIT, "Počakaj");
    huEngine->registerEntry("sl_SI", TXT_KEY_ANR_PROP_UNKNOWN, "(neznano)");

    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Program <b>{app}</b> zahteva neznano dovoljenje.");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Program <b>{app}</b> poskuša zajeti vaš zaslon.\n\nAli mu želite to dovoliti?");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Program <b>{app}</b> skuša naložiti vtičnik: <b>{plugin}</b>.\n\nAli mu želite to dovoliti?");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Nova tipkovnica je bila zaznana: <b>{keyboard}</b>.\n\nAli ji želite dovoliti delovanje?");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(neznano)");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_TITLE, "Zahteva za dovoljenje");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Namig: v Hyprlandovi konfiguracijski datoteki lahko nastavite stalna pravila za dovoljenja.");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_ALLOW, "Dovoli");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Dovoli in si zapomni");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_ALLOW_ONCE, "Dovoli enkrat");
    huEngine->registerEntry("sl_SI", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Neznan program (ID stranke Wayland: {wayland_id})");

    huEngine->registerEntry("sl_SI", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Zdi se, da je Vaše okolje XDG_CURRENT_DESKTOP upravljano od zunaj, trenutna vrednost je {value}.\nTo lahko povzroči težave, če ni namerno.");
    huEngine->registerEntry("sl_SI", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "hyprland-guiutils ni nameščen na vaši napravi. To je odvisnost od izvajalnega okolja za nekatera pogovorna okna. Razmislite o namestitvi.");
    huEngine->registerEntry("sl_SI", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assertNo = std::stoi(vars.at("count"));
        if (assertNo <= 1)
            return "Hyprlandu ni uspelo naložiti {count} bistvenega sredstva, krivite upravitelja paketov vaše distribucije za slabo opravljeno pakiranje!";
        return "Hyprlandu ni uspelo naložiti {count} bistvenih sredstev, krivite upravitelja paketov vaše distribucije za slabo opravljeno pakiranje!";
    });
    huEngine->registerEntry("sl_SI", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Vaša zaslonska razporeditev je napačno nastavljena. Zaslon {monitor} se prekriva z drugimi zasloni v razporeditvi.\n"
                            "Prosimo poglejte wiki (stran Monitors) za več. To <b>bo</b> povzročilo težave.");
    huEngine->registerEntry("sl_SI", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Zaslon {name} ni uspel nastaviti nobenega zahtevanega načina, vrnitev k načinu {mode}.");
    huEngine->registerEntry("sl_SI", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
                            "Neveljavna skala je bila posredovana zaslonu {name}: {scale}, uporabljena je predlagana skala: {fixed_scale}");
    huEngine->registerEntry("sl_SI", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Vtičnika {name} ni bilo mogoče naložiti: {error}");
    huEngine->registerEntry("sl_SI", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Ponovno nalaganje senčnika CM ni uspelo, vrnitev k rgba/rgbx.");
    huEngine->registerEntry("sl_SI", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Zaslon {name}: širok barvni razpon je omogočen, vendar zaslon ni v 10-bitnem načinu.");

    // sr_RS (Serbian)
    huEngine->registerEntry("sr_RS", TXT_KEY_ANR_TITLE, "Апликација не реагује");
    huEngine->registerEntry("sr_RS", TXT_KEY_ANR_CONTENT, "Апликација {title} - {class} не реагује.\nШта желите да урадите са њом?");
    huEngine->registerEntry("sr_RS", TXT_KEY_ANR_OPTION_TERMINATE, "Прекини");
    huEngine->registerEntry("sr_RS", TXT_KEY_ANR_OPTION_WAIT, "Чекај");
    huEngine->registerEntry("sr_RS", TXT_KEY_ANR_PROP_UNKNOWN, "(непознато)");

    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Апликација <b>{app}</b> захтева непознату дозволу.");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Апликација <b>{app}</b> покушава да снима твој екран.\n\nДа ли желиш да то дозволиш?");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Апликација <b>{app}</b> покушава да учита додатак: <b>{plugin}</b>.\n\nДа ли желиш да то дозволиш?");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Нова тастатура је детектована: <b>{keyboard}</b>.\n\nДа ли желиш да дозволиш њен рад?");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(непознато)");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_TITLE, "Захтев за дозволу");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Савет: можеш направити трајна правила за ово у Hyprland конфигурационој датотеци.");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_ALLOW, "Дозволи");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Дозволи и запамти");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_ALLOW_ONCE, "Дозволи једном");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_DENY, "Одбиј");
    huEngine->registerEntry("sr_RS", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Непозната апликација (wayland client ID {wayland_id})");

    huEngine->registerEntry("sr_RS", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Изгледа да се твојим XDG_CURRENT_DESKTOP окружењем управља споља, и тренутна вредност је {value}.\nОво може правити проблеме осим ако је намерно.");
    huEngine->registerEntry("sr_RS", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Твој систем нема инсталиран hyprland-guiutils. Ово је зависност при покретању за неке дијалоге. Размотри инсталацију.");
    huEngine->registerEntry("sr_RS", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland није успео да учита {count} кључни ресурс, криви пакера твоје дистрибуције за лоше одрађен посао!";
        if (assetsNo <= 4)
            return "Hyprland није успео да учита {count} кључна ресурса, криви пакера твоје дистрибуције за лоше одрађен посао!";
        return "Hyprland није успео да учита {count} кључних ресурса, криви пакера твоје дистрибуције за лоше одрађен посао!";
    });
    huEngine->registerEntry(
        "sr_RS", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Твој распоред монитора је неправилно постављен. Монитор {name} се преклапа са другим монитором/мониторима у распореду.\nМолим те погледај вики (Monitors страницу) за "
        "више информација. Ово <b>ће</b> изазвати проблеме.");
    huEngine->registerEntry("sr_RS", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Монитор {name} није успео да постави ниједан тражени режим, враћање на режим {mode}.");
    huEngine->registerEntry("sr_RS", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Невалидна скала прослеђена монитору {name}: {scale}, користи се препоручена скала: {fixed_scale}");
    huEngine->registerEntry("sr_RS", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Неуспешно учитавање додатка {name}: {error}");
    huEngine->registerEntry("sr_RS", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Поново учитавање CM шејдера није успело, враћање на rgba/rgbx.");
    huEngine->registerEntry("sr_RS", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Монитор {name}: широк спектар боја је омогућен али екран није у 10-битном режиму.");

    // sr_RS@latin (Serbian Latin)
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_ANR_TITLE, "Aplikacija ne reaguje");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_ANR_CONTENT, "Aplikacija {title} - {class} ne reaguje.\nŠta želite da uradite sa njom?");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_ANR_OPTION_TERMINATE, "Prekini");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_ANR_OPTION_WAIT, "Čekaj");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_ANR_PROP_UNKNOWN, "(nepoznato)");

    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Aplikacija <b>{app}</b> zahteva nepoznatu dozvolu.");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Aplikacija <b>{app}</b> pokušava da snima tvoj ekran.\n\nDa li želiš da to dozvoliš?");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Aplikacija <b>{app}</b> pokušava da učita dodatak: <b>{plugin}</b>.\n\nDa li želiš da to dozvoliš?");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Nova tastatura je detektovana: <b>{keyboard}</b>.\n\nDa li želiš da dozvoliš njen rad?");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(nepoznato)");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_TITLE, "Zahtev za dozvolu");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Savet: možeš napraviti trajna pravila za ovo u Hyprland konfiguracionoj datoteci.");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_ALLOW, "Dozvoli");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Dozvoli i zapamti");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_ALLOW_ONCE, "Dozvoli jednom");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_DENY, "Odbij");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Nepoznata aplikacija (wayland client ID {wayland_id})");

    huEngine->registerEntry("sr_RS@latin", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "Izgleda da se tvojim XDG_CURRENT_DESKTOP okruženjem upravlja spolja, i trenutna vrednost je {value}.\nOvo može praviti probleme osim ako je namerno.");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Tvoj sistem nema instaliran hyprland-guiutils. Ovo je zavisnost pri pokretanju za neke dijaloge. Razmotri instalaciju.");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland nije uspeo da učita {count} ključni resurs, krivi pakera tvoje distribucije za loše odrađen posao!";
        if (assetsNo <= 4)
            return "Hyprland nije uspeo da učita {count} ključna resursa, krivi pakera tvoje distribucije za loše odrađen posao!";
        return "Hyprland nije uspeo da učita {count} ključnih resursa, krivi pakera tvoje distribucije za loše odrađen posao!";
    });
    huEngine->registerEntry(
        "sr_RS@latin", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Tvoj raspored monitora je nepravilno postavljen. Monitor {name} se preklapa sa drugim monitorom/monitorima u rasporedu.\nMolim te pogledaj wiki (Monitors stranicu) za "
        "više informacija. Ovo <b>će</b> izazvati probleme.");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} nije uspeo da postavi nijedan traženi režim, vraćanje na režim {mode}.");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Nevalidna skala prosleđena monitoru {name}: {scale}, koristi se preporučena skala: {fixed_scale}");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Neuspešno učitavanje dodatka {name}: {error}");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Ponovno učitavanje CM šejdera nije uspelo, vraćanje na rgba/rgbx.");
    huEngine->registerEntry("sr_RS@latin", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: širok spektar boja je omogućen ali ekran nije u 10-bitnom režimu.");

    // tr_TR (Turkish)
    huEngine->registerEntry("tr_TR", TXT_KEY_ANR_TITLE, "Uygulama Yanıt Vermiyor");
    huEngine->registerEntry("tr_TR", TXT_KEY_ANR_CONTENT, "Bir uygulama {title} - {class} yanıt vermiyor.\nBununla ne yapmak istiyorsun?");
    huEngine->registerEntry("tr_TR", TXT_KEY_ANR_OPTION_TERMINATE, "Sonlandır");
    huEngine->registerEntry("tr_TR", TXT_KEY_ANR_OPTION_WAIT, "Bekle");
    huEngine->registerEntry("tr_TR", TXT_KEY_ANR_PROP_UNKNOWN, "(bilinmiyor)");

    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Bir uygulama <b>{app}</b> bilinmeyen bir izin istiyor.");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Bir uygulama <b>{app}</b> ekran kaydı yapmaya çalışıyor.\n\nİzin vermek istiyor musun?");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Bir uygulama <b>{app}</b> bir eklenti kurmaya çalışıyor: <b>{plugin}</b>.\n\nİzin vermek istiyor musun?");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Yeni bir klavye algılandı: <b>{keyboard}</b>.\n\nÇalışmasına izin vermek istiyor musun?");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(bilinmiyor)");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_TITLE, "İzin isteği");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "İpucu: Hyprland config dosyasında bunlar için kalıcı kurallar atayabilirsin.");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_ALLOW, "İzin ver");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "İzin ver ve seçimimi hatırla");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_ALLOW_ONCE, "Yalnızca bir defa izin ver");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_DENY, "Reddet");
    huEngine->registerEntry("tr_TR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Bilinmeyen uygulama (wayland istemci ID {wayland_id})");

    huEngine->registerEntry("tr_TR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "XDG_CURRENT_DESKTOP ortamın harici olarak yönetiliyor gibi gözüküyor, ve mevcut değeri {value}.\nEğer bu bilinçli değilse sorunlara yol açabilir.");
    huEngine->registerEntry("tr_TR", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "Sisteminde hyprland-guiutils yüklü değil. Bu bazı diyaloglar için bir çalışma zamanı bağımlılığı. İndirmeyi göz önünde bulundurabilirsin.");
    huEngine->registerEntry("tr_TR", TXT_KEY_NOTIF_FAILED_ASSETS,
                            "Hyprland {count} gerekli dosyayı yüklemekte başarısız oldu, kötü bir iş çıkardığı için kullandığın distronun paketleyicisini suçla!");
    huEngine->registerEntry(
        "tr_TR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Monitör düzenin yanlış ayarlanmış. Monitör {name} düzenindeki başka monitörlerle çakışıyor.\nLütfen daha fazla bilgi için wiki'ye (Monitörler sayfası) göz at. "
        "Bu <b>sorunlara yol açacak</b>.");
    huEngine->registerEntry("tr_TR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitör {name} istenen modları ayarlamada başarısız oldu, {mode} moduna geri dönülüyor.");
    huEngine->registerEntry("tr_TR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Monitöre geçersiz ölçek iletildi {name}: {scale}, önerilen ölçek kullanılıyor: {fixed_scale}");
    huEngine->registerEntry("tr_TR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "{name} plugini yüklenemedi: {error}");
    huEngine->registerEntry("tr_TR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader yeniden yüklemesi başarısız, rgba/rgbx'e geri dönülüyor.");
    huEngine->registerEntry("tr_TR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitör {name}: wide color gamut etkinleştirildi ama ekran 10-bit modunda değil.");

    // uk_UA (Ukrainian)
    huEngine->registerEntry("uk_UA", TXT_KEY_ANR_TITLE, "Програма не відповідає");
    huEngine->registerEntry("uk_UA", TXT_KEY_ANR_CONTENT, "Програма {title} - {class} не відповідає.\nЩо ви хочете з нею зробити?");
    huEngine->registerEntry("uk_UA", TXT_KEY_ANR_OPTION_TERMINATE, "Завершити");
    huEngine->registerEntry("uk_UA", TXT_KEY_ANR_OPTION_WAIT, "Чекати");
    huEngine->registerEntry("uk_UA", TXT_KEY_ANR_PROP_UNKNOWN, "(невідомо)");

    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Програма <b>{app}</b> запитує невідомий дозвіл.");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Програма <b>{app}</b> намагається захопити екран.\n\nДозволити?");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Програма <b>{app}</b> намагається завантажити плагін: <b>{plugin}</b>.\n\nДозволити?");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Виявлено нову клавіатуру: <b>{keyboard}</b>.\n\nДозволити її використання?");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(невідомо)");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_TITLE, "Запит дозволу");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Підказка: ви можете встановити постійні правила для дозволів у файлі конфігурації Hyprland.");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_ALLOW, "Дозволити");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Дозволити та запам'ятати");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_ALLOW_ONCE, "Дозволити один раз");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_DENY, "Заборонити");
    huEngine->registerEntry("uk_UA", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Невідома програма (ідентифікатор клієнта wayland {wayland_id})");

    huEngine->registerEntry(
        "uk_UA", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Ваше середовище XDG_CURRENT_DESKTOP, схоже, керується ззовні, і поточне значення становить {value}.\nЦе може спричинити проблеми, якщо це не зроблено навмисно.");
    huEngine->registerEntry(
        "uk_UA", TXT_KEY_NOTIF_NO_GUIUTILS,
        "У вашій системі не встановлено hyprland-guiutils. Це залежність, потрібна для роботи деяких діалогових вікон. Розгляньте можливість його встановлення.");
    huEngine->registerEntry("uk_UA", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Не вдалося завантажити {count} необхідний ресурс, звинувачуйте пакувальника свого дистрибутива у недобросовісній роботі!";
        return "Не вдалося завантажити {count} необхідних ресурсів, звинувачуйте пакувальника свого дистрибутива у недобросовісній роботі!";
    });
    huEngine->registerEntry("uk_UA", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Макет моніторів налаштовано неправильно. Монітор {name} перекриває інші монітори у макеті.\nБудь ласка, перегляньте wiki (сторінка Monitors). "
                            "Це <b>обов'язково</b> створить проблеми.");
    huEngine->registerEntry("uk_UA", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Монітор {name} не зміг встановити жодного із запитуваних режимів, повернення до режиму {mode}.");
    huEngine->registerEntry("uk_UA", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
                            "Неправильний масштаб переданий монітору {name}: {scale}, використання запропонованого масштабу: {fixed_scale}");
    huEngine->registerEntry("uk_UA", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Помилка завантаження плагіна {name}: {error}");
    huEngine->registerEntry("uk_UA", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Не вдалося перезавантажити шейдер CM, повернення до rgba/rgbx.");
    huEngine->registerEntry("uk_UA", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Монітор {name}: широка кольорова гама увімкнена, але дисплей не працює в 10-бітному режимі.");

    // cs_CZ (Czech)
    huEngine->registerEntry("cs_CZ", TXT_KEY_ANR_TITLE, "Aplikace Neodpovídá");
    huEngine->registerEntry("cs_CZ", TXT_KEY_ANR_CONTENT, "Aplikace {title} - {class} neodpovídá.\nCo s ní chcete udělat?");
    huEngine->registerEntry("cs_CZ", TXT_KEY_ANR_OPTION_TERMINATE, "Ukončit");
    huEngine->registerEntry("cs_CZ", TXT_KEY_ANR_OPTION_WAIT, "Počkat");
    huEngine->registerEntry("cs_CZ", TXT_KEY_ANR_PROP_UNKNOWN, "(neznámé)");

    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Aplikace <b>{app}</b> vyžaduje neznámé oprávnění.");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Aplikace <b>{app}</b> se pokouší zaznamenávat vaši obrazovku.\n\nChcete jí to povolit?");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Aplikace <b>{app}</b> se pokouší načíst plugin: <b>{plugin}</b>.\n\nChcete jí to povolit?");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Byla detekována nová klávesnice: <b>{keyboard}</b>.\n\nChcete jí povolit?");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(neznámé)");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_TITLE, "Žádost o oprávnění");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tip: pro tyto případy můžete nastavit trvalá pravidla v konfiguračním souboru Hyprland.");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_ALLOW, "Povolit");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Povolit a zapamatovat");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_ALLOW_ONCE, "Povolit jednou");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_DENY, "Zamítnout");
    huEngine->registerEntry("cs_CZ", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Neznámá aplikace (ID klienta Wayland {wayland_id})");

    huEngine->registerEntry(
        "cs_CZ", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Proměnná prostředí XDG_CURRENT_DESKTOP se zdá být spravována externě a její aktuální hodnota je {value}.\nPokud to není záměr, může to způsobit problémy.");
    huEngine->registerEntry("cs_CZ", TXT_KEY_NOTIF_NO_GUIUTILS,
                            "V systému není nainstalován balíček hyprland-guiutils. Jedná se o závislost pro běh některých dialogových oken. Zvažte jeho instalaci.");
    huEngine->registerEntry("cs_CZ", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprlandu se nepodařilo načíst {count} nezbytnou součást. Za špatně odvedenou práci viňte tvůrce balíčku vaší distribuce!";
        if (assetsNo <= 4)
            return "Hyprlandu se nepodařilo načíst {count} nezbytné součásti. Za špatně odvedenou práci viňte tvůrce balíčku vaší distribuce!";
        return "Hyprlandu se nepodařilo načíst {count} nezbytných součástí. Za špatně odvedenou práci viňte tvůrce balíčku vaší distribuce!";
    });
    huEngine->registerEntry("cs_CZ", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                            "Rozložení vašich monitorů je nastaveno nesprávně. Monitor {name} se překrývá s ostatními monitory.\nVíce informací naleznete na wiki "
                            "(stránka Monitors). Toto <b>způsobí</b> problémy.");
    huEngine->registerEntry("cs_CZ", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitoru {name} se nepodařilo nastavit žádný z požadovaných režimů, vrací se k režimu {mode}.");
    huEngine->registerEntry("cs_CZ", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Monitoru {name} bylo předáno neplatné měřítko: {scale}, použije se navrhované měřítko: {fixed_scale}");
    huEngine->registerEntry("cs_CZ", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Nepodařilo se načíst plugin {name}: {error}");
    huEngine->registerEntry("cs_CZ", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Nepodařilo se znovu načíst CM shader, vrací se k rgba/rgbx.");
    huEngine->registerEntry("cs_CZ", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: široký barevný gamut je povolen, ale displej není v 10bitovém režimu.");
}

std::string I18n::CI18nEngine::localize(eI18nKeys key, const Hyprutils::I18n::translationVarMap& vars) {
    static auto CONFIG_LOCALE = CConfigValue<std::string>("general:locale");
    std::string locale        = *CONFIG_LOCALE != "" ? *CONFIG_LOCALE : localeStr;
    return huEngine->localizeEntry(locale, key, vars);
}
