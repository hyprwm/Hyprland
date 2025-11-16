#include "Engine.hpp"

#include <hyprutils/i18n/I18nEngine.hpp>

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

    // es (Spanish)
    huEngine->registerEntry("es", TXT_KEY_ANR_TITLE, "La aplicación no responde");
    huEngine->registerEntry("es", TXT_KEY_ANR_CONTENT, "Una aplicación {title} - {class} no responde.\n¿Qué quieres hacer?");
    huEngine->registerEntry("es", TXT_KEY_ANR_OPTION_TERMINATE, "Terminar");
    huEngine->registerEntry("es", TXT_KEY_ANR_OPTION_WAIT, "Esperar");
    huEngine->registerEntry("es", TXT_KEY_ANR_PROP_UNKNOWN, "(desconocido)");

    huEngine->registerEntry("es", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Una aplicación <b>{app}</b> está solicitando un permiso desconocido.");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Una aplicación <b>{app}</b> está intentando capturar la pantalla.\n\n¿Quieres permitirlo?");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Una aplicación <b>{app}</b> está intentando cargar un plugin: <b>{plugin}</b>.\n\n¿Quieres permitirlo?");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Se ha detectado un nuevo teclado: <b>{keyboard}</b>.\n\n¿Quieres permitir su funcionamiento?");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(desconocido)");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_TITLE, "Solicitud de permiso");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Sugerencia: puedes establecer reglas persistentes para estos en el archivo de configuración de Hyprland.");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_ALLOW, "Permitir");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Permitir y recordar");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_ALLOW_ONCE, "Permitir una vez");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_DENY, "Denegar");
    huEngine->registerEntry("es", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Aplicación desconocida (wayland client ID {wayland_id})");

    huEngine->registerEntry("es", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                            "La variable de entorno XDG_CURRENT_DESKTOP parece estar gestionada externamente, y el valor actual es {value}.\nEsto podría causar problemas, a menos "
                            "que sea intencionado.");
    huEngine->registerEntry(
        "es", TXT_KEY_NOTIF_NO_GUIUTILS,
        "Tu sistema no tiene instalado hyprland-guiutils. Se trata de una dependencia de tiempo de ejecución para algunos diálogos. Considera la posibilidad de instalarlo.");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "No se ha podido cargar {count} recurso clave, ¡culpa a tu empaquetador por hacer un mal trabajo!";
        return "No se ha podido cargar {count} recursos clave, ¡culpa a tu empaquetador por hacer un mal trabajo!";
    });
    huEngine->registerEntry(
        "es", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "La configuración de su monitor no es correcta. El monitor {name} se superpone con otros monitores en la configuración. Consulte la wiki (página Monitors, en inglés) "
        "para obtener más información. Esto <b>provocará</b> problemas.");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_MONITOR_MODE_FAIL,
                            "El monitor {name} no ha podido configurar ninguno de los modos solicitados, por lo que ha recurrido al modo {mode}.");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Escala no válida pasada al monitor {name}: {scale}, utilizando la escala sugerida: {fixed_scale}");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Error al cargar el plugin {name}: {error}");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Error al recargar el sombreador CM, recurriendo a rgba/rgbx.");
    huEngine->registerEntry("es", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: la gama de colores amplia está habilitada, pero la pantalla no está en modo de 10-bit.");

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

    // it_IT (Italian)
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_TITLE, "L'applicazione non risponde");
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_CONTENT, "Un'applicazione {title} - {class} non risponde.\nCosa vuoi fare?");
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_OPTION_TERMINATE, "Termina");
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_OPTION_WAIT, "Attendi");
    huEngine->registerEntry("it_IT", TXT_KEY_ANR_PROP_UNKNOWN, "(sconosciuto)");

    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Un'applicazione <b>{app}</b> richiede un'autorizzazione sconosciuta.");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Un'applicazione <b>{app}</b> sta provando a catturare il tuo schermo.\n\nGlie lo vuoi permettere?");
    huEngine->registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_PLUGIN,
                            "Un'applicazione <b>{app}</b> sta provando a caricare un plugin: <b>{plugin}</b>.\n\nGlie lo vuoi permettere?");
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
}

std::string I18n::CI18nEngine::localize(eI18nKeys key, const Hyprutils::I18n::translationVarMap& vars) {
    return huEngine->localizeEntry(localeStr, key, vars);
}
