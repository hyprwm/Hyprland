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
