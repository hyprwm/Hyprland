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
}

std::string I18n::CI18nEngine::localize(eI18nKeys key, const Hyprutils::I18n::translationVarMap& vars) {
    return huEngine->localizeEntry(localeStr, key, vars);
}
