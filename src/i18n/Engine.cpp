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

// prevents CI18nEngine constructor from being bloated by std::string allocations/deallocations
[[gnu::noinline]] static void registerEntry(const char* locale, eI18nKeys key, const char* translation) {
    huEngine->registerEntry(locale, key, translation);
}

[[gnu::noinline]] static void registerEntry(const char* locale, eI18nKeys key, const char* (*translationFunc)(const Hyprutils::I18n::translationVarMap&)) {
    huEngine->registerEntry(locale, key, translationFunc);
}

I18n::CI18nEngine::CI18nEngine() {
    huEngine = makeShared<Hyprutils::I18n::CI18nEngine>();
    huEngine->setFallbackLocale("en_US");
    localeStr = huEngine->getSystemLocale().locale();

    // be_BY (Belarusian)
    registerEntry("be_BY", TXT_KEY_ANR_TITLE, "Праграма не адказвае");
    registerEntry("be_BY", TXT_KEY_ANR_CONTENT, "Праграма {title} - {class} не адказвае.\nШто хочаце з ёй зрабіць?");
    registerEntry("be_BY", TXT_KEY_ANR_OPTION_TERMINATE, "Прымусова спыніць");
    registerEntry("be_BY", TXT_KEY_ANR_OPTION_WAIT, "Пачакаць");
    registerEntry("be_BY", TXT_KEY_ANR_PROP_UNKNOWN, "(невядома)");

    registerEntry("be_BY", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Праграма <b>{app}</b> запытвае невядомы дазвол.");
    registerEntry("be_BY", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Праграма <b>{app}</b> спрабуе здымаць экран.\n\nЦі хочаце дазволіць?");
    registerEntry("be_BY", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Праграма <b>{app}</b> спрабуе загрузіць плагін: <b>{plugin}</b>.\n\nХочаце дазволіць?");
    registerEntry("be_BY", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Выяўленая новая клавіятура: <b>{keyboard}</b>.\n\nХочаце дазволіць яе выкарыстанне?");
    registerEntry("be_BY", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(невядома)");
    registerEntry("be_BY", TXT_KEY_PERMISSION_TITLE, "Запыт дазволу");
    registerEntry("be_BY", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Падказка: вы можаце задаць пастаянныя правілы для гэтага ў файле канфігурацыі Hyprland.");
    registerEntry("be_BY", TXT_KEY_PERMISSION_ALLOW, "Дазволіць");
    registerEntry("be_BY", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Дазволіць і запомніць");
    registerEntry("be_BY", TXT_KEY_PERMISSION_ALLOW_ONCE, "Дазволіць аднойчы");
    registerEntry("be_BY", TXT_KEY_PERMISSION_DENY, "Забараніць");
    registerEntry("be_BY", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Невядомая праграма (Ідэнтыфікатар кліента wayland {wayland_id})");

    registerEntry("be_BY", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Выглядае, што вашая пераменная асяроддзя XDG_CURRENT_DESKTOP зададзеная звонку, цяперашняе значэнне: {value}.\nГэта можа выклікаць праблемы, калі "
                  "гэта не зроблена наўмысна.");
    registerEntry("be_BY", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "У вашай сістэме не ўсталяваны hyprland-guiutils, што выкарыстоўваецца для некаторых дыялогавых вокнаў. Разгледзьце ўсталёўку пакета.");
    registerEntry("be_BY", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland не змог загрузіць {count} важны рэсурс, вінавацьце ў гэтым адказнага за зборку пакетаў для свайго дыстрыбутыва!";
        return "Hyprland не змог загрузіць {count} важных рэсурсаў, вінавацьце ў гэтым адказнага за зборку пакетаў для свайго дыстрыбутыва!";
    });
    registerEntry("be_BY", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Макет манітораў наладжаны некарэктна. Манітор {name} накладаецца на іншы(я) манітор(ы).\nДля падрабязнасцей звярніцеся да Wiki (Старонка Monitors). "
                  "Гэта <b>абавязкова</b> створыць праблемы.");
    registerEntry("be_BY", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Манітор {name} не змог наладзіць ніводны з запатрабаваных рэжымаў, аварыйна ўжыты рэжым {mode}.");
    registerEntry("be_BY", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Няверна зададзены маштаб для манітора {name}: {scale}, ужываецца прапанаваны маштаб: {fixed_scale}");
    registerEntry("be_BY", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Не атрымалася загрузіць плагін {name}: {error}");
    registerEntry("be_BY", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Не атрымалася перазагрузіць шэйдар CM, аварыйна ўжываецца rgba/rgbx.");
    registerEntry("be_BY", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Манітор {name}: пашыраны каляровы дыяпазон уключаны, але экран не ў рэжыме 10-біт.");

    // bn_BD (Bengali)
    registerEntry("bn_BD", TXT_KEY_ANR_TITLE, "অ্যাপ্লিকেশন সাড়া দিচ্ছে না");
    registerEntry("bn_BD", TXT_KEY_ANR_CONTENT, "অ্যাপ্লিকেশন {title} - {class} সাড়া দিচ্ছে না।\nআপনি এটি নিয়ে কি করতে চান?");
    registerEntry("bn_BD", TXT_KEY_ANR_OPTION_TERMINATE, "বন্ধ করুন");
    registerEntry("bn_BD", TXT_KEY_ANR_OPTION_WAIT, "অপেক্ষা করুন");
    registerEntry("bn_BD", TXT_KEY_ANR_PROP_UNKNOWN, "(অজানা)");

    registerEntry("bn_BD", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "একটি অ্যাপ্লিকেশন <b>{app}</b> একটি অজানা অনুমতির অনুরোধ করছে।");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "একটি অ্যাপ্লিকেশন <b>{app}</b> আপনার স্ক্রিন রেকর্ড করার চেষ্টা করছে।\n\nআপনি কি এটি অনুমতি দিতে চান?");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "একটি অ্যাপ্লিকেশন <b>{app}</b> একটি প্লাগইন লোড করার চেষ্টা করছে: <b>{plugin}</b>।\n\nআপনি কি এটি অনুমতি দিতে চান?");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "একটি নতুন কীবোর্ড সনাক্ত করা হয়েছে: <b>{keyboard}</b>।\n\nআপনি কি এটি কাজ করতে অনুমতি দিতে চান?");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(অজানা)");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_TITLE, "অনুমতির অনুরোধ");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "টিপ: আপনি Hyprland কনফিগারেশন ফাইলে এর জন্য স্থায়ী নিয়ম সেট করতে পারেন।");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_ALLOW, "অনুমতি দিন");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "অনুমতি দিন এবং মনে রাখুন");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_ALLOW_ONCE, "একবার অনুমতি দিন");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_DENY, "প্রত্যাখ্যান করুন");
    registerEntry("bn_BD", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "অজানা অ্যাপ্লিকেশন (wayland ক্লায়েন্ট ID {wayland_id})");

    registerEntry("bn_BD", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "আপনার XDG_CURRENT_DESKTOP পরিবেশ পরিবর্তনশীল বাহ্যিকভাবে পরিচালিত হচ্ছে বলে মনে হচ্ছে, বর্তমান মান: {value}।\nএটি সমস্যা সৃষ্টি করতে পারে যদি না এটি ইচ্ছাকৃত হয়।");
    registerEntry("bn_BD", TXT_KEY_NOTIF_NO_GUIUTILS, "আপনার সিস্টেমে hyprland-guiutils ইনস্টল নেই যা কিছু ডায়ালগের জন্য ব্যবহৃত হয়। এটি ইনস্টল করার কথা বিবেচনা করুন।");
    registerEntry("bn_BD", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland {count}টি প্রয়োজনীয় সম্পদ লোড করতে ব্যর্থ হয়েছে, খারাপ প্যাকেজিং কাজের জন্য আপনার ডিস্ট্রো প্যাকেজারদের দোষ দিন!";
        return "Hyprland {count}টি প্রয়োজনীয় সম্পদ লোড করতে ব্যর্থ হয়েছে, খারাপ প্যাকেজিং কাজের জন্য আপনার ডিস্ট্রো প্যাকেজারদের দোষ দিন!";
    });
    registerEntry("bn_BD", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "আপনার মনিটর লেআউট ভুলভাবে কনফিগার করা হয়েছে। মনিটর {name} লেআউটে অন্য মনিটর(গুলি) এর সাথে ওভারল্যাপ করছে।\nবিস্তারিত জানতে wiki (Monitors page) দেখুন। "
                  "এটি <b>অবশ্যই</b> সমস্যা সৃষ্টি করবে।");
    registerEntry("bn_BD", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "মনিটর {name} কোনো অনুরোধকৃত মোড সেট করতে পারেনি, মোড {mode} এ ফিরে যাচ্ছে।");
    registerEntry("bn_BD", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "মনিটর {name} এর জন্য অবৈধ স্কেল পাঠানো হয়েছে: {scale}, প্রস্তাবিত স্কেল ব্যবহার করা হচ্ছে: {fixed_scale}");
    registerEntry("bn_BD", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "প্লাগইন {name} লোড করতে ব্যর্থ: {error}");
    registerEntry("bn_BD", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM শেডার পুনরায় লোড করতে ব্যর্থ, rgba/rgbx এ ফিরে যাচ্ছে।");
    registerEntry("bn_BD", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "মনিটর {name}: ওয়াইড কালার গ্যামুট সক্রিয় কিন্তু স্ক্রিন 10-বিট মোডে নেই।");
    registerEntry("bn_BD", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland start-hyprland ছাড়া চালু করা হয়েছে। এটি অত্যন্ত সুপারিশকৃত নয় যদি না আপনি ডিবাগিং পরিবেশে থাকেন।");

    registerEntry("bn_BD", TXT_KEY_SAFE_MODE_TITLE, "নিরাপদ মোড");
    registerEntry("bn_BD", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland নিরাপদ মোডে চালু করা হয়েছে, যার মানে আপনার শেষ সেশন ক্র্যাশ হয়েছিল।\nনিরাপদ মোড আপনার কনফিগ লোড হওয়া থেকে প্রতিরোধ করে। আপনি "
                  "এই পরিবেশে সমস্যা সমাধান করতে পারেন, অথবা নিচের বাটন দিয়ে আপনার কনফিগ লোড করতে পারেন।\nডিফল্ট কীবাইন্ড প্রযোজ্য: kitty এর জন্য SUPER+Q, মৌলিক রানারের জন্য SUPER+R, "
                  "প্রস্থান করতে SUPER+M।\nHyprland পুনরায় চালু করলে আবার স্বাভাবিক মোডে চালু হবে।");
    registerEntry("bn_BD", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "কনফিগ লোড করুন");
    registerEntry("bn_BD", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "ক্র্যাশ রিপোর্ট ডিরেক্টরি খুলুন");
    registerEntry("bn_BD", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "ঠিক আছে, এটি বন্ধ করুন");

    // da_DK (Danish)
    registerEntry("da_DK", TXT_KEY_ANR_TITLE, "Applikationen Svarer Ikke");
    registerEntry("da_DK", TXT_KEY_ANR_CONTENT, "En applikation {title} - {class} svarer ikke.\nHvad vil du gøre ved det?");
    registerEntry("da_DK", TXT_KEY_ANR_OPTION_TERMINATE, "Luk");
    registerEntry("da_DK", TXT_KEY_ANR_OPTION_WAIT, "Vent");
    registerEntry("da_DK", TXT_KEY_ANR_PROP_UNKNOWN, "(ukendt)");

    registerEntry("da_DK", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "En applikation <b>{app}</b> forespørger en ukendt rettighed.");
    registerEntry("da_DK", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "En applikation <b>{app}</b> forsøger at optage din skærm.\n\nVil du tillade dette?");
    registerEntry("da_DK", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "En applikation <b>{app}</b> forsøger at indlæse et plugin: <b>{plugin}</b>.\n\nVil du tillade dette?");
    registerEntry("da_DK", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Et nyt tastatur er fundet: <b>{keyboard}</b>.\n\nVil du tillade den at fungere?");
    registerEntry("da_DK", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(ukendt)");
    registerEntry("da_DK", TXT_KEY_PERMISSION_TITLE, "Anmodning om tilladelse");
    registerEntry("da_DK", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tip: Du kan indstille vedvarende regler for disse i Hyprland-konfigurationsfilen.");
    registerEntry("da_DK", TXT_KEY_PERMISSION_ALLOW, "Tillad");
    registerEntry("da_DK", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Tillad og husk");
    registerEntry("da_DK", TXT_KEY_PERMISSION_ALLOW_ONCE, "Tillad én gang");
    registerEntry("da_DK", TXT_KEY_PERMISSION_DENY, "Nægt");
    registerEntry("da_DK", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Ukendt applikation (wayland client ID {wayland_id})");

    registerEntry(
        "da_DK", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Dit XDG_CURRENT_DESKTOP miljø ser ud til at være administreret externt, og den nuværende værdi er {value}.\nDette kan forårsage problemer, medmindre det er bevidst.");
    registerEntry("da_DK", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Dit system har ikke hyprland-guiutils installeret. Dette er en runtime-afhængighed for nogle dialoger. Overvej at installere den.");
    registerEntry("da_DK", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland kunne ikke indlæse {count} essentiale aktiver, skyd skylden på din distributions pakker for et dårligt stykke arbejde af pakningen!";
        return "Hyprland kunne ikke indlæse {count} essentiale aktiver, skyd skylden på din distributions pakker for et dårligt stykke arbejde af pakningen!";
    });
    registerEntry("da_DK", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Dit skærmlayout har en ukorrekt opsætning. Skærm {name} overlapper med andre skærm(e) i layoutet.\nLæs venligst wiki'en (Monitors page) for "
                  "mere. Dette <b>vil</b> skabe problemer.");
    registerEntry("da_DK", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Skærm {name} kunne ikke indlæse nogen af de ønskede tilstande, vender tilbage til tilstand {mode}.");
    registerEntry("da_DK", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Ugyldig skalering sendt til skærm {name}: {scale}, bruger foreslået skalering: {fixed_scale}");
    registerEntry("da_DK", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Kunne ikke indlæse plugin {name}: {error}");
    registerEntry("da_DK", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Genindlæsning af CM-shader mislykkedes, går tilbage til rgba/rgbx.");
    registerEntry("da_DK", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Skærm {name}: wide color gamut er aktiveret men skærmen er ikke i 10-bit tilstand.");

    // el_GR (Greek)
    registerEntry("el_GR", TXT_KEY_ANR_TITLE, "Η εφαρμογή δεν αποκρίνεται");
    registerEntry("el_GR", TXT_KEY_ANR_CONTENT, "Η εφαρμογή {title} - {class} δεν αποκρίνεται.\nΤι θέλετε να κάνετε;");
    registerEntry("el_GR", TXT_KEY_ANR_OPTION_TERMINATE, "Τερματισμός");
    registerEntry("el_GR", TXT_KEY_ANR_OPTION_WAIT, "Αναμονή");
    registerEntry("el_GR", TXT_KEY_ANR_PROP_UNKNOWN, "(άγνωστο)");

    registerEntry("el_GR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Μια εφαρμογή <b>{app}</b> ζητά μια άγνωστη άδεια.");
    registerEntry("el_GR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Μια εφαρμογή <b>{app}</b> προσπαθεί να καταγράψει την οθόνη σας.\n\nΘέλετε να το επιτρέψετε;");
    registerEntry("el_GR", TXT_KEY_PERMISSION_REQUEST_CURSOR_POS, "Μια εφαρμογή <b>{app}</b> προσπαθεί να καταγράψει τη θέση του δρομέα σας.\n\nΘέλετε να το επιτρέψετε;");
    registerEntry("el_GR", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Μια εφαρμογή <b>{app}</b> προσπαθεί να φορτώσει ένα πρόσθετο: <b>{plugin}</b>.\n\nΘέλετε να το επιτρέψετε;");
    registerEntry("el_GR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Εντοπίστηκε νέο πληκτρολόγιο: <b>{keyboard}</b>.\n\nΘέλετε να επιτρέψετε τη λειτουργία του;");
    registerEntry("el_GR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(άγνωστο)");
    registerEntry("el_GR", TXT_KEY_PERMISSION_TITLE, "Αίτημα άδειας");
    registerEntry("el_GR", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Συμβουλή: μπορείτε να ορίσετε μόνιμους κανόνες γι' αυτά στο αρχείο ρυθμίσεων του Hyprland.");
    registerEntry("el_GR", TXT_KEY_PERMISSION_ALLOW, "Αποδοχή");
    registerEntry("el_GR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Αποδοχή και απομνημόνευση");
    registerEntry("el_GR", TXT_KEY_PERMISSION_ALLOW_ONCE, "Αποδοχή μία φορά");
    registerEntry("el_GR", TXT_KEY_PERMISSION_DENY, "Απόρριψη");
    registerEntry("el_GR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Άγνωστη εφαρμογή (αναγνωριστικό wayland πελάτη {wayland_id})");

    registerEntry("el_GR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Η μεταβλητή περιβάλλοντος XDG_CURRENT_DESKTOP φαίνεται να διαχειρίζεται εξωτερικά, με τρέχουσα τιμή: {value}.\nΑυτό μπορεί να προκαλέσει προβλήματα "
                  "αν δεν είναι σκόπιμο.");
    registerEntry("el_GR", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Το σύστημά σας δεν έχει εγκατεστημένο το hyprland-guiutils, το οποίο χρησιμοποιείται για ορισμένα παράθυρα διαλόγου. Σκεφτείτε να το εγκαταστήσετε.");
    registerEntry("el_GR", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Το Hyprland απέτυχε να φορτώσει {count} απαραίτητο πόρο, φταίει ο συντηρητής πακέτων της διανομής σας!";
        return "Το Hyprland απέτυχε να φορτώσει {count} απαραίτητους πόρους, φταίει ο συντηρητής πακέτων της διανομής σας!";
    });
    registerEntry("el_GR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Η διάταξη των οθονών σας είναι εσφαλμένη. Η οθόνη {name} επικαλύπτεται με άλλη(ες) οθόνη(ες) στη διάταξη.\nΔείτε το wiki (σελίδα Monitors) για "
                  "περισσότερα. Αυτό <b>θα</b> προκαλέσει προβλήματα.");
    registerEntry("el_GR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Η οθόνη {name} απέτυχε να ορίσει οποιαδήποτε ζητούμενη λειτουργία, επιστροφή στη λειτουργία {mode}.");
    registerEntry("el_GR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Μη έγκυρη κλίμακα για την οθόνη {name}: {scale}, χρησιμοποιείται η προτεινόμενη κλίμακα: {fixed_scale}");
    registerEntry("el_GR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Αποτυχία φόρτωσης πρόσθετου {name}: {error}");
    registerEntry("el_GR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Η επαναφόρτωση του CM shader απέτυχε, επιστροφή σε rgba/rgbx.");
    registerEntry("el_GR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Οθόνη {name}: η ευρεία γκάμα χρωμάτων είναι ενεργοποιημένη αλλά η οθόνη δεν βρίσκεται σε λειτουργία 10-bit.");
    registerEntry("el_GR", TXT_KEY_NOTIF_NO_WATCHDOG, "Το Hyprland εκκινήθηκε χωρίς το start-hyprland. Αυτό δεν συνιστάται εκτός αν βρίσκεστε σε περιβάλλον αποσφαλμάτωσης.");

    registerEntry("el_GR", TXT_KEY_SAFE_MODE_TITLE, "Ασφαλής λειτουργία");
    registerEntry("el_GR", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Το Hyprland εκκινήθηκε σε ασφαλή λειτουργία, που σημαίνει ότι η τελευταία σας συνεδρία κατέρρευσε.\nΗ ασφαλής λειτουργία αποτρέπει τη φόρτωση "
                  "των ρυθμίσεών σας. Μπορείτε να αντιμετωπίσετε προβλήματα σε αυτό το περιβάλλον ή να φορτώσετε τις ρυθμίσεις σας με το παρακάτω κουμπί.\nΙσχύουν "
                  "οι προεπιλεγμένες συντομεύσεις: SUPER+Q για kitty, SUPER+R για βασικό εκκινητή, SUPER+M για έξοδο.\nΗ επανεκκίνηση του Hyprland θα γίνει σε "
                  "κανονική λειτουργία.");
    registerEntry("el_GR", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Φόρτωση ρυθμίσεων");
    registerEntry("el_GR", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Άνοιγμα φακέλου αναφορών κατάρρευσης");
    registerEntry("el_GR", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "Εντάξει, κλείσιμο");

    // en_US (English)
    registerEntry("en_US", TXT_KEY_ANR_TITLE, "Application Not Responding");
    registerEntry("en_US", TXT_KEY_ANR_CONTENT, "An application {title} - {class} is not responding.\nWhat do you want to do with it?");
    registerEntry("en_US", TXT_KEY_ANR_OPTION_TERMINATE, "Terminate");
    registerEntry("en_US", TXT_KEY_ANR_OPTION_WAIT, "Wait");
    registerEntry("en_US", TXT_KEY_ANR_PROP_UNKNOWN, "(unknown)");

    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "An application <b>{app}</b> is requesting an unknown permission.");
    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "An application <b>{app}</b> is trying to capture your screen.\n\nDo you want to allow it to?");
    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_CURSOR_POS, "An application <b>{app}</b> is trying to capture your cursor position.\n\nDo you want to allow it to?");
    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "An application <b>{app}</b> is trying to load a plugin: <b>{plugin}</b>.\n\nDo you want to allow it to?");
    registerEntry("en_US", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "A new keyboard has been detected: <b>{keyboard}</b>.\n\nDo you want to allow it to operate?");
    registerEntry("en_US", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(unknown)");
    registerEntry("en_US", TXT_KEY_PERMISSION_TITLE, "Permission request");
    registerEntry("en_US", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Hint: you can set persistent rules for these in the Hyprland config file.");
    registerEntry("en_US", TXT_KEY_PERMISSION_ALLOW, "Allow");
    registerEntry("en_US", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Allow and remember");
    registerEntry("en_US", TXT_KEY_PERMISSION_ALLOW_ONCE, "Allow once");
    registerEntry("en_US", TXT_KEY_PERMISSION_DENY, "Deny");
    registerEntry("en_US", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Unknown application (wayland client ID {wayland_id})");

    registerEntry("en_US", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Your XDG_CURRENT_DESKTOP environment seems to be managed externally, and the current value is {value}.\nThis might cause issues unless it's intentional.");
    registerEntry("en_US", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Your system does not have hyprland-guiutils installed. This is a runtime dependency for some dialogs. Consider installing it.");
    registerEntry("en_US", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland failed to load {count} essential asset, blame your distro's packager for doing a bad job at packaging!";
        return "Hyprland failed to load {count} essential assets, blame your distro's packager for doing a bad job at packaging!";
    });
    registerEntry("en_US", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Your monitor layout is set up incorrectly. Monitor {name} overlaps with other monitor(s) in the layout.\nPlease see the wiki (Monitors page) for "
                  "more. This <b>will</b> cause issues.");
    registerEntry("en_US", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} failed to set any requested modes, falling back to mode {mode}.");
    registerEntry("en_US", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Invalid scale passed to monitor {name}: {scale}, using suggested scale: {fixed_scale}");
    registerEntry("en_US", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Failed to load plugin {name}: {error}");
    registerEntry("en_US", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader reload failed, falling back to rgba/rgbx.");
    registerEntry("en_US", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: wide color gamut is enabled but the display is not in 10-bit mode.");
    registerEntry("en_US", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland was started without start-hyprland. This is strongly discouraged unless you are in a debugging environment.");

    registerEntry("en_US", TXT_KEY_SAFE_MODE_TITLE, "Safe Mode");
    registerEntry("en_US", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland has been launched in safe mode, which means your last session crashed.\nSafe mode prevents your config from being loaded. You can "
                  "troubleshoot in this environment, or load your config with the button below.\nDefault keybinds apply: SUPER+Q for kitty, SUPER+R for a basic runner, "
                  "SUPER+M to exit.\nRestarting "
                  "Hyprland will launch in normal mode again.");
    registerEntry("en_US", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Load config");
    registerEntry("en_US", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Open crash report directory");
    registerEntry("en_US", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "Ok, close this");

    // as_IN (Assamese)
    registerEntry("as_IN", TXT_KEY_ANR_TITLE, "এপ্লিকেচনে উত্তৰ দিয়া নাই");
    registerEntry("as_IN", TXT_KEY_ANR_CONTENT, "এপ্লিকেচন {title} - {class}-এ উত্তৰ দিয়া নাই।\nআপুনি এয়াৰ লগত কি কৰিব বিচাৰে?");
    registerEntry("as_IN", TXT_KEY_ANR_OPTION_TERMINATE, "সমাপ্ত কৰক");
    registerEntry("as_IN", TXT_KEY_ANR_OPTION_WAIT, "অপেক্ষা কৰক");
    registerEntry("as_IN", TXT_KEY_ANR_PROP_UNKNOWN, "(অজ্ঞাত)");

    registerEntry("as_IN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "এপ্লিকেচন <b>{app}</b>-এ এটা অজ্ঞাত অনুমতি বিচাৰিছে।");
    registerEntry("as_IN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "এটা এপ্লিকেচন <b>{app}</b>-এ আপোনাৰ স্ক্ৰীণ কেপচাৰ কৰিবলৈ চেষ্টা কৰিছে।\n\nআপুনি ইয়াক অনুমতি দিব বিচাৰেনে?");
    registerEntry("as_IN", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "এপ্লিকেচন <b>{app}</b>-এ এটা প্লাগিন লোড কৰিবলৈ চেষ্টা কৰিছে: <b>{plugin}</b>।\n\nআপুনি ইয়াক অনুমতি দিব বিচাৰেনে?");
    registerEntry("as_IN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "এটা নতুন কিবৰ্ড ধৰা পৰিছে: <b>{keyboard}</b>।\n\nআপুনি ইয়াক চলাবলৈ অনুমতি দিব বিচাৰেনে?");
    registerEntry("as_IN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(অজ্ঞাত)");
    registerEntry("as_IN", TXT_KEY_PERMISSION_TITLE, "অনুমতিৰ অনুৰোধ");
    registerEntry("as_IN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "ইঙ্গিত: আপুনি হাইপাৰলেণ্ড কনফিগ ফাইলত এইবোৰৰ বাবে স্থায়ী নিয়ম স্থাপন কৰিব পাৰে।");
    registerEntry("as_IN", TXT_KEY_PERMISSION_ALLOW, "অনুমতি দিয়ক");
    registerEntry("as_IN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "অনুমতি দি মনত ৰাখক");
    registerEntry("as_IN", TXT_KEY_PERMISSION_ALLOW_ONCE, "এবাৰ অনুমতি দিয়ক");
    registerEntry("as_IN", TXT_KEY_PERMISSION_DENY, "অস্বীকাৰ কৰক");
    registerEntry("as_IN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "অজ্ঞাত এপ্লিকেচন (ৱেইলেণ্ড ক্লায়েণ্ট আইডি {wayland_id})");

    registerEntry("as_IN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "আপোনাৰ XDG_CURRENT_DESKTOP পৰিৱেশটো বাহ্যিকভাৱে পৰিচালিত হোৱা যেন লাগিছে, আৰু বৰ্তমানৰ মান হৈছে {value}।\nযদি ই ইচ্ছাকৃতভাৱে নহয়, তেনে হলে সমস্যাৰ সৃষ্টি হ'ব পাৰে।");
    registerEntry("as_IN", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "আপোনাৰ চিষ্টেমত hyprland-guiutils ইনষ্টল কৰা নাই। কিছুমান ডাইলগৰ বাবে ই এটা ৰানটাইম নিৰ্ভৰশীলতা। ইয়াক ইনষ্টল কৰাৰ কথা চিন্তা কৰক।");
    registerEntry("as_IN", TXT_KEY_NOTIF_FAILED_ASSETS, "হাইপাৰলেণ্ড {count}-টা প্ৰয়োজনীয় সম্পদ লোড কৰাত অসফল হৈছে, বেয়া পেকজিং কৰাৰ বাবে আপোনাৰ ডিষ্ট্ৰ'ৰ পেকেজাৰক দোষাৰোপ কৰক!");
    registerEntry("as_IN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "আপোনাৰ মনিটৰৰ লেআউট ভুলকৈ ছেট কৰা হৈছে। মনিটৰ {name} লেআউটত আন মনিটৰ(সমূহ)ৰ সৈতে ওপৰা-উপৰি হৈ আছে।\nঅধিক তথ্যৰ বাবে অনুগ্ৰহ কৰি ৱিকি (মনিটৰ পৃষ্ঠা) চাওক। ই "
                  "<b>সমস্যাৰ</b> সৃষ্টি কৰিব।");
    registerEntry("as_IN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "মনিটৰ {name}-এ কোনো অনুৰোধ কৰা মোড ছেট কৰাত অসফল হৈছে, মোড {mode}-লৈ ঘূৰি আহিছে।");
    registerEntry("as_IN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "মনিটৰ {name}: {scale}-লৈ অবৈধ মাপন দিয়া হৈছে, পৰামৰ্শ দিয়া মাপন ব্যৱহাৰ কৰা যাব: {fixed_scale}");
    registerEntry("as_IN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "প্লাগিন {name} লোড কৰাত অসফল হৈছে: {error}");
    registerEntry("as_IN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM শ্বেডাৰ ৰিলোড কৰাত অসফল হৈছে, rgba/rgbx-লৈ ঘূৰি আহিছে।");
    registerEntry("as_IN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "প্ৰসাৰিত ৰঙৰ বৰ্গ সক্ষম কৰা হৈছে কিন্তু ডিচপ্লে 10-বিট মোডত নাই।");

    // de_DE (German)
    registerEntry("de_DE", TXT_KEY_ANR_TITLE, "Anwendung Reagiert Nicht");
    registerEntry("de_DE", TXT_KEY_ANR_CONTENT, "Eine Anwendung {title} - {class} reagiert nicht.\nWas möchten Sie damit tun?");
    registerEntry("de_DE", TXT_KEY_ANR_OPTION_TERMINATE, "Beenden");
    registerEntry("de_DE", TXT_KEY_ANR_OPTION_WAIT, "Warten");
    registerEntry("de_DE", TXT_KEY_ANR_PROP_UNKNOWN, "(unbekannt)");

    registerEntry("de_DE", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Eine Anwendung <b>{app}</b> fordert eine unbekannte Berechtigung an.");
    registerEntry("de_DE", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Eine Anwendung <b>{app}</b> versucht Ihren Bildschrim aufzunehmen.\n\nMöchten Sie dies erlauben?");
    registerEntry("de_DE", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Eine Anwendung <b>{app}</b> versucht ein Plugin zu laden: <b>{plugin}</b>.\n\nMöchten Sie dies erlauben?");
    registerEntry("de_DE", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Eine neue Tastatur wurde erkannt: <b>{keyboard}</b>.\n\nMöchten Sie diese in Betrieb nehmen?");
    registerEntry("de_DE", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(unbekannt)");
    registerEntry("de_DE", TXT_KEY_PERMISSION_TITLE, "Berechtigungsanfrage");
    registerEntry("de_DE", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tip: Sie können dafür permanente Regeln in der Hyprland-Konfigurationsdatei festlegen.");
    registerEntry("de_DE", TXT_KEY_PERMISSION_ALLOW, "Erlauben");
    registerEntry("de_DE", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Erlauben und merken");
    registerEntry("de_DE", TXT_KEY_PERMISSION_ALLOW_ONCE, "Einmal erlauben");
    registerEntry("de_DE", TXT_KEY_PERMISSION_DENY, "Verweigern");
    registerEntry("de_DE", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Unbekannte Anwendung (wayland client ID {wayland_id})");

    registerEntry("de_DE", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Ihre XDG_CURRENT_DESKTOP umgebung scheint extern gemanagt zu werden, und der aktuelle Wert ist {value}.\nDies könnte zu Problemen führen sofern es "
                  "nicht absichtlich so ist.");
    registerEntry("de_DE", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Ihr System hat hyprland-guiutils nicht installiert. Dies ist eine Laufzeitabhängigkeit für einige Dialoge. Es ist empfohlen diese zu installieren.");
    registerEntry("de_DE", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland konnte {count} essentielle Ressource nicht laden, geben Sie dem Packager ihrer Distribution die Schuld für ein schlechtes Package!";
        return "Hyprland konnte {count} essentielle Ressroucen nicht laden, geben Sie dem Packager ihrer Distribution die Schuld für ein schlechtes Package!";
    });
    registerEntry("de_DE", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Ihr Bildschirmlayout ist fehlerhaft aufgesetzt. Der Bildschirm {name} überlappt mit anderen Bildschirm(en) im Layout.\nBitte siehe im Wiki (Monitors Seite) für "
                  "mehr Informationen. Dies <b>wird</b> zu Problemen führen.");
    registerEntry("de_DE", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Bildschirm {name} konnte keinen der angeforderten Modi setzen fällt auf den Modus {mode} zurück.");
    registerEntry("de_DE", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Ungültiger Skalierungsfaktor {scale} für Bildschirm {name}, es wird der empfohlene Faktor {fixed_scale} verwendet.");
    registerEntry("de_DE", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Plugin {name} konnte nicht geladen werden: {error}");
    registerEntry("de_DE", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader konnte nicht neu geladen werden und es wird auf rgba/rgbx zurückgefallen.");
    registerEntry("de_DE", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Bildschirm {name}: wide color gamut ist aktiviert aber der Bildschirm ist nicht im 10-bit Modus.");

    // de_CH (Swiss German)
    registerEntry("de_CH", TXT_KEY_ANR_TITLE, "Aawändig Reagiert Ned");
    registerEntry("de_CH", TXT_KEY_ANR_CONTENT, "En Aawändig {title} - {class} reagiert ned.\nWas wend Sie demet mache?");
    registerEntry("de_CH", TXT_KEY_ANR_OPTION_TERMINATE, "Beände");
    registerEntry("de_CH", TXT_KEY_ANR_OPTION_WAIT, "Warte");
    registerEntry("de_CH", TXT_KEY_ANR_PROP_UNKNOWN, "(onbekannt)");

    registerEntry("de_CH", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "En Aawändig <b>{app}</b> fordert en onbekannti Berächtigong aa.");
    registerEntry("de_CH", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "En Aawändig <b>{app}</b> versuecht Ehre Beldscherm uufznäh.\n\nWend Sie das erlaube?");
    registerEntry("de_CH", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "En Aawändig <b>{app}</b> versuecht es Plugin z'lade: <b>{plugin}</b>.\n\nWend Sie das erlaube?");
    registerEntry("de_CH", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "En neui Tastatur esch erkönne worde: <b>{keyboard}</b>.\n\nWend sie die in Betreb nä?");
    registerEntry("de_CH", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(onbekannt)");
    registerEntry("de_CH", TXT_KEY_PERMISSION_TITLE, "Berächtigongsaafrog");
    registerEntry("de_CH", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tip: Sie chönd permanenti Regle deför i ehrere Hyprland-Konfigurationsdatei festlegge.");
    registerEntry("de_CH", TXT_KEY_PERMISSION_ALLOW, "Erlaube");
    registerEntry("de_CH", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Erlaube ond merke");
    registerEntry("de_CH", TXT_KEY_PERMISSION_ALLOW_ONCE, "Einisch erlaube");
    registerEntry("de_CH", TXT_KEY_PERMISSION_DENY, "Verweigere");
    registerEntry("de_CH", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Onbekannti Aawändig (wayland client ID {wayland_id})");

    registerEntry(
        "de_CH", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Ehri XDG_CURRENT_DESKTOP omgäbig schiint extern gmanagt z'wärde, ond de aktuelli Wärt esch {value}.\nDas chönnt zo Problem füehre sofärn das ned absechtlech so esch.");
    registerEntry("de_CH", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Ehres System hed hyprland-guiutils ned installiert. Das esch en Laufziitabhängigkeit för es paar Dialog. Es werd empfohle sie z'installiere.");
    registerEntry("de_CH", TXT_KEY_NOTIF_FAILED_ASSETS,
                  "Hyprland hed {count} essentielli Ressource ned chönne lade, gäbed Sie im Packager vo ehrere Distribution schold för es schlächts Package!");
    registerEntry("de_CH", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Ehres Beldschermlayout esch fählerhaft uufgsetzt. De Beldscherm {name} öberlappt met andere Beldscherm(e) im Layout.\nBitte lueged sie im Wiki "
                  "(Monitors Siite) för meh Informatione. Das <b>werd</b> zo Problem füehre.");
    registerEntry("de_CH", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "De Beldscherm {name} hed keine vode aagforderete Modi chönne setze, ond fallt uf de Modus {mode} zrogg.");
    registerEntry("de_CH", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Ongöltige Skalierigsfaktor {scale} för de Beldscherm {name}, es werd de empfohleni Faktor {fixed_scale} verwändet.");
    registerEntry("de_CH", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "S Plugin {name} hed ned chönne glade wärde: {error}");
    registerEntry("de_CH", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader hed ned chönne neu glade wärde, es werd uf rgba/rgbx zrogggfalle.");
    registerEntry("de_CH", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Beldscherm {name}: wide color gamut esch aktiviert aber de Beldscherm esch ned im 10-bit Modus.");

    // pt_BR (Brazilian Portuguese)
    registerEntry("pt_BR", TXT_KEY_ANR_TITLE, "O aplicativo não está respondendo");
    registerEntry("pt_BR", TXT_KEY_ANR_CONTENT, "O aplicativo {title} - {class} não está respondendo.\nO que você deseja fazer?");
    registerEntry("pt_BR", TXT_KEY_ANR_OPTION_TERMINATE, "Encerrar");
    registerEntry("pt_BR", TXT_KEY_ANR_OPTION_WAIT, "Esperar");
    registerEntry("pt_BR", TXT_KEY_ANR_PROP_UNKNOWN, "(Desconhecido)");

    registerEntry("pt_BR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "O aplicativo <b>{app}</b> está pedindo uma permissão desconhecida.");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "O aplicativo <b>{app}</b> está tentando capturar sua tela.\n\nVocê deseja permitir?");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_REQUEST_CURSOR_POS, "O aplicativo <b>{app}</b> está tentando capturar a posição de seu cursor. \n\nVocê deseja permitir?");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "O aplicativo <b>{app}</b> está tentando carregar um plugin: <b>{plugin}</b>.\n\nVocê deseja permitir?");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Um novo teclado foi detectado: <b>{keyboard}</b>.\n\nVocê deseja permitir seu uso?");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(Desconhecido)");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_TITLE, "Solicitação de permissão");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Dica: você pode definir regras persistentes para essas permissões no arquivo de configuração do Hyprland");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_ALLOW, "Permitir");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Permitir e lembrar");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_ALLOW_ONCE, "Permitir uma vez");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_DENY, "Negar");
    registerEntry("pt_BR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Aplicativo desconhecido (wayland client ID {wayland_id})");

    registerEntry("pt_BR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Seu XDG_CURRENT_DESKTOP parece estar sendo gerenciado externamente, e atualmente é {value}.\nIsso pode causar problemas caso não seja intencional.");
    registerEntry("pt_BR", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Seu sistema não possui hyprland-guiutils instalado. Essa é uma dependência de execução para alguns diálogos. Considere instalá-lo.");
    registerEntry("pt_BR", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "O Hyprland falhou ao carregar {count} recurso essencial, culpe o empacotador da sua distro por fazer um péssimo trabalho!";
        return "O Hyprland falhou ao carregar {count} recursos essenciais, culpe o empacotador da sua distro por fazer um péssimo trabalho!";
    });
    registerEntry("pt_BR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Sua disposição de monitores está configurada incorretamente. O monitor {name} se sobrepõe a outro(s) monitor(es) na disposição.\nPor favor consulte "
                  "a wiki (Monitors page) para "
                  "mais informações. Isso <b>vai</b> causar problemas.");
    registerEntry("pt_BR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "O monitor {name} falhou em definir qualquer um dos modos solicitados, voltando ao modo {mode}.");
    registerEntry("pt_BR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Um fator de escala inválido foi passado para o monitor {name}: {scale}, usando o fator sugerido: {fixed_scale}");
    registerEntry("pt_BR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Falha ao carregar o plugin {name}: {error}");
    registerEntry("pt_BR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Falha ao carregar o shader CM, voltando para rgba/rgbx.");
    registerEntry("pt_BR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: o modo de gama de cores amplo está ativado, mas a tela não está configurada para 10 bits.");
    registerEntry("pt_BR", TXT_KEY_NOTIF_NO_WATCHDOG,
                  "Hyprland foi iniciado sem o comando start-hyprland. Isso é altamente desaconselhável a menos que você esteja em um ambiente de depuração.");
    registerEntry("pt_BR", TXT_KEY_SAFE_MODE_TITLE, "Modo seguro");
    registerEntry("pt_BR", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland foi iniciado em modo seguro, o que significa que a sua última sessão falhou.\nO modo seguro impede que suas configurações sejam carregadas. Você pode "
                  "solucionar esse problema neste ambiente ou carregar suas configurações com o botão abaixo. \nEsse ambiente usa os atalhos padrão: SUPER+Q para abrir kitty, "
                  "SUPER+R para abrir o inicializador básico e SUPER+M para sair do Hyprland. \nReiniciar "
                  "o Hyprland fará com que ele seja iniciado no modo normal novamente.");
    registerEntry("pt_BR", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Carregar configurações");
    registerEntry("pt_BR", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Abrir diretório de relatórios de falhas");
    registerEntry("pt_BR", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "Entendi, fechar");

    // es (Spanish)
    registerEntry("es", TXT_KEY_ANR_TITLE, "La aplicación no responde");
    registerEntry("es", TXT_KEY_ANR_CONTENT, "La aplicación {title} - {class} no responde.\n¿Qué deseas hacer?");
    registerEntry("es", TXT_KEY_ANR_OPTION_TERMINATE, "Forzar cierre");
    registerEntry("es", TXT_KEY_ANR_OPTION_WAIT, "Esperar");
    registerEntry("es", TXT_KEY_ANR_PROP_UNKNOWN, "(desconocido)");

    registerEntry("es", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Una aplicación <b>{app}</b> está solicitando un permiso desconocido.");
    registerEntry("es", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Una aplicación <b>{app}</b> está intentando capturar la pantalla.\n\n¿Deseas permitirlo?");
    registerEntry("es", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Una aplicación <b>{app}</b> está intentando cargar un plugin: <b>{plugin}</b>.\n\n¿Deseas permitirlo?");
    registerEntry("es", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Se ha detectado un nuevo teclado: <b>{keyboard}</b>.\n\n¿Deseas permitir su uso?");
    registerEntry("es", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(desconocido)");
    registerEntry("es", TXT_KEY_PERMISSION_TITLE, "Solicitud de permiso");
    registerEntry("es", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Sugerencia: puedes establecer reglas persistentes para estos permisos en el archivo de configuración de Hyprland.");
    registerEntry("es", TXT_KEY_PERMISSION_ALLOW, "Permitir");
    registerEntry("es", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Permitir y recordar");
    registerEntry("es", TXT_KEY_PERMISSION_ALLOW_ONCE, "Permitir una vez");
    registerEntry("es", TXT_KEY_PERMISSION_DENY, "Denegar");
    registerEntry("es", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Aplicación desconocida (ID de cliente de Wayland: {wayland_id})");

    registerEntry(
        "es", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "La variable de entorno XDG_CURRENT_DESKTOP parece gestionarse externamente; su valor actual es {value}.\nEsto podría causar problemas a menos que sea intencional.");
    registerEntry("es", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Tu sistema no tiene instalado 'hyprland-guiutils'. Es una dependencia en tiempo de ejecución para algunos diálogos. Considera instalarlo.");
    registerEntry("es", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "No se pudo cargar {count} recurso esencial. Contacta al empaquetador de tu distribución.";
        return "No se pudieron cargar {count} recursos esenciales. Contacta al empaquetador de tu distribución.";
    });
    registerEntry("es", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "La configuración de tus monitores no es correcta. El monitor {name} se superpone con otros monitores en la disposición. Consulta la wiki (página "
                  "Monitors, en inglés) para más información. Esto <b>provocará</b> problemas.");
    registerEntry("es", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "El monitor {name} no pudo configurar ninguno de los modos solicitados y ha vuelto al modo {mode}.");
    registerEntry("es", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Se pasó una escala no válida al monitor {name}: {scale}; se usará la escala sugerida: {fixed_scale}");
    registerEntry("es", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Error al cargar el plugin {name}: {error}");
    registerEntry("es", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Error al recargar el shader CM; volviendo a rgba/rgbx.");
    registerEntry("es", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: la gama de color amplia está habilitada, pero la pantalla no está en modo de 10 bits.");

    // fa_IR (Persian)
    registerEntry("fa_IR", TXT_KEY_ANR_TITLE, "برنامه پاسخ نمی‌دهد");
    registerEntry("fa_IR", TXT_KEY_ANR_CONTENT, "برنامه {title} - {class} پاسخی نمی‌دهد.\nمی‌خواهید چه کاری انجام شود؟");
    registerEntry("fa_IR", TXT_KEY_ANR_OPTION_TERMINATE, "بستن برنامه");
    registerEntry("fa_IR", TXT_KEY_ANR_OPTION_WAIT, "صبر کنید");
    registerEntry("fa_IR", TXT_KEY_ANR_PROP_UNKNOWN, "(نامشخص)");

    registerEntry("fa_IR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "برنامه <b>{app}</b> در حال درخواست یک مجوز ناشناخته است.");

    registerEntry("fa_IR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY,
                  "برنامه <b>{app}</b> می‌خواهد صفحه‌نمایش شما را ضبط کند.\n\nآیا اجازه می‌دهید؟");

    registerEntry(
        "fa_IR", TXT_KEY_PERMISSION_REQUEST_PLUGIN,
        "برنامه <b>{app}</b> می‌خواهد پلاگین <b>{plugin}</b> را بارگذاری کند.\n\nآیا اجازه می‌دهید پلاگین بارگذاری "
        "شود؟");

    registerEntry("fa_IR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD,
                  "یک کیبورد جدید شناسایی شد: <b>{keyboard}</b>.\n\nآیا اجازه استفاده از آن را صادر می‌کنید؟");

    registerEntry("fa_IR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(نامشخص)");
    registerEntry("fa_IR", TXT_KEY_PERMISSION_TITLE, "درخواست مجوز");

    registerEntry("fa_IR", TXT_KEY_PERMISSION_PERSISTENCE_HINT,
                  "نکته: می‌توانید قوانین دائمی مرتبط را در فایل تنظیمات هایپرلند تعریف کنید.");

    registerEntry("fa_IR", TXT_KEY_PERMISSION_ALLOW, "اجازه");
    registerEntry("fa_IR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "اجازه و ذخیره");
    registerEntry("fa_IR", TXT_KEY_PERMISSION_ALLOW_ONCE, "اجازه یک‌بار");
    registerEntry("fa_IR", TXT_KEY_PERMISSION_DENY, "عدم اجازه");

    registerEntry("fa_IR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "برنامه ناشناخته (شناسه Wayland: {wayland_id})");

    registerEntry("fa_IR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "متغیر XDG_CURRENT_DESKTOP توسط محیطی خارجی تنظیم شده است و مقدار فعلی آن {value} است.\n"
                  "اگر این کار عمدی نباشد ممکن است باعث ایجاد مشکل شود.");

    registerEntry(
        "fa_IR", TXT_KEY_NOTIF_NO_GUIUTILS,
        "بستهٔ hyprland-guiutils در سیستم نصب نیست. این بسته برای برخی از پنجره‌ها و دیالوگ‌ها لازم است. نصب "
        "آن "
        "پیشنهاد "
        "می‌شود.");

    registerEntry("fa_IR", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "هایپرلند نتوانست یک فایل ضروری را بارگذاری کند؛ ممکن است بسته‌بندی توزیع مشکل داشته "
                   "باشد.";
        return "هایپرلند نتوانست {count} فایل ضروری را بارگذاری کند؛ ممکن است بسته‌بندی توزیع مشکل داشته "
               "باشد.";
    });

    registerEntry("fa_IR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "چیدمان مانیتورها صحیح نیست. مانیتور {name} با یک یا چند مانیتور دیگر تداخل دارد.\n"
                  "برای اطلاعات بیشتر به صفحهٔ مانیتورها در ویکی مراجعه کنید. این موضوع <b>حتماً</b> باعث مشکل "
                  "می‌شود.");

    registerEntry("fa_IR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL,
                  "مانیتور {name} نتوانست هیچ‌کدام از حالت‌های درخواستی را اعمال کند؛ بازگشت به حالت {mode}.");

    registerEntry("fa_IR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "مقیاس واردشده برای مانیتور {name} نامعتبر است: {scale}. مقیاس پیشنهادی اعمال شد: {fixed_scale}");

    registerEntry("fa_IR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "بارگذاری پلاگین {name} با خطا روبه‌رو شد: {error}");

    registerEntry("fa_IR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "بارگذاری دوبارهٔ شیدر CM ناموفق بود؛ از حالت rgba/rgbx استفاده شد.");

    registerEntry("fa_IR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "مانیتور {name}: گسترهٔ رنگ وسیع فعال است اما نمایشگر در حالت ۱۰ بیتی نیست.");

    // fi_FI (Finnish)
    registerEntry("fi_FI", TXT_KEY_ANR_TITLE, "Sovellus ei vastaa");
    registerEntry("fi_FI", TXT_KEY_ANR_CONTENT, "Sovellus {title} - {class} ei vastaa.\nMitä haluat tehdä sille?");
    registerEntry("fi_FI", TXT_KEY_ANR_OPTION_TERMINATE, "Lopeta");
    registerEntry("fi_FI", TXT_KEY_ANR_OPTION_WAIT, "Odota");
    registerEntry("fi_FI", TXT_KEY_ANR_PROP_UNKNOWN, "(tuntematon)");

    registerEntry("fi_FI", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Sovellus <b>{app}</b> pyytää tuntematonta käyttöoikeutta.");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Sovellus <b>{app}</b> yrittää nauhoittaa näyttöäsi.\n\nHaluatko sallia nauhoituksen?");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Sovellus <b>{app}</b> yrittää ladata laajennusta: <b>{plugin}</b>.\n\nHaluatko sallia latauksen?");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Uusi näppäimistö havaittu: <b>{keyboard}</b>.\n\nHaluatko sallia sen toiminnan?");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(tuntematon)");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_TITLE, "Käyttöoikeuspyyntö");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Vihje: voit asettaa nämä säännöt pysyvästi Hyprland konfiguraatio tiedostossa.");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_ALLOW, "Salli");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Salli ja muista");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_ALLOW_ONCE, "Salli kerran");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_DENY, "Kiellä");
    registerEntry("fi_FI", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Tuntematon sovellus (wayland client ID {wayland_id})");

    registerEntry("fi_FI", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "XDG_CURRENT_DESKTOP ympäristösi näyttäisi olevan ulkoisesti hallittu, ja sen nykyinen arvo on {value}.\nTämä voi aiheuttaa ongelmia, jos sitä ei ole "
                  "tehty tarkoituksella.");
    registerEntry("fi_FI", TXT_KEY_NOTIF_NO_GUIUTILS, "Paketti hyprland-guiutils ei ole asennettuna järjestelmääsi. Jotkin dialogit tarvitsevat sitä. Harkitse sen asentamista.");
    registerEntry("fi_FI", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland epäonnistui olennaisen resurssin ({count}) latauksessa. Tämä johtuu todennäköisesti jakelusi virheellisestä pakkauksesta.";
        return "Hyprland epäonnistui olennaisten resurssien ({count}) latauksessa. Tämä johtuu todennäköisesti jakelusi virheellisestä pakkauksesta.";
    });
    registerEntry(
        "fi_FI", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Näyttöjesi asettelu on virheellinen. Näyttö {name} on muiden näyttöjen päällä.\nLisätietoja löydät wikistä (Monitors sivu). Tämä <b>tulee aiheuttamaan</b> ongelmia.");
    registerEntry("fi_FI", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Näyttö {name} epäonnistui pyydetyn tilan asettamisessa, palataan tilaan {mode}.");
    registerEntry("fi_FI", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Näytölle {name} asetettu skaalaus: {scale} on virheellinen, asetetaan suositeltu skaalaus: {fixed_scale}.");
    registerEntry("fi_FI", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Laajennuksen {name} lataus epäonnistui: {error}");
    registerEntry("fi_FI", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM varjostimen uudelleenlataus epäonnistui, palataan takaisin rgba/rgbx tilaan.");
    registerEntry("fi_FI", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Näyttö {name}: laaja väriskaala on otettu käyttöön, mutta näyttö ei ole 10-bit tilassa.");

    // fr_FR (French)
    registerEntry("fr_FR", TXT_KEY_ANR_TITLE, "L'application ne répond plus");
    registerEntry("fr_FR", TXT_KEY_ANR_CONTENT, "L'application {title} - {class} ne répond plus.\nQue voulez-vous faire?");
    registerEntry("fr_FR", TXT_KEY_ANR_OPTION_TERMINATE, "Forcer l'arrêt");
    registerEntry("fr_FR", TXT_KEY_ANR_OPTION_WAIT, "Attendre");
    registerEntry("fr_FR", TXT_KEY_ANR_PROP_UNKNOWN, "(inconnu)");

    registerEntry("fr_FR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Une application <b>{app}</b> demande une autorisation inconnue.");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Une application <b>{app}</b> tente de capturer votre écran.\n\nVoulez-vous l'autoriser?");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Une application <b>{app}</b> tente de charger un module : <b>{plugin}</b>.\n\nVoulez-vous l'autoriser?");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Un nouveau clavier a été détecté : <b>{keyboard}</b>.\n\nVoulez-vous l'autoriser à fonctionner?");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(inconnu)");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_TITLE, "Demande d'autorisation");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Astuce: vous pouvez définir des règles persistantes dans le fichier de configuration de Hyprland.");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_ALLOW, "Autoriser");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Autoriser et mémoriser");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_ALLOW_ONCE, "Autoriser une fois");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_DENY, "Refuser");
    registerEntry("fr_FR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Application inconnue (ID client wayland {wayland_id})");

    registerEntry("fr_FR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Votre variable d'environnement XDG_CURRENT_DESKTOP semble être gérée de manière externe, et sa valeur actuelle est {value}.\nCela peut provoquer des "
                  "problèmes si ce n'est pas intentionnel.");
    registerEntry("fr_FR", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Votre système n'a pas hyprland-guiutils installé. C'est une dépendance d'éxécution pour certains dialogues. Envisagez de l'installer.");
    registerEntry("fr_FR", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland n'a pas pu charger {count} ressource essentielle, cela indique très probablement un problème dans le paquet de votre distribution.";
        return "Hyprland n'a pas pu charger {count} ressources essentielles, cela indique très probablement un problème dans le paquet de votre distribution.";
    });
    registerEntry("fr_FR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Votre disposition d'écrans est incorrecte. Le moniteur {name} chevauche un ou plusieurs autres.\nVeuillez consulter le wiki (page Moniteurs) pour"
                  "en savoir plus. Cela <b>causera</> des problèmes.");
    registerEntry("fr_FR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Le moniteur {name} n'a pu appliquer les modes demandés, retour au mode {mode}.");
    registerEntry("fr_FR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Échelle invalide pour le moniteur {name}: {scale}. Utilisation de l'échelle suggérée: {fixed_scale}.");
    registerEntry("fr_FR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Échec du chargement du module {name} : {error}");
    registerEntry("fr_FR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Le rechargement du shader CM a échoué, retour aux formats rgba/rgbx");
    registerEntry("fr_FR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Moniteur {name} : l'espace colorimétrique étendu est activé, mais l'écran n'est pas en mode 10-bits.");

    // hi_IN (Hindi)
    registerEntry("hi_IN", TXT_KEY_ANR_TITLE, "एप्लिकेशन प्रतिक्रिया नहीं दे रहा है");
    registerEntry("hi_IN", TXT_KEY_ANR_CONTENT,
                  "एक एप्लिकेशन {title} - {class} प्रतिक्रिया नहीं दे रहा "
                  "है।\nआप इसके साथ क्या करना चाहेंगे?");
    registerEntry("hi_IN", TXT_KEY_ANR_OPTION_TERMINATE, "समाप्त करें");
    registerEntry("hi_IN", TXT_KEY_ANR_OPTION_WAIT, "इंतजार करें");
    registerEntry("hi_IN", TXT_KEY_ANR_PROP_UNKNOWN, "(अज्ञात)");

    registerEntry("hi_IN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "एक एप्लिकेशन <b>{app}</b> एक अज्ञात अनुमति का अनुरोध कर रहा है।");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY,
                  "एक एप्लिकेशन <b>{app}</b> आपकी स्क्रीन कैप्चर करने की "
                  "कोशिश कर रहा है।\n\nक्या आप इसे अनुमति देना चाहते हैं?");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_REQUEST_PLUGIN,
                  "एक एप्लिकेशन <b>{app}</b> एक प्लगइन लोड करने की कोशिश कर रहा है: "
                  "<b>{plugin}</b>.\n\nक्या आप इसे अनुमति देना चाहते हैं?");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD,
                  "नया कीबोर्ड पाया गया: <b>{keyboard}</b>.\n\nक्या आप "
                  "इसे काम करने की अनुमति देना चाहते हैं?");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(अज्ञात)");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_TITLE, "अनुमति अनुरोध");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "संकेत: आप Hyprland कॉन्फ़िग फ़ाइल में इनके लिए स्थायी नियम सेट कर सकते हैं।");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_ALLOW, "अनुमति दें");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "अनुमति दें और याद रखें");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_ALLOW_ONCE, "एक बार अनुमति दें");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_DENY, "अस्वीकार करें");
    registerEntry("hi_IN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "अज्ञात एप्लिकेशन (wayland क्लाइंट ID {wayland_id})");

    registerEntry("hi_IN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "आपका XDG_CURRENT_DESKTOP परिवेश बाहरी रूप से प्रबंधित लगता है, और वर्तमान मान "
                  "{value} है।\nयह समस्या पैदा कर सकता "
                  "है जब तक कि यह जानबूझकर न किया गया हो।");
    registerEntry("hi_IN", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "आपके सिस्टम में hyprland-guiutils इंस्टॉल नहीं है। यह कुछ संवादों के लिए एक रनटाइम "
                  "निर्भरता है। इसे इंस्टॉल करने पर विचार करें।");
    registerEntry("hi_IN", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland {count} आवश्यक संसाधन लोड करने में विफल रहा, अपने डिस्ट्रो "
                   "के पैकेजर को पैकेजिंग में खराब काम करने का दोष दें!";
        return "Hyprland {count} आवश्यक संसाधनों को लोड करने में विफल रहा, अपने "
               "डिस्ट्रो के पैकेजर को पैकेजिंग में खराब काम करने का दोष दें!";
    });
    registerEntry("hi_IN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "आपका मॉनिटर लेआउट गलत तरीके से सेट है। मॉनिटर {name} लेआउट में अन्य मॉनिटर(ओं) के "
                  "साथ ओवरलैप कर रहा है।\nकृपया विकि "
                  " (Monitors पेज) देखें। यह <b>समस्याएँ</b> पैदा करेगा।");
    registerEntry("hi_IN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL,
                  "मॉनिटर {name} ने किसी भी अनुरोधित मोड को सेट करने में "
                  "विफल रहा, मोड {mode} पर वापस जा रहा है।");
    registerEntry("hi_IN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE,
                  "मॉनिटर {name} को अवैध स्केल दिया गया: {scale}, सुझाया "
                  "गया स्केल इस्तेमाल किया जा रहा है: {fixed_scale}");
    registerEntry("hi_IN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "प्लगइन {name} लोड करने में विफल: {error}");
    registerEntry("hi_IN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM शेडर रीलोड विफल हुआ, rgba/rgbx पर वापस जा रहा है।");
    registerEntry("hi_IN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "मॉनिटर {name}: वाइड कलर गैम सक्षम है लेकिन डिस्प्ले 10-बिट मोड में नहीं है।");

    // id_ID (Indonesia)
    registerEntry("id_ID", TXT_KEY_ANR_TITLE, "Aplikasi Tidak Merespon");
    registerEntry("id_ID", TXT_KEY_ANR_CONTENT, "Aplikasi {title} - {class} tidak merespon.\nApa yang ingin Anda lakukan?");
    registerEntry("id_ID", TXT_KEY_ANR_OPTION_TERMINATE, "Hentikan");
    registerEntry("id_ID", TXT_KEY_ANR_OPTION_WAIT, "Tunggu");
    registerEntry("id_ID", TXT_KEY_ANR_PROP_UNKNOWN, "(tidak diketahui)");

    registerEntry("id_ID", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Aplikasi <b>{app}</b> meminta izin yang tidak dikenali.");
    registerEntry("id_ID", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Aplikasi <b>{app}</b> mencoba merekam layar Anda.\n\nApakah Anda mengizinkannya?");
    registerEntry("id_ID", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Aplikasi <b>{app}</b> mencoba memuat <i>plugin</i>: <b>{plugin}</b>.\n\nApakah Anda mengizinkannya?");
    registerEntry("id_ID", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Keyboard baru terdeteksi: <b>{keyboard}</b>.\n\nApakah Anda mengizinkannya beroperasi?");
    registerEntry("id_ID", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(tidak diketahui)");
    registerEntry("id_ID", TXT_KEY_PERMISSION_TITLE, "Permintaan Izin");
    registerEntry("id_ID", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Petunjuk: Anda dapat mengatur <i>rule</i> ini secara permanen di <i>file</i> konfigurasi Hyprland.");
    registerEntry("id_ID", TXT_KEY_PERMISSION_ALLOW, "Izinkan");
    registerEntry("id_ID", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Izinkan dan Ingat");
    registerEntry("id_ID", TXT_KEY_PERMISSION_ALLOW_ONCE, "Izinkan Sekali");
    registerEntry("id_ID", TXT_KEY_PERMISSION_DENY, "Tolak");
    registerEntry("id_ID", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Aplikasi tidak dikenal (ID klien wayland {wayland_id})");

    registerEntry("id_ID", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Variabel <i>environment</i> XDG_CURRENT_DESKTOP Anda tampaknya dikelola secara eksternal, nilainya saat ini: {value}.\nHal ini dapat menyebabkan "
                  "masalah, kecuali jika disengaja.");
    registerEntry("id_ID", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "hyprland-guiutils belum terpasang di Sistem Anda. Paket tersebut merupakan dependensi <i>runtime</i> untuk beberapa dialog. Mohon untuk menginstalnya.");
    registerEntry("id_ID", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland gagal memuat {count} aset penting. Salahkan pengelola paket distro Anda karena pengemasannya buruk!";
        return "Hyprland gagal memuat {count} aset penting. Salahkan pengelola paket distro Anda karena pengemasannya buruk!";
    });
    registerEntry("id_ID", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Susunan monitor Anda tidak benar. Monitor {name} tertumpuk dengan monitor lain.\nSilakan lihat wiki (halaman <i>Monitors</i>) untuk "
                  "detailnya. Hal ini <b>pasti</b> akan menimbulkan masalah.");
    registerEntry("id_ID", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} gagal menerapkan mode yang diminta, kembali ke mode {mode}.");
    registerEntry("id_ID", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Skala tidak valid diberikan ke monitor {name}: {scale}, skala yang disarankan: {fixed_scale}");
    registerEntry("id_ID", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Gagal memuat plugin {name}: {error}");
    registerEntry("id_ID", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Gagal memuat ulang shader CM, kembali ke rgba/rgbx.");
    registerEntry("id_ID", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: <i>wide color gamut</i> aktif tetapi layar tidak dalam mode 10-bit.");

    // hr_HR (Croatian)
    registerEntry("hr_HR", TXT_KEY_ANR_TITLE, "Aplikacija ne reagira");
    registerEntry("hr_HR", TXT_KEY_ANR_CONTENT, "Aplikacija {title} - {class} ne reagira.\nŠto želiš napraviti s njom?");
    registerEntry("hr_HR", TXT_KEY_ANR_OPTION_TERMINATE, "Zaustavi");
    registerEntry("hr_HR", TXT_KEY_ANR_OPTION_WAIT, "Pričekaj");
    registerEntry("hr_HR", TXT_KEY_ANR_PROP_UNKNOWN, "(nepoznato)");

    registerEntry("hr_HR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Aplikacija <b>{app}</b> zahtijeva nepoznatu dozvolu.");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Aplikacija <b>{app}</b> pokušava snimati vaš zaslon.\n\nŽeliš li dopustiti?");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Aplikacija <b>{app}</b> pokušava učitati dodatak: <b>{plugin}</b>.\n\nŽeliš li dopustiti?");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Otkrivena je nova tipkovnica: <b>{keyboard}</b>.\n\nŽeliš li omogućiti njen rad?");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(nepoznato)");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_TITLE, "Zahtjev za dozvolu");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Savjet: za ovo možeš postaviti trajna pravila u Hyprland konfiguracijskoj datoteci.");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_ALLOW, "Dozvoli");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Dozvoli i zapamti");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_ALLOW_ONCE, "Dozvoli samo ovaj put");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_DENY, "Uskrati");
    registerEntry("hr_HR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Nepoznata aplikacija (ID wayland klijenta {wayland_id})");

    registerEntry("hr_HR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Izgleda da je tvoja XDG_CURRENT_DESKTOP okolina vanjski upravljana te je trenutna vrijednost {value}.\nOvo može izazvati problem, osim ako je namjerno.");
    registerEntry("hr_HR", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Na tvojem sustavu nije instaliran hyprland-guiutils. Ovo je ovisnost tijekom pokretanja nekih dijaloga. Preporučeno je da je instaliraš.");
    registerEntry("hr_HR", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo % 10 <= 1 && assetsNo % 100 != 11)
            return "Hyprland nije uspio učitati {count} neophodnu komponentu, krivi pakera svoje distribucije za loš posao pakiranja!";
        else if (assetsNo % 10 <= 4 && assetsNo % 100 > 14)
            return "Hyprland nije uspio učitati {count} neophodne komponente, krivi pakera svoje distribucije za loš posao pakiranja!";
        return "Hyprland nije uspio učitati {count} neophodnih komponenata, krivi pakera svoje distribucije za loš posao pakiranja!";
    });
    registerEntry("hr_HR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Raspored tvojih monitora je krivo postavljen. Monitor {name} preklapa se s ostalim monitorom/ima u rasporedu.\nProvjeri wiki (Monitors stranicu) za "
                  "više informacija. Ovo <b>hoće</b> izazvati probleme.");
    registerEntry("hr_HR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} nije uspio odrediti zatražene načine rada, povratak na zadani način rada: {mode}.");
    registerEntry("hr_HR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Nevažeći razmjer proslijeđen monitoru {name}: {scale}, koristi se predloženi razmjer: {fixed_scale}");
    registerEntry("hr_HR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Učitavanje dodatka {name} nije uspjelo: {error}");
    registerEntry("hr_HR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Ponovno učitavanje CM shadera nije uspjelo, povratak na zadano: rgba/rgbx.");
    registerEntry("hr_HR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: široki raspon boja je omogućen, ali ekran nije u 10-bitnom načinu rada.");

    // it_IT (Italian)
    registerEntry("it_IT", TXT_KEY_ANR_TITLE, "L'applicazione non risponde");
    registerEntry("it_IT", TXT_KEY_ANR_CONTENT, "Un'applicazione {title} - {class} non risponde.\nCosa vuoi fare?");
    registerEntry("it_IT", TXT_KEY_ANR_OPTION_TERMINATE, "Termina");
    registerEntry("it_IT", TXT_KEY_ANR_OPTION_WAIT, "Attendi");
    registerEntry("it_IT", TXT_KEY_ANR_PROP_UNKNOWN, "(sconosciuto)");

    registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Un'applicazione <b>{app}</b> richiede un'autorizzazione sconosciuta.");
    registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Un'applicazione <b>{app}</b> sta tentando di catturare il tuo schermo.\n\nVuoi autorizzarla?");
    registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_CURSOR_POS, "Un'applicazione <b>{app}</b> sta tentando di leggere la posizione del cursore.\n\nVuoi autorizzarla?");
    registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Un'applicazione <b>{app}</b> sta tentando di caricare un plugin: <b>{plugin}</b>.\n\nVuoi autorizzarla?");
    registerEntry("it_IT", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "È stata rilevata una nuova tastiera: <b>{keyboard}</b>.\n\nVuoi autorizzarla a operare?");
    registerEntry("it_IT", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(sconosciuto)");
    registerEntry("it_IT", TXT_KEY_PERMISSION_TITLE, "Richiesta di autorizzazione");
    registerEntry("it_IT", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Suggerimento: puoi impostare una regola persistente nel tuo file di configurazione di Hyprland.");
    registerEntry("it_IT", TXT_KEY_PERMISSION_ALLOW, "Consenti");
    registerEntry("it_IT", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Consenti e ricorda");
    registerEntry("it_IT", TXT_KEY_PERMISSION_ALLOW_ONCE, "Consenti una volta");
    registerEntry("it_IT", TXT_KEY_PERMISSION_DENY, "Nega");
    registerEntry("it_IT", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Applicazione sconosciuta (wayland client ID {wayland_id})");

    registerEntry("it_IT", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "L'ambiente XDG_CURRENT_DESKTOP sembra essere gestito esternamente, il valore attuale è {value}.\nSe non è voluto, potrebbe causare problemi.");
    registerEntry("it_IT", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Sembra che hyprland-guiutils non sia installato. È una dipendenza richiesta per alcuni dialoghi che potresti voler installare.");
    registerEntry("it_IT", TXT_KEY_NOTIF_FAILED_ASSETS,
                  "Hyprland non ha potuto caricare {count} asset, dai la colpa al packager della tua distribuzione per il suo cattivo lavoro!");
    registerEntry("it_IT", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "I tuoi schermi sono configurati incorrettamente. Lo schermo {name} si sovrappone con altri nel layout.\nConsulta la wiki (voce Schermi) per "
                  "altre informazioni. Questo <b>causerà</b> problemi.");
    registerEntry("it_IT", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Lo schermo {name} non ha potuto impostare alcuna modalità richiesta, sarà usata la modalità {mode}.");
    registerEntry("it_IT", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Fattore di scala non valido per lo schermo {name}: {scale}; verrà usato quello consigliato: {fixed_scale}");
    registerEntry("it_IT", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Impossibile caricare il plugin {name}: {error}");
    registerEntry("it_IT", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Impossibile ricaricare gli shader CM, verrà usato rgba/rgbx.");
    registerEntry("it_IT", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Schermo {name}: la gamma di colori ampia è abilitata ma lo schermo non è in modalità 10-bit.");
    registerEntry("it_IT", TXT_KEY_NOTIF_NO_WATCHDOG,
                  "Hyprland è stato avviato senza start-hyprland. Ciò è assolutamente sconsigliato a meno che tu non sia in un ambiente di debug.");

    registerEntry("it_IT", TXT_KEY_SAFE_MODE_TITLE, "Modalità sicura");
    registerEntry(
        "it_IT", TXT_KEY_SAFE_MODE_DESCRIPTION,
        "Hyprland è stato avviato in modalità sicura, dato che l'ultima sessione è crashata.\nLa modalità sicura impedisce alla tua configurazione di essere caricata. Puoi "
        "risolvere i problemi in questo ambiente, o caricare la tua configurazione con il pulsante sottostante.\nVerranno usate le scorciatoie predefinite: SUPER+Q per kitty, "
        "SUPER+R per un runner di base, "
        "SUPER+M per uscire.\nAl riavvio "
        "Hyprland verrà avviato in modalità normale.");
    registerEntry("it_IT", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Carica la configurazione");
    registerEntry("it_IT", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Apri la cartella delle segnalazioni di crash");
    registerEntry("it_IT", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "Va bene, chiudi");

    // ja_JP (Japanese)
    registerEntry("ja_JP", TXT_KEY_ANR_TITLE, "アプリが応答しません");
    registerEntry("ja_JP", TXT_KEY_ANR_CONTENT, "アプリ {title} - {class} が応答しません。\nどうしますか？");
    registerEntry("ja_JP", TXT_KEY_ANR_OPTION_TERMINATE, "強制終了");
    registerEntry("ja_JP", TXT_KEY_ANR_OPTION_WAIT, "待機");
    registerEntry("ja_JP", TXT_KEY_ANR_PROP_UNKNOWN, "（不明）");

    registerEntry("ja_JP", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "アプリ <b>{app}</b> が権限を求めています。");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "アプリ <b>{app}</b> が画面をキャプチャしようとしています。\n\n許可しますか？");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "アプリ <b>{app}</b> がプラグイン <b>{plugin}</b> をロードしようとしています。\n\n許可しますか？");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "新しいキーボード <b>{keyboard}</b> が接続されました。\n\n使用を許可しますか？");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_UNKNOWN_NAME, "（不明）");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_TITLE, "権限の要求");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "ヒント：永続的なルールを Hyprland の設定ファイルに記述できます。");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_ALLOW, "許可");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "許可して保存");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_ALLOW_ONCE, "今回だけ許可");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_DENY, "却下");
    registerEntry("ja_JP", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "不明なアプリ（wayland クライアント ID {wayland_id}）");

    registerEntry("ja_JP", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "環境変数 XDG_CURRENT_DESKTOP は外部から {value} に設定されています。\n意図的なものでなければ、何らかの問題を起こすかもしれません。");
    registerEntry("ja_JP", TXT_KEY_NOTIF_NO_GUIUTILS, "hyprland-guiutils がありません。このパッケージをインストールしてください。");
    registerEntry("ja_JP", TXT_KEY_NOTIF_FAILED_ASSETS, "{count} 個の必要なアセットをロードできません。ディストリビューションのパッケージ作成者にこの問題を報告してください。");
    registerEntry("ja_JP", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "モニタのレイアウトが正しく設定されていません。モニタ {name} の表示領域が他のモニタと重複しています。\n詳細は Wiki の Monitor "
                  "の項目を参照してください。これは<b>絶対に</b>問題を起こします。");
    registerEntry("ja_JP", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "モニタ {name} のモード設定に失敗したため、モード {mode} を使用します。");
    registerEntry("ja_JP", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "モニタ {name} のスケール設定が正しくないため、代わりにスケール {fixed_scale} を使用します。");
    registerEntry("ja_JP", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "プラグイン {name} のロードで、エラー {error} が発生しました。");
    registerEntry("ja_JP", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM シェーダのリロードに失敗したため、rgba/rgbx を使用します。");
    registerEntry("ja_JP", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "広色域が有効なモニタ {name} を使用していますが、画面表示の設定は 10 ビットになっていません。");
    registerEntry("ja_JP", TXT_KEY_NOTIF_NO_WATCHDOG, "start-hyprland なしで Hyprland を実行しています。これは、デバッグ目的以外ではおすすめしません。");

    registerEntry("ja_JP", TXT_KEY_SAFE_MODE_TITLE, "セーフモード");
    registerEntry("ja_JP", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "前回のセッションがクラッシュしました。Hyprland "
                  "は設定ファイルをロードしない、セーフモードで動作しています。\n問題を解決するか、もしくは下のボタンで設定ファイルをロードしてください。"
                  "\nデフォルトのキーバインドは、SUPER+Q が kitty、SUPER+R が簡素なランチャー、SUPER+M が Hyprland の終了です。"
                  "\nHyprland を再起動することで、ノーマルモードで動作します。");
    registerEntry("ja_JP", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "設定ファイルをロード");
    registerEntry("ja_JP", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "クラッシュレポートフォルダを開く");
    registerEntry("ja_JP", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "了解（このウィンドウを閉じる）");

    // lv_LV (Latvian)
    registerEntry("lv_LV", TXT_KEY_ANR_TITLE, "Lietotne nereaģē");
    registerEntry("lv_LV", TXT_KEY_ANR_CONTENT, "Lietotne {title} - {class} nereaģē.\nKo jūs vēlaties darīt?");
    registerEntry("lv_LV", TXT_KEY_ANR_OPTION_TERMINATE, "Beigt procesu");
    registerEntry("lv_LV", TXT_KEY_ANR_OPTION_WAIT, "Gaidīt");
    registerEntry("lv_LV", TXT_KEY_ANR_PROP_UNKNOWN, "(nezināms)");

    registerEntry("lv_LV", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Lietotne <b>{app}</b> pieprasa nezināmu atļauju.");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Lietotne <b>{app}</b> mēģina lasīt no jūsu ekrāna.\n\nVai vēlaties to atļaut?");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Lietotne <b>{app}</b> mēģina ielādēt spraudni: <b>{plugin}</b>.\n\nVai vēlaties to atļaut?");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Ir atrasta jauna tastatūra: <b>{keyboard}</b>.\n\nVai vēlaties atļaut tās darbību?");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(nezināms)");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_TITLE, "Atļaujas pieprasījums");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Padoms: Hyprland konfigurācijas failā varat arī iestatīt atļaujas.");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_ALLOW, "Atļaut");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Atļaut un atcerēties");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_ALLOW_ONCE, "Atļaut vienreiz");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_DENY, "Aizliegt");
    registerEntry("lv_LV", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Nezināma lietotne (Wayland klienta ID {wayland_id})");

    registerEntry("lv_LV", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP, "Jūsu XDG_CURRENT_DESKTOP tiek ārēji pārvaldīts, tās vērtība ir {value}.\nTas var neapzināti izraisīt problēmas.");
    registerEntry("lv_LV", TXT_KEY_NOTIF_NO_GUIUTILS, "Jums nav instalēts hyprland-guiutils. Šī pakotne ir nepieciešama dažiem dialogiem. Apsveriet tās instalēšanu.");
    registerEntry("lv_LV", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland nevarēja ielādēt {count} būtisku resursu, vainojiet sava distro iepakotāju par sliktu iepakošanu!";
        return "Hyprland nevarēja ielādēt {count} būtiskus resursus, vainojiet sava distro iepakotāju par sliktu iepakošanu!";
    });
    registerEntry("lv_LV", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Jūsu monitora izkārtojums ir nepareizi iestatīts. Monitors {name} pārklājas ar citiem izkārtojumā iestatītajiem monitoriem.\nLūdzu apskatieties (Monitoru lapā),"
                  "lai uzzinātu vairāk. Tas <b>radīs</b> problēmas.");
    registerEntry("lv_LV", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitoram {name} neizdevās iestatīt nevienu no pieprasītajiem režīmiem, izmantojam {mode}.");
    registerEntry("lv_LV", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Monitoram {name} ir nodots nederīgs mērogs: {scale}, izmantojam ieteikto mērogu: {fixed_scale}");
    registerEntry("lv_LV", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Nevarēja ielādēt spraudni {name}: {error}");
    registerEntry("lv_LV", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM šeiderus neizdevās pārlādēt, izmantojam rgba/rgbx.");
    registerEntry("lv_LV", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitors {name}: Ir iespējota plaša krāsu gamma, bet displejs nav 10-bitu režīmā.");

    // hu_HU (Hungarian)
    registerEntry("hu_HU", TXT_KEY_ANR_TITLE, "Az alkalmazás nem válaszol");
    registerEntry("hu_HU", TXT_KEY_ANR_CONTENT, "A(z) {title} - {class} alkalmazás nem válaszol.\nMit szeretne tenni vele?");
    registerEntry("hu_HU", TXT_KEY_ANR_OPTION_TERMINATE, "Leállítás");
    registerEntry("hu_HU", TXT_KEY_ANR_OPTION_WAIT, "Várakozás");
    registerEntry("hu_HU", TXT_KEY_ANR_PROP_UNKNOWN, "(ismeretlen)");

    registerEntry("hu_HU", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "A(z) <b>{app}</b> alkalmazás ismeretlen engedélyt kér.");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "A(z) <b>{app}</b> alkalmazás megpróbálja rögzíteni a képernyőjét.\n\nEngedélyezi?");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "A(z) <b>{app}</b> alkalmazás megpróbál egy bővítményt betölteni: <b>{plugin}</b>.\n\nEngedélyezi?");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Új billentyűzetet észleltünk: <b>{keyboard}</b>.\n\nEngedélyezi a használatát?");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(ismeretlen)");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_TITLE, "Engedélykérés");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tipp: Állandó szabályokat állíthat be a Hyprland konfigurációs fájlban.");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_ALLOW, "Engedélyezés");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Mindig engedélyez");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_ALLOW_ONCE, "Egyszeri engedélyezés");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_DENY, "Elutasítás");
    registerEntry("hu_HU", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Ismeretlen alkalmazás (wayland kliens ID {wayland_id})");

    registerEntry("hu_HU", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Úgy tűnik, hogy az XDG_CURRENT_DESKTOP környezetet külsőleg kezelik, és a jelenlegi érték {value}.\nEz problémákat okozhat, hacsak nem szándékos.");
    registerEntry("hu_HU", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "A rendszerében nincs telepítve a hyprland-guiutils. Ez egy futásidejű függőség néhány párbeszédablakhoz. Fontolja meg a telepítését.");
    registerEntry("hu_HU", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "A Hyprland nem tudta betölteni az 1 szükséges erőforrást. Kérjük, jelezze a hibát a disztribúció csomagolójának.";
        return "A Hyprland nem tudott betölteni {count} szükséges erőforrást. Kérjük, jelezze a hibát a disztribúció csomagolójának.";
    });
    registerEntry(
        "hu_HU", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "A monitor elrendezése helytelenül van beállítva. A(z) {name} monitor átfedi a többi monitort az elrendezésben.\nKérjük, további információkért tekintse meg a wikit "
        "(Monitors oldal). Ez <b>problémákat</b> fog okozni.");
    registerEntry("hu_HU", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "A(z) {name} monitor nem tudta beállítani a kért módokat, visszaáll a(z) {mode} módra.");
    registerEntry("hu_HU", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Érvénytelen skálázás a(z) {name} monitorhoz: {scale}, a javasolt skálázás használata: {fixed_scale}");
    registerEntry("hu_HU", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Nem sikerült betölteni a(z) {name} bővítményt: {error}");
    registerEntry("hu_HU", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "A CM shader újratöltése sikertelen, visszaáll rgba/rgbx-re.");
    registerEntry("hu_HU", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: A széles színtartomány engedélyezve van, de a kijelző nem 10 bites módban van.");

    // ml_IN (Malayalam)
    registerEntry("ml_IN", TXT_KEY_ANR_TITLE, "ആപ്ലിക്കേഷൻ പ്രതികരിക്കുന്നില്ല");
    registerEntry("ml_IN", TXT_KEY_ANR_CONTENT, "ആപ്ലിക്കേഷൻ {title} - {class} പ്രതികരിക്കുന്നില്ല.\nഇതിന് നിങ്ങൾ എന്ത് ചെയ്യാൻ ആഗ്രഹിക്കുന്നു?");
    registerEntry("ml_IN", TXT_KEY_ANR_OPTION_TERMINATE, "അവസാനിപ്പിക്കുക");
    registerEntry("ml_IN", TXT_KEY_ANR_OPTION_WAIT, "കാത്തിരിക്കുക");
    registerEntry("ml_IN", TXT_KEY_ANR_PROP_UNKNOWN, "(അജ്ഞാതം)");

    registerEntry("ml_IN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "ആപ്ലിക്കേഷൻ <b>{app}</b> ഒരു അജ്ഞാത അനുമതി അഭ്യർത്ഥിക്കുന്നു.");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "ആപ്ലിക്കേഷൻ <b>{app}</b> നിങ്ങളുടെ സ്ക്രീൻ പകർത്താൻ ശ്രമിക്കുന്നു.\n\nനിങ്ങൾ അത് അനുവദിക്കണോ?");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "ആപ്ലിക്കേഷൻ <b>{app}</b> ഒരു പ്ലഗിൻ ലോഡ് ചെയ്യാൻ ശ്രമിക്കുന്നു: <b>{plugin}</b>.\n\nഇത് അനുവദിക്കണോ?");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "പുതിയ കീബോർഡ് കണ്ടെത്തി: <b>{keyboard}</b>.\n\nഇത് പ്രവർത്തിക്കാൻ അനുവദിക്കണോ?");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(അജ്ഞാതം)");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_TITLE, "അനുമതി അഭ്യർത്ഥന");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "സൂചന: Hyprland കോൺഫിഗ് ഫയലിൽ സ്ഥിരനിയമങ്ങൾ സജ്ജമാക്കാം.");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_ALLOW, "അനുവദിക്കുക");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "അനുവദിച്ച് ഓർക്കുക");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_ALLOW_ONCE, "ഒന്നുതവണ അനുവദിക്കുക");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_DENY, "നിരസിക്കുക");
    registerEntry("ml_IN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "അജ്ഞാത അപ്ലിക്കേഷൻ (wayland client ID {wayland_id})");

    registerEntry("ml_IN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "നിങ്ങളുടെ XDG_CURRENT_DESKTOP പരിസ്ഥിതി പുറത്ത് നിന്ന് നിയന്ത്രിക്കപ്പെടുന്നു, ഇപ്പോഴത്തെ മൂല്യം "
                  "{value}.\nഇത് ഉദ്ദേശ്യമായല്ലെങ്കിൽ പ്രശ്നങ്ങൾ ഉണ്ടാകും.");
    registerEntry("ml_IN", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "നിങ്ങളുടെ സിസ്റ്റത്തിൽ hyprland-guiutils ഇൻസ്റ്റാൾ ചെയ്തിട്ടില്ല. ഇത് ചില ഡയലോഗുകൾക്ക് ആവശ്യമായ "
                  "റൺടൈം ആശ്രയമാണ്. ഇൻസ്റ്റാൾ ചെയ്യുക.");
    registerEntry("ml_IN", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland {count} പ്രധാന അസറ്റ് ലോഡുചെയ്യാൻ പരാജയപ്പെട്ടു, നിങ്ങളുടെ "
                   "ഡിസ്‌ട്രോ "
                   "പാക്കേജർ പിശക് ചെയ്തിരിക്കുന്നു!";
        return "Hyprland {count} പ്രധാന അസറ്റുകൾ ലോഡുചെയ്യാൻ പരാജയപ്പെട്ടു, നിങ്ങളുടെ "
               "ഡിസ്‌ട്രോ "
               "പാക്കേജർ പിശക് ചെയ്തിരിക്കുന്നു!";
    });
    registerEntry("ml_IN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "മോണിറ്റർ ലേയൗട്ട് തെറ്റാണ്. മോണിറ്റർ {name} മറ്റുള്ളവയുമായ് ഒതുങ്ങുന്നു.\nകൂടുതൽ വിവരങ്ങൾക്ക് Wiki "
                  "(Monitors page) കാണുക. ഇത് <b>പ്രശ്നങ്ങൾ ഉണ്ടാക്കും</b>.");
    registerEntry("ml_IN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "മോണിറ്റർ {name} ആവശ്യപ്പെട്ട മോഡുകൾ സജ്ജമാക്കാൻ പരാജയപ്പെട്ടു, ഇപ്പോൾ {mode} ഉപയോഗിക്കുന്നു.");
    registerEntry("ml_IN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "മോണിറ്റർ {name} ന് അസാധുവായ സ്കെയിൽ: {scale}, നിർദ്ദേശിച്ച സ്കെയിൽ: {fixed_scale}");
    registerEntry("ml_IN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "പ്ലഗിൻ {name} ലോഡ് ചെയ്യാൻ പരാജയപ്പെട്ടു: {error}");
    registerEntry("ml_IN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM ഷേഡർ റീലോഡ് പരാജയപ്പെട്ടു, rgba/rgbx ലേക്ക് മാറുന്നു.");
    registerEntry("ml_IN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "മോണിറ്റർ {name}: വൈഡ് കളർ ഗാമട്ട് പ്രവർത്തനക്ഷമമാണെങ്കിലും, മോഡ് 10-bit അല്ല.");

    // nb_NO (Norwegian Bokmål)
    registerEntry("nb_NO", TXT_KEY_ANR_TITLE, "Applikasjonen svarer ikke");
    registerEntry("nb_NO", TXT_KEY_ANR_CONTENT, "En applikasjon {title} - {class} svarer ikke.\nHva vil du gjøre med den?");
    registerEntry("nb_NO", TXT_KEY_ANR_OPTION_TERMINATE, "Avslutt");
    registerEntry("nb_NO", TXT_KEY_ANR_OPTION_WAIT, "Vent");
    registerEntry("nb_NO", TXT_KEY_ANR_PROP_UNKNOWN, "(ukjent)");

    registerEntry("nb_NO", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "En applikasjon <b>{app}</b> ber om en ukjent tillatelse.");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "En applikasjon <b>{app}</b> prøver å fange skjermen din.\n\nVil du tillate den?");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "En applikasjon <b>{app}</b> prøver å laste en plugin: <b>{plugin}</b>.\n\nVil du tillate den?");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Et nytt tastatur er oppdaget: <b>{keyboard}</b>.\n\nVil du tillate at det opererer?");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(ukjent)");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_TITLE, "Tillatelsesforespørsel");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Hint: du kan angi vedvarende regler for disse i Hyprland konfigurasjonsfilen.");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_ALLOW, "Tillat");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Tillat og husk");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_ALLOW_ONCE, "Tillat en gang");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_DENY, "Nekte");
    registerEntry("nb_NO", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Ukjent applikasjon (wayland client ID {wayland_id})");

    registerEntry(
        "nb_NO", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Ditt XDG_CURRENT_DESKTOP miljø ser ut til å være eksternt administrert, og den nåværende verdien er {value}.\nDette kan forårsake problemer med mindre det er bevisst.");
    registerEntry("nb_NO", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Ditt system har ikke hyprland-guiutils installert. Dette er en kjøretidsavhengighet for noen dialoger. Vurder å installere den.");
    registerEntry("nb_NO", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland kunne ikke laste {count} essensiell ressurs, skyld på distroens pakkeansvarlig for å ha gjort en dårlig jobb med pakkingen!";
        return "Hyprland kunne ikke laste {count} essensielle ressurser, skyld på distroens pakkeansvarlig for å ha gjort en dårlig jobb med pakkingen!";
    });
    registerEntry("nb_NO", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Skjermoppsettet ditt er satt opp feil. Skjerm {name} overlapper med skjerm(er) i oppsettet.\nSjekk wiki (Skjerm oppsett siden) for "
                  "mer. Dette <b>vil</b> skape problemer.");
    registerEntry("nb_NO", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Skjerm {name} feilet å sette de forespurte modusene, faller tilbake til modus {mode}.");
    registerEntry("nb_NO", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Ugyldig skala sendt til skjerm {name}: {scale}, bruker foreslått skala: {fixed_scale}");
    registerEntry("nb_NO", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Feilet å laste plugin {name}: {error}");
    registerEntry("nb_NO", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader omlading feilet, faller tilbake til rgba/rgbx.");
    registerEntry("nb_NO", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Skjerm {name}: bredt fargespekter er aktivert, men skjermen er ikke i 10-bit modus.");

    // ne_NP (Nepali)
    registerEntry("ne_NP", TXT_KEY_ANR_TITLE, "एपले रिस्पन्ड गरिरहेको छैन");
    registerEntry("ne_NP", TXT_KEY_ANR_CONTENT, "{title} - {class} एपले रिस्पन्ड गरिरहेको छैन।\nयससँग के गर्न चहानुहुन्छ?");
    registerEntry("ne_NP", TXT_KEY_ANR_OPTION_TERMINATE, "टर्मिनेट गर्नुहोस्");
    registerEntry("ne_NP", TXT_KEY_ANR_OPTION_WAIT, "पर्खनुहोस्");
    registerEntry("ne_NP", TXT_KEY_ANR_PROP_UNKNOWN, "(अज्ञात)");

    registerEntry("ne_NP", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "<b>{app}</b> एपले अज्ञात सुविधाको अनुमति मागिरहेको छ।");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "<b>{app}</b> एपले स्क्रिन क्याप्चर गर्न खोज्दै छ।\n\nयसलाई अनुमति दिन चहानुहुन्छ?");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "<b>{app}</b> एपले एउटा प्लगिन लोड गर्न खोज्दै छ: <b>{plugin}</b>।\n\nयसलाई अनुमति दिन चहानुहुन्छ?");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "एउटा नयाँ किबोर्ड डिटेक्ट गरिएको छ: <b>{keyboard}</b>।\n\nयसलाई चल्ने अनुमति दिन चहानुहुन्छ?");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(अज्ञात)");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_TITLE, "अनुमतिको माग");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "टिप: यसको लागि पर्सिस्टेन्ट नियमहरु तपाइँले हाइपरल्यान्डको कन्फीग्युरेसन फाइलमा राख्न सक्नुहुन्छ।");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_ALLOW, "अनुमति दिनुहोस्");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "अनुमति दिनुहोस् र सम्झनुहोस्");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_ALLOW_ONCE, "एकपटक अनुमति दिनुहोस्");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_DENY, "अनुमति नदिनुहोस्");
    registerEntry("ne_NP", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "अज्ञात एप (wayland क्लाइन्ट आईडी {wayland_id})");

    registerEntry("ne_NP", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "तपाईँको XDG_CURRENT_DESKTOP वातावरण बाहिरबाट व्यवस्थापन भइरहेको जस्तो देखिएको छ, अहिले {value} देखाइरहेको छ।\nजानीजानी नगरीएको भएमा यसले समस्याहरु निम्त्याउन सक्छ।");
    registerEntry("ne_NP", TXT_KEY_NOTIF_NO_GUIUTILS, "तपाइँको सिस्टममा hyprland-guiutils इन्सटल गरिएको छैन। केहि डायलगहरुका लागि यो रनटाइम डिपेन्डेन्सी हो। कृपया इन्सटल गर्नुहोला।");
    registerEntry("ne_NP", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "हाइपरल्यान्डले एउटा अत्यावश्यक एसेट लोड गर्न सकेन, तपाइँको डिस्ट्रोको प्याकेजरको प्याकेजिङ गतिलो छैन!";
        return "हाइपरल्यान्डले {count} अत्यावश्यक एसेटहरु लोड गर्न सकेन, तपाइँको डिस्ट्रोको प्याकेजरको प्याकेजिङ गतिलो छैन!";
    });
    registerEntry("ne_NP", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "तपाइँको मनिटरको लेआउट गलत तरिकाले मिलाइएको छ। लेआउटमा {name} मनिटर अर्को मनिटर वा मनिटरहरुसङ्ग ओभरल्याप भएको छ।\nथप बुझ्नलाई कृपया विकिको मनिटर पेज हेर्नुहोस्।"
                  "यसले <b>निश्चित रुपमा</b> समस्या निम्त्याउने छ।");
    registerEntry("ne_NP", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "{name} मनिटरले चाहेको कुनै पनि मोड सेट गर्न सकेन, {mode} मोडमा फर्कँदै।");
    registerEntry("ne_NP", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "{name} मनिटरलाई अमान्य स्केल पठाइयो: {scale}, सजेस्ट गरिएको स्केल प्रयोग गर्दै: {fixed_scale}");
    registerEntry("ne_NP", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "{name} प्लगिन लोेड गर्न सकिएन: {error}");
    registerEntry("ne_NP", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader रिलोड गर्न सकिएन, rgba/rgbx मा फर्कँदै।");
    registerEntry("ne_NP", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "{name} मनिटर: wide color gamut अन छ तर डिस्प्ले 10-bit मोड मा छैन।");

    // nl_NL (Dutch)
    registerEntry("nl_NL", TXT_KEY_ANR_TITLE, "Applicatie Reageert Niet");
    registerEntry("nl_NL", TXT_KEY_ANR_CONTENT, "Een applicatie {title} - {class} reageert niet.\nWat wilt u doen?");
    registerEntry("nl_NL", TXT_KEY_ANR_OPTION_TERMINATE, "Beëindigen");
    registerEntry("nl_NL", TXT_KEY_ANR_OPTION_WAIT, "Wachten");
    registerEntry("nl_NL", TXT_KEY_ANR_PROP_UNKNOWN, "(onbekend)");

    registerEntry("nl_NL", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Een applicatie <b>{app}</b> vraagt om een onbekende machtiging.");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Een applicatie <b>{app}</b> probeert uw scherm op te nemen.\n\nWilt u dit toestaan?");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Een applicatie <b>{app}</b> probeert een plugin te laden: <b>{plugin}</b>.\n\nWilt u dit toestaan?");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Een nieuw toetsenbord is gedetecteerd: <b>{keyboard}</b>.\n\nWilt u toestemming geven dat het wordt gebruikt?");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(onbekend)");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_TITLE, "Toestemmingsverzoek");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tip: U kunt hiervoor vaste regels instellen in het Hyprland-configuratiebestand.");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_ALLOW, "Toestaan");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Toestaan en onthouden");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_ALLOW_ONCE, "Één keer toestaan");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_DENY, "Weigeren");
    registerEntry("nl_NL", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Onbekende applicatie (wayland client ID {wayland_id})");

    registerEntry(
        "nl_NL", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "De XDG_CURRENT_DESKTOP omgevingsvariabele lijkt extern beheerd te worden en de huidige waarde is {value}.\nDit kan problemen veroorzaken, tenzij dit opzettelijk is.");
    registerEntry("nl_NL", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Hyprland-guiutils is niet op uw systeem geïnstalleerd. Dit is een runtime-afhankelijkheid voor sommige dialogen. Overweeg het te installeren.");
    registerEntry("nl_NL", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland kon {count} essentieel bestand niet laden, geef de pakketbeheerder van uw distro de schuld voor slecht verpakkingswerk!";
        return "Hyprland kon {count} essentiële bestanden niet laden, geef de pakketbeheerder van uw distro de schuld voor slecht verpakkingswerk!";
    });
    registerEntry("nl_NL", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Uw monitorindeling is onjuist ingesteld. Monitor {name} overlapt met één of meerdere andere monitoren in de indeling.\n"
                  "Zie de wiki (Monitors pagina) voor meer informatie. Dit <b>zal</b> problemen veroorzaken.");
    registerEntry("nl_NL", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} is er niet in geslaagd om een van de aangevraagde modi toe te passen en gebruikt nu de modus {mode}.");
    registerEntry("nl_NL", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Ongeldige schaal opgegeven voor monitor {name}: {scale}, de voorgestelde schaal {fixed_scale} wordt gebruikt.");
    registerEntry("nl_NL", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Plugin {name} kon niet worden geladen: {error}");
    registerEntry("nl_NL", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Het opnieuw laden van de CM-shader is mislukt. Er wordt teruggevallen op rgba/rgbx.");
    registerEntry("nl_NL", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: breed kleurbereik is ingeschakeld maar het scherm staat niet in 10-bitmodus.");

    // pa_IN (Punjabi)
    registerEntry("pa_IN", TXT_KEY_ANR_TITLE, "ਐਪਲੀਕੇਸ਼ਨ ਜਵਾਬ ਨਹੀਂ ਦੇ ਰਹੀ");
    registerEntry("pa_IN", TXT_KEY_ANR_CONTENT, "ਇੱਕ ਐਪਲੀਕੇਸ਼ਨ {title} - {class} ਜਵਾਬ ਨਹੀਂ ਦੇ ਰਹੀ ਹੈ।\nਤੁਸੀਂ ਇਸ ਨਾਲ ਕੀ ਕਰਨਾ ਚਾਹੁੰਦੇ ਹੋ?");
    registerEntry("pa_IN", TXT_KEY_ANR_OPTION_TERMINATE, "ਬੰਦ ਕਰੋ");
    registerEntry("pa_IN", TXT_KEY_ANR_OPTION_WAIT, "ਉਡੀਕ ਕਰੋ");
    registerEntry("pa_IN", TXT_KEY_ANR_PROP_UNKNOWN, "(ਅਗਿਆਤ)");

    registerEntry("pa_IN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "ਇੱਕ ਐਪਲੀਕੇਸ਼ਨ <b>{app}</b> ਅਗਿਆਤ ਇਜਾਜ਼ਤ ਦੀ ਬੇਨਤੀ ਕਰ ਰਹੀ ਹੈ।");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "ਇੱਕ ਐਪਲੀਕੇਸ਼ਨ <b>{app}</b> ਤੁਹਾਡੀ ਸਕ੍ਰੀਨ ਨੂੰ ਰਿਕਾਰਡ ਕਰਨ ਦੀ ਕੋਸ਼ਿਸ਼ ਕਰ ਰਹੀ ਹੈ।\n\nਕੀ ਤੁਸੀਂ ਇਸਦੀ ਇਜਾਜ਼ਤ ਦੇਣਾ ਚਾਹੁੰਦੇ ਹੋ?");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_REQUEST_CURSOR_POS, "ਇੱਕ ਐਪਲੀਕੇਸ਼ਨ <b>{app}</b> ਤੁਹਾਡੇ ਕਰਸਰ ਦੀ ਸਥਿਤੀ ਪੜ੍ਹਨ ਦੀ ਕੋਸ਼ਿਸ਼ ਕਰ ਰਹੀ ਹੈ।\n\nਕੀ ਤੁਸੀਂ ਇਸਦੀ ਇਜਾਜ਼ਤ ਦੇਣਾ ਚਾਹੁੰਦੇ ਹੋ?");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "ਇੱਕ ਐਪਲੀਕੇਸ਼ਨ <b>{app}</b> ਇੱਕ ਪਲੱਗਇਨ ਲੋਡ ਕਰਨ ਦੀ ਕੋਸ਼ਿਸ਼ ਕਰ ਰਹੀ ਹੈ: <b>{plugin}</b>।\n\nਕੀ ਤੁਸੀਂ ਇਸਦੀ ਇਜਾਜ਼ਤ ਦੇਣਾ ਚਾਹੁੰਦੇ ਹੋ?");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "ਇੱਕ ਨਵਾਂ ਕੀਬੋਰਡ ਲੱਭਿਆ ਗਿਆ ਹੈ: <b>{keyboard}</b>।\n\nਕੀ ਤੁਸੀਂ ਇਸਨੂੰ ਕੰਮ ਕਰਨ ਦੀ ਇਜਾਜ਼ਤ ਦੇਣਾ ਚਾਹੁੰਦੇ ਹੋ?");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(ਅਗਿਆਤ)");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_TITLE, "ਇਜਾਜ਼ਤ ਦੀ ਬੇਨਤੀ");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "ਸੁਝਾਅ: ਤੁਸੀਂ ਆਪਣੀ Hyprland ਕੌਂਫਿਗਰੇਸ਼ਨ ਫਾਈਲ ਵਿੱਚ ਇਹਨਾਂ ਲਈ ਪੱਕੇ ਨਿਯਮ ਸੈੱਟ ਕਰ ਸਕਦੇ ਹੋ।");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_ALLOW, "ਇਜਾਜ਼ਤ ਦਿਓ");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "ਇਜਾਜ਼ਤ ਦਿਓ ਅਤੇ ਯਾਦ ਰੱਖੋ");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_ALLOW_ONCE, "ਇੱਕ ਵਾਰ ਇਜਾਜ਼ਤ ਦਿਓ");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_DENY, "ਇਨਕਾਰ ਕਰੋ");
    registerEntry("pa_IN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "ਅਗਿਆਤ ਐਪਲੀਕੇਸ਼ਨ (wayland ਕਲਾਇੰਟ ID {wayland_id})");

    registerEntry("pa_IN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "ਤੁਹਾਡਾ XDG_CURRENT_DESKTOP ਵਾਤਾਵਰਣ ਵੇਰੀਏਬਲ ਬਾਹਰੀ ਤੌਰ 'ਤੇ ਪ੍ਰਬੰਧਿਤ ਜਾਪਦਾ ਹੈ, ਮੌਜੂਦਾ ਮੁੱਲ: {value}।\nਜਦੋਂ ਤੱਕ ਇਹ ਜਾਣਬੁੱਝ ਕੇ ਨਾ "
                  "ਕੀਤਾ ਗਿਆ ਹੋਵੇ, ਇਹ ਸਮੱਸਿਆਵਾਂ ਪੈਦਾ ਕਰ ਸਕਦਾ ਹੈ।");
    registerEntry("pa_IN", TXT_KEY_NOTIF_NO_GUIUTILS, "ਤੁਹਾਡੇ ਸਿਸਟਮ ਵਿੱਚ hyprland-guiutils ਇੰਸਟਾਲ ਨਹੀਂ ਹੈ। ਇਹ ਕੁਝ ਡਾਇਲਾਗਸ ਲਈ ਜ਼ਰੂਰੀ ਹੈ। ਕਿਰਪਾ ਕਰਕੇ ਇਸਨੂੰ ਇੰਸਟਾਲ ਕਰਨ ਬਾਰੇ ਵਿਚਾਰ ਕਰੋ।");
    registerEntry("pa_IN", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland {count} ਜ਼ਰੂਰੀ ਸੰਪਤੀ ਨੂੰ ਲੋਡ ਨਹੀਂ ਕਰ ਸਕਿਆ, ਖਰਾਬ ਪੈਕੇਜਿੰਗ ਲਈ ਆਪਣੇ ਡਿਸਟ੍ਰੋ ਪੈਕੇਜਰਾਂ ਨੂੰ ਦੋਸ਼ੀ ਠਹਿਰਾਓ!";
        return "Hyprland {count} ਜ਼ਰੂਰੀ ਸੰਪਤੀਆਂ ਨੂੰ ਲੋਡ ਨਹੀਂ ਕਰ ਸਕਿਆ, ਖਰਾਬ ਪੈਕੇਜਿੰਗ ਲਈ ਆਪਣੇ ਡਿਸਟ੍ਰੋ ਪੈਕੇਜਰਾਂ ਨੂੰ ਦੋਸ਼ੀ ਠਹਿਰਾਓ!";
    });
    registerEntry("pa_IN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "ਤੁਹਾਡਾ ਮਾਨੀਟਰ ਲੇਆਉਟ ਗਲਤ ਤਰੀਕੇ ਨਾਲ ਕੌਂਫਿਗਰ ਕੀਤਾ ਗਿਆ ਹੈ। ਮਾਨੀਟਰ {name} ਲੇਆਉਟ ਵਿੱਚ ਦੂਜੇ ਮਾਨੀਟਰਾਂ ਨਾਲ ਓਵਰਲੈਪ ਕਰ ਰਿਹਾ ਹੈ।\nਹੋਰ "
                  "ਜਾਣਕਾਰੀ ਲਈ ਵਿਕੀ (Monitors page) ਦੇਖੋ। ਇਸ ਨਾਲ ਸਮੱਸਿਆਵਾਂ <b>ਪੈਦਾ ਹੋਣਗੀਆਂ</b>।");
    registerEntry("pa_IN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "ਮਾਨੀਟਰ {name} ਕਿਸੇ ਵੀ ਬੇਨਤੀ ਕੀਤੇ ਮੋਡ ਨੂੰ ਸੈੱਟ ਕਰਨ ਵਿੱਚ ਅਸਫਲ ਰਿਹਾ, ਵਾਪਸ ਮੋਡ {mode} 'ਤੇ ਜਾ ਰਿਹਾ ਹੈ।");
    registerEntry("pa_IN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "ਮਾਨੀਟਰ {name} ਨੂੰ ਅਵੈਧ ਸਕੇਲ ਭੇਜਿਆ ਗਿਆ: {scale}, ਪ੍ਰਸਤਾਵਿਤ ਸਕੇਲ ਦੀ ਵਰਤੋਂ ਕਰ ਰਿਹਾ ਹੈ: {fixed_scale}");
    registerEntry("pa_IN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "ਪਲੱਗਇਨ {name} ਲੋਡ ਕਰਨ ਵਿੱਚ ਅਸਫਲ: {error}");
    registerEntry("pa_IN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM ਸ਼ੇਡਰ ਮੁੜ-ਲੋਡ ਕਰਨ ਵਿੱਚ ਅਸਫਲ, rgba/rgbx 'ਤੇ ਵਾਪਸ ਜਾ ਰਿਹਾ ਹੈ।");
    registerEntry("pa_IN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "ਮਾਨੀਟਰ {name}: ਵਾਈਡ ਕਲਰ ਗੇਮਟ ਚਾਲੂ ਹੈ ਪਰ ਡਿਸਪਲੇ 10-ਬਿੱਟ ਮੋਡ ਵਿੱਚ ਨਹੀਂ ਹੈ।");
    registerEntry("pa_IN", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland ਨੂੰ start-hyprland ਤੋਂ ਬਿਨਾਂ ਸ਼ੁਰੂ ਕੀਤਾ ਗਿਆ ਸੀ। ਜਦੋਂ ਤੱਕ ਤੁਸੀਂ ਡੀਬੱਗਿੰਗ ਵਾਤਾਵਰਣ ਵਿੱਚ ਨਹੀਂ ਹੋ, ਇਸਦੀ ਸਿਫਾਰਸ਼ ਨਹੀਂ ਕੀਤੀ ਜਾਂਦੀ।");

    registerEntry("pa_IN", TXT_KEY_SAFE_MODE_TITLE, "ਸੁਰੱਖਿਅਤ ਮੋਡ (Safe Mode)");
    registerEntry("pa_IN", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland ਸੁਰੱਖਿਅਤ ਮੋਡ ਵਿੱਚ ਸ਼ੁਰੂ ਕੀਤਾ ਗਿਆ ਹੈ, ਜਿਸਦਾ ਮਤਲਬ ਹੈ ਕਿ ਤੁਹਾਡਾ ਪਿਛਲਾ ਸੈਸ਼ਨ ਕ੍ਰੈਸ਼ ਹੋ ਗਿਆ ਸੀ।\nਸੁਰੱਖਿਅਤ ਮੋਡ ਤੁਹਾਡੀ "
                  "ਕੌਂਫਿਗਰੇਸ਼ਨ ਨੂੰ ਲੋਡ ਹੋਣ ਤੋਂ ਰੋਕਦਾ ਹੈ। ਤੁਸੀਂ ਇਸ ਵਾਤਾਵਰਣ ਵਿੱਚ ਸਮੱਸਿਆ ਦਾ ਨਿਪਟਾਰਾ ਕਰ ਸਕਦੇ ਹੋ, ਜਾਂ ਹੇਠਾਂ ਦਿੱਤੇ ਬਟਨ ਨਾਲ ਆਪਣੀ "
                  "ਕੌਂਫਿਗਰੇਸ਼ਨ ਲੋਡ ਕਰ ਸਕਦੇ ਹੋ।\nਡਿਫੌਲਟ ਕੀਬਾਈਂਡ ਲਾਗੂ ਹੁੰਦੇ ਹਨ: kitty ਲਈ SUPER+Q, ਬੇਸਿਕ ਰਨਰ ਲਈ SUPER+R, ਬਾਹਰ ਨਿਕਲਣ ਲਈ SUPER+M।\n"
                  "Hyprland ਨੂੰ ਦੁਬਾਰਾ ਸ਼ੁਰੂ ਕਰਨ ਨਾਲ ਇਹ ਮੁੜ ਆਮ ਮੋਡ ਵਿੱਚ ਚੱਲੇਗਾ।");
    registerEntry("pa_IN", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "ਕੌਂਫਿਗ ਲੋਡ ਕਰੋ");
    registerEntry("pa_IN", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "ਕ੍ਰੈਸ਼ ਰਿਪੋਰਟ ਡਾਇਰੈਕਟਰੀ ਖੋਲ੍ਹੋ");
    registerEntry("pa_IN", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "ਸਮਝ ਆ ਗਿਆ, ਇਸਨੂੰ ਬੰਦ ਕਰੋ");

    // pl_PL (Polish)
    registerEntry("pl_PL", TXT_KEY_ANR_TITLE, "Aplikacja Nie Odpowiada");
    registerEntry("pl_PL", TXT_KEY_ANR_CONTENT, "Aplikacja {title} - {class} nie odpowiada.\nCo chcesz z nią zrobić?");
    registerEntry("pl_PL", TXT_KEY_ANR_OPTION_TERMINATE, "Zakończ proces");
    registerEntry("pl_PL", TXT_KEY_ANR_OPTION_WAIT, "Czekaj");
    registerEntry("pl_PL", TXT_KEY_ANR_PROP_UNKNOWN, "(nieznane)");

    registerEntry("pl_PL", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Aplikacja <b>{app}</b> prosi o pozwolenie na nieznany typ operacji.");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Aplikacja <b>{app}</b> prosi o dostęp do twojego ekranu.\n\nCzy chcesz jej na to pozwolić?");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Aplikacja <b>{app}</b> próbuje załadować plugin: <b>{plugin}</b>.\n\nCzy chcesz jej na to pozwolić?");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Wykryto nową klawiaturę: <b>{keyboard}</b>.\n\nCzy chcesz jej pozwolić operować?");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(nieznane)");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_TITLE, "Prośba o pozwolenie");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Podpowiedź: możesz ustawić stałe zasady w konfiguracji Hyprland'a.");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_ALLOW, "Zezwól");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Zezwól i zapamiętaj");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_ALLOW_ONCE, "Zezwól raz");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_DENY, "Odmów");
    registerEntry("pl_PL", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Nieznana aplikacja (ID klienta wayland {wayland_id})");

    registerEntry("pl_PL", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Zmienna środowiska XDG_CURRENT_DESKTOP została ustawiona zewnętrznie na {value}.\nTo może sprawić problemy, chyba, że jest celowe.");
    registerEntry("pl_PL", TXT_KEY_NOTIF_NO_GUIUTILS, "Twój system nie ma hyprland-guiutils zainstalowanych, co może sprawić problemy. Zainstaluj pakiet.");
    registerEntry("pl_PL", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo == 1)
            return "Nie udało się załadować {count} kluczowego zasobu, wiń swojego packager'a za robienie słabej roboty!";

        return "Nie udało się załadować {count} kluczowych zasobów, wiń swojego packager'a za robienie słabej roboty!";
    });
    registerEntry("pl_PL", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Pozycje twoich monitorów nie są ustawione poprawnie. Monitor {name} wchodzi na inne monitory.\nWejdź na wiki (stronę Monitory) "
                  "po więcej. To <b>będzie</b> sprawiać problemy.");
    registerEntry("pl_PL", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} nie zaakceptował żadnego wybranego programu. Użyto {mode}.");
    registerEntry("pl_PL", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Nieprawidłowa skala dla monitora {name}: {scale}, użyto proponowanej skali: {fixed_scale}");
    registerEntry("pl_PL", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Nie udało się załadować plugin'a {name}: {error}");
    registerEntry("pl_PL", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Nie udało się przeładować shader'a CM, użyto rgba/rgbx.");
    registerEntry("pl_PL", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: skonfigurowano szeroką głębię barw, ale monitor nie jest w trybie 10-bit.");
    registerEntry("pl_PL", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland został uruchomiony bez start-hyprland. Nie jest to zalecane, chyba, że jest to środowisko do debugowania.");

    registerEntry("pl_PL", TXT_KEY_SAFE_MODE_TITLE, "Tryb Bezpieczny");
    registerEntry("pl_PL", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland został uruchomiony w trybie bezpiecznym, co oznacza, że twoja ostatnia sesja uległa awarii.\nTryb bezpieczny zapobiega ładowaniu twojej "
                  "konfiguracji. Możesz próbować rozwiązać"
                  "problem w tym środowisku, lub załadować swoją konfigurację przyciskiem poniżej.\nDomyślne skróty klawiszowe są dostępne: SUPER+Q uruchamia kitty, "
                  "SUPER+R otwiera podstawowy launcher, SUPER+M zamyka Hyprland.\nUruchomienie ponowne Hyprland'a uruchomi go w trybie normalnym.");
    registerEntry("pl_PL", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Załaduj konfigurację");
    registerEntry("pl_PL", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Otwórz folder z raportami awarii");
    registerEntry("pl_PL", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "Ok, zamknij to okno");

    // pt_PT (Portuguese Portugal)
    registerEntry("pt_PT", TXT_KEY_ANR_TITLE, "A aplicação não está a responder");
    registerEntry("pt_PT", TXT_KEY_ANR_CONTENT, "Uma aplicação {title} - {class} não está a responder.\nO que pretendes fazer com ela?");
    registerEntry("pt_PT", TXT_KEY_ANR_OPTION_TERMINATE, "Terminar");
    registerEntry("pt_PT", TXT_KEY_ANR_OPTION_WAIT, "Esperar");
    registerEntry("pt_PT", TXT_KEY_ANR_PROP_UNKNOWN, "(desconhecido)");

    registerEntry("pt_PT", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Uma aplicação <b>{app}</b> está a pedir uma permissão desconhecida.");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Uma aplicação <b>{app}</b> está a tentar fazer uma captura do ecrã.\n\nQueres permiti-lo?");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "A aplicação <b>{app}</b> está a tentar carregar o plugin: <b>{plugin}</b>.\n\nQueres permiti-lo?");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Um novo teclado foi detectado: <b>{keyboard}</b>.\n\nQueres permitir a sua operação?");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(desconhecido)");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_TITLE, "Pedido de permissão");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Dica: podes definir regras persistentes para estes no ficheiro de configuração do Hyprland.");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_ALLOW, "Permitir");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Permitir sempre");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_ALLOW_ONCE, "Permitir esta vez");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_DENY, "Recusar");
    registerEntry("pt_PT", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Aplicação desconhecida (ID de cliente wayland {wayland_id})");

    registerEntry(
        "pt_PT", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "O teu ambiente XDG_CURRENT_DESKTOP parece estar a ser gerido externamente, e o valor actual é {value}.\nIsto pode causar problemas a não ser que seja intencional.");
    registerEntry("pt_PT", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "O teu sistema não tem o hyprland-guiutils instalado. Esta dependência de runtime é necessária para algumas caixas de diálogo, deverias instalá-la.");
    registerEntry("pt_PT", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland não conseguiu carregar {count} asset essencial, podes culpar o gestor de dependências da tua distro por fazer um mau trabalho!";
        return "Hyprland não conseguiu carregar {count} assets essenciais, podes culpar o gestor de dependências da tua distro por fazer um mau trabalho!";
    });
    registerEntry(
        "pt_PT", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "O layout do teu monitor não está configurado correctamente. Monitor {name} está em conflito com outro(s) monitor(es) no layout.\nProcura na wiki (página Monitores) para "
        "mais informações. Isto <b>vai</b> causar problemas.");
    registerEntry("pt_PT", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} falhou ao configurar os modos requisitados, revertento para o modo {mode} de volta.");
    registerEntry("pt_PT", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Resolução inválida para o monitor {name}: {scale}, revertendo para a resolução sugerida: {fixed_scale}");
    registerEntry("pt_PT", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Falha ao carregar o plugin {name}: {error}");
    registerEntry("pt_PT", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader falhou ao recarregar, revertendo para rgba/rgbx.");
    registerEntry("pt_PT", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: gama de cores ampla está activada mas o monitor não está em modo 10-bits.");

    // zh_CN (Simplified Chinese)
    registerEntry("zh_CN", TXT_KEY_ANR_TITLE, "应用程序未响应");
    registerEntry("zh_CN", TXT_KEY_ANR_CONTENT, "应用程序 {title} - {class} 未响应。\n你想要采取什么行动？");
    registerEntry("zh_CN", TXT_KEY_ANR_OPTION_TERMINATE, "终止");
    registerEntry("zh_CN", TXT_KEY_ANR_OPTION_WAIT, "等待");
    registerEntry("zh_CN", TXT_KEY_ANR_PROP_UNKNOWN, "（未知）");

    registerEntry("zh_CN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "应用程序 <b>{app}</b> 正在请求一个未知的权限。");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "应用程序 <b>{app}</b> 想要捕获你的屏幕。\n\n允许它这么做吗？");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "应用程序 <b>{app}</b> 想要加载插件： <b>{plugin}</b>。\n\n允许它这么做吗？");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "检测到新的键盘 <b>{keyboard}</b> 接入了。\n\n允许这个键盘操作你的系统吗？");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "（未知）");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_TITLE, "权限请求");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "提示：你可以在Hyprland配置中为他们创建永久性的规则。");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_ALLOW, "允许");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "总是允许");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_ALLOW_ONCE, "允许一次");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_DENY, "阻止");
    registerEntry("zh_CN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "未知的应用程序 （Wayland客户端ID {wayland_id}）");

    registerEntry("zh_CN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP, "你的环境变量XDG_CURRENT_DESKTOP似乎被外部管理，且当前的值为{value}。如果你不是有意这么做，这可能会导致问题。");
    registerEntry("zh_CN", TXT_KEY_NOTIF_NO_GUIUTILS, "你的系统似乎没有安装hyprland-guiutils。这是一个用于部分对话框的运行时依赖。请考虑安装。");
    registerEntry("zh_CN", TXT_KEY_NOTIF_FAILED_ASSETS, "Hyprland无法加载{count}个重要资产，问问你发行版的打包者在打包个什么玩意！？");
    registerEntry("zh_CN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "你的显示器没有被正确设置。显示器 {name} 和其他显示器的布局重叠了。请看wiki中的“显示器”一章获取更多信息。这<b>会</b>导致问题。");
    registerEntry("zh_CN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "显示器 {name} 无法被设置为任何请求的模式，将使用 {mode} 兜底。");
    registerEntry("zh_CN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "显示器 {name} 被设置了非法的缩放：{scale}，将使用建议的缩放：{fixed_scale}");
    registerEntry("zh_CN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "无法加载插件 {name}：{error}");
    registerEntry("zh_CN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "无法重新加载CM着色器，将使用rgba/rgbx兜底。");
    registerEntry("zh_CN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "显示器 {name}：宽色域被启用了，但是显示器并不在10-bit模式。");
    registerEntry("zh_CN", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland 启动时未使用 start-hyprland。除非你处于调试环境，否则极度不推荐这样做。");

    registerEntry("zh_CN", TXT_KEY_SAFE_MODE_TITLE, "安全模式");
    registerEntry("zh_CN", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland "
                  "已在安全模式下启动，这意味着你上次会话崩溃了。\n安全模式会阻止加载你的配置。你可以在此环境中进行故障排除，或者使用下方按钮加载你的配置。\n默认快"
                  "捷键适用：SUPER+Q 打开 Kitty，SUPER+R 打开简易启动器，SUPER+M 退出。\n重新启动 "
                  "Hyprland 将再次进入正常模式。");
    registerEntry("zh_CN", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "加载配置");
    registerEntry("zh_CN", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "打开崩溃报告目录");
    registerEntry("zh_CN", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "好的，关闭窗口");

    // zh_TW (Traditional Chinese)
    registerEntry("zh_TW", TXT_KEY_ANR_TITLE, "應用程式沒有回應");
    registerEntry("zh_TW", TXT_KEY_ANR_CONTENT, "應用程式 {title} - {class} 沒有回應。\n您想要怎麼做？");
    registerEntry("zh_TW", TXT_KEY_ANR_OPTION_TERMINATE, "強制結束");
    registerEntry("zh_TW", TXT_KEY_ANR_OPTION_WAIT, "等待");
    registerEntry("zh_TW", TXT_KEY_ANR_PROP_UNKNOWN, "（未知）");

    registerEntry("zh_TW", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "應用程式 <b>{app}</b> 正在請求未知的權限。");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "應用程式 <b>{app}</b> 試圖擷取您的螢幕畫面。\n\n您是否允許？");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "應用程式 <b>{app}</b> 試圖載入外掛：<b>{plugin}</b>。\n\n您是否允許？");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "偵測到新鍵盤：<b>{keyboard}</b>。\n\n您是否允許它進行操作？");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_UNKNOWN_NAME, "（未知）");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_TITLE, "權限請求");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "提示：您可以在 Hyprland 設定檔中為此建立永久規則。");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_ALLOW, "允許");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "總是允許");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_ALLOW_ONCE, "僅允許一次");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_DENY, "拒絕");
    registerEntry("zh_TW", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "未知的應用程式 （Wayland 用戶端 ID {wayland_id}）");

    registerEntry("zh_TW", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP, "您的 XDG_CURRENT_DESKTOP 環境變數似乎由外部管理，目前的值為 {value}。\n除非您有意為之，否則這可能會導致問題。");
    registerEntry("zh_TW", TXT_KEY_NOTIF_NO_GUIUTILS, "您的系統未安裝 hyprland-guiutils。這是部分對話視窗的執行期依賴元件。建議您安裝它。");
    registerEntry("zh_TW", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland 無法載入 {count} 個必要資源，去怪那個把發行版打包成這副德性的維護者！";
        return "Hyprland 無法載入 {count} 個必要資源，去怪那個把發行版打包成這副德性的維護者！";
    });
    registerEntry("zh_TW", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "您的螢幕配置設定不正確。螢幕 {name} 與配置中的其他螢幕重疊了。\n請參閱 Wiki（螢幕頁面）以了解詳情。這<b>絕對會</b>導致問題。");
    registerEntry("zh_TW", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "螢幕 {name} 無法設定為任何請求的模式，將改用模式 {mode}。");
    registerEntry("zh_TW", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "傳遞給螢幕 {name} 的縮放比例無效：{scale}，將使用建議的比例：{fixed_scale}");
    registerEntry("zh_TW", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "無法載入外掛 {name}：{error}");
    registerEntry("zh_TW", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM 著色器重新載入失敗，將退回使用 rgba/rgbx。");
    registerEntry("zh_TW", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "螢幕 {name}：已啟用廣色域，但顯示器並非處於 10-bit 模式。");
    registerEntry("zh_TW", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland 啟動時未使用 start-hyprland wrapper。除非您處於除錯環境，否則極度不建議這麼做。");

    registerEntry("zh_TW", TXT_KEY_SAFE_MODE_TITLE, "安全模式");
    registerEntry("zh_TW", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland "
                  "已在安全模式下啟動，這代表您的上個工作階段當機。\n安全模式會阻止載入您的設定檔。您可以在此環境中進行故障排除，或使用下方按鈕載入您的設定。\n預設快"
                  "捷鍵適用：SUPER+Q 開啟 Kitty，SUPER+R 開啟簡易啟動器，SUPER+M 退出。\n重新啟動 "
                  "Hyprland 將再次進入正常模式。");
    registerEntry("zh_TW", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "載入設定檔");
    registerEntry("zh_TW", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "開啟當機報告目錄");
    registerEntry("zh_TW", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "好，關閉視窗");

    // ar (Arabic - Modern Standard)
    registerEntry("ar", TXT_KEY_ANR_TITLE, "التطبيق لا يستجيب");
    registerEntry("ar", TXT_KEY_ANR_CONTENT, "التطبيق {title} - {class} لا يستجيب.\nما الذي تريد فعله؟");
    registerEntry("ar", TXT_KEY_ANR_OPTION_TERMINATE, "إنهاء");
    registerEntry("ar", TXT_KEY_ANR_OPTION_WAIT, "الانتظار");
    registerEntry("ar", TXT_KEY_ANR_PROP_UNKNOWN, "(غير معروف)");

    registerEntry("ar", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "يطلب التطبيق <b>{app}</b> صلاحية غير معروفة.");
    registerEntry("ar", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "يحاول التطبيق <b>{app}</b> التقاط الشاشة.\n\nهل تريد السماح له بذلك؟");
    registerEntry("ar", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "يحاول التطبيق <b>{app}</b> تحميل إضافة: <b>{plugin}</b>.\n\nهل تريد السماح له بذلك؟");
    registerEntry("ar", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "تم اكتشاف لوحة مفاتيح جديدة: <b>{keyboard}</b>.\n\nهل تريد السماح لها بالعمل؟");
    registerEntry("ar", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(غير معروف)");
    registerEntry("ar", TXT_KEY_PERMISSION_TITLE, "طلب الإذن");
    registerEntry("ar", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "تلميح: يمكنك تعيين قواعد دائمة لهذه الطلبات في ملف إعدادات Hyprland.");
    registerEntry("ar", TXT_KEY_PERMISSION_ALLOW, "السماح");
    registerEntry("ar", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "السماح مع تذكّر الاختيار");
    registerEntry("ar", TXT_KEY_PERMISSION_ALLOW_ONCE, "السماح لمرة واحدة");
    registerEntry("ar", TXT_KEY_PERMISSION_DENY, "الرفض");
    registerEntry("ar", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "تطبيق غير معروف (معرّف عميل Wayland {wayland_id})");

    registerEntry("ar", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "يبدو أنّ متغيّر البيئة XDG_CURRENT_DESKTOP يُدار من خارج النظام، والقيمة الحالية هي {value}.\n"
                  "قد يؤدي ذلك إلى مشكلات ما لم يكن مقصودًا.");
    registerEntry("ar", TXT_KEY_NOTIF_NO_GUIUTILS, "لا يحتوي نظامك على الحزمة hyprland-guiutils مثبتة. هذه حزمة مطلوبة أثناء التشغيل لبعض مربعات الحوار. يُنصَح بتثبيتها.");
    registerEntry("ar", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "فشل Hyprland في تحميل مورد أساسي ({count}). قد يكون السبب سوء تغليف الحزم في التوزيعة.";
        return "فشل Hyprland في تحميل {count} من الموارد الأساسية. قد يكون السبب سوء تغليف الحزم في التوزيعة.";
    });
    registerEntry("ar", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "تم إعداد مخطط الشاشات لديك بشكل غير صحيح. الشاشة {name} تتداخل مع شاشة أو أكثر في المخطط.\n"
                  "يرجى مراجعة صفحة الشاشات في الويكي لمزيد من التفاصيل. هذا <b>سيسبب</b> مشكلات.");
    registerEntry("ar", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "فشلت الشاشة {name} في ضبط أي من الأوضاع المطلوبة، وسيتم الرجوع إلى الوضع {mode}.");
    registerEntry("ar", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "تم تمرير قيمة تحجيم غير صالحة إلى الشاشة {name}: {scale}. سيتم استخدام قيمة التحجيم المقترحة: {fixed_scale}.");
    registerEntry("ar", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "فشل تحميل الإضافة {name}: {error}");
    registerEntry("ar", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "فشلت إعادة تحميل نظام إدارة الألوان (CM). سيتم الرجوع إلى صيغة الألوان rgba/rgbx.");
    registerEntry("ar", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "الشاشة {name}: تم تفعيل نطاق الألوان الواسع، لكن العرض ليس في وضع 10 بت.");

    registerEntry("ar", TXT_KEY_SAFE_MODE_TITLE, "الوضع الآمن");
    registerEntry("ar", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "شُغل Hyprland في الوضع الآمن، هذا يعني أن جلستك الأخيرة قد انهارت.\nالوضع الآمن يمنع تحميل إعداداتك، "
                  "يمكنك البحث عن وحل المشاكل في هذه البيئة، أو تحميل إعداداتك باستخدام الزر أدناه.\n اختصارات المفاتيح الافتراضية: الطرفية (Kitty) - SUPER+Q، مشغّل "
                  "الأوامر البسيط - SUPER+R، الخروج - SUPER+M.\n"
                  "إعادة تشغيل Hyprland سيشغله في الوضع العادي");
    registerEntry("ar", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "حمل ملف الإعدادات");
    registerEntry("ar", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "افتح مجلد تقرير الانهيار");
    registerEntry("ar", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "حسنًا، أغلق هذا");

    // ro_RO (Romanian)
    registerEntry("ro_RO", TXT_KEY_ANR_TITLE, "Aplicația Nu Răspunde");
    registerEntry("ro_RO", TXT_KEY_ANR_CONTENT, "O aplicație {title} - {class} nu răspunde.\nCe vrei să faci cu ea?");
    registerEntry("ro_RO", TXT_KEY_ANR_OPTION_TERMINATE, "Închide");
    registerEntry("ro_RO", TXT_KEY_ANR_OPTION_WAIT, "Așteaptă");
    registerEntry("ro_RO", TXT_KEY_ANR_PROP_UNKNOWN, "(necunoscut)");

    registerEntry("ro_RO", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "O aplicație <b>{app}</b> solicită o permisiune necunoscută.");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "O aplicație <b>{app}</b> încearcă să captureze ecranul.\n\nDorești să îi permiți acest lucru?");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "O aplicație <b>{app}</b> încearcă să încarce un plugin: <b>{plugin}</b>.\n\nDorești să îi permiți acest lucru?");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "A fost detectată o tastatură nouă: <b>{keyboard}</b>.\n\nDorești să îi permiți să funcționeze?");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(necunoscut)");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_TITLE, "Cerere de permisiune");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Indiciu: poți seta reguli persistente pentru acestea în fișierul de configurare Hyprland.");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_ALLOW, "Permite");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Permite și reține");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_ALLOW_ONCE, "Permite o dată");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_DENY, "Respinge");
    registerEntry("ro_RO", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Aplicație necunoscută (ID client wayland {wayland_id})");

    registerEntry("ro_RO", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Se pare că mediul tău XDG_CURRENT_DESKTOP este gestionat extern, iar valoarea curentă este {value}.\nAcest lucru ar putea cauza probleme, cu excepția "
                  "cazului în care este intenționat.");
    registerEntry("ro_RO", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Sistemul tău nu are instalat hyprland-guiutils. Aceasta este o dependență de execuție pentru anumite dialoguri. Ia în considerare instalarea acesteia.");
    registerEntry("ro_RO", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo == 1)
            return "Hyprland nu a reușit să încarce un element esențial. Dă vina pe packager-ul distro-ului tău că a făcut o treabă proastă la ambalare!";
        return "Hyprland nu a reușit să încarce {count} elemente esențiale. Dă vina pe packager-ul distro-ului tău că a făcut o treabă proastă la ambalare!";
    });
    registerEntry("ro_RO", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Configurația monitorului este incorectă. Monitorul {name} se suprapune cu alte monitoare.\nConsultați wiki-ul (pagina Monitoare) pentru "
                  "mai multe informații. Acest lucru <b>va cauza</b> probleme.");
    registerEntry("ro_RO", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitorul {name} nu a reușit să seteze niciun mod solicitat, revenind la modul {mode}.");
    registerEntry("ro_RO", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Scară nevalidă transmisă monitorului {name}: {scale}, se utilizează scara sugerată: {fixed_scale}");
    registerEntry("ro_RO", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Nu s-a putut încărca pluginul {name}: {error}");
    registerEntry("ro_RO", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Reîncărcarea shaderului CM a eșuat, revenind la rgba/rgbx.");
    registerEntry("ro_RO", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: gama largă de culori este activată, dar afișajul nu este în modul pe 10 biți.");
    registerEntry("ro_RO", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland a fost pornit fără start-hyprland. Acest lucru nu este recomandat decât dacă te afli într-un mediu de depanare.");

    registerEntry("ro_RO", TXT_KEY_SAFE_MODE_TITLE, "Modul de Siguranță");
    registerEntry(
        "ro_RO", TXT_KEY_SAFE_MODE_DESCRIPTION,
        "Hyprland a fost lansat în modul de siguranță, ceea ce înseamnă că ultima sesiune s-a blocat.\nModul de siguranță împiedică încărcarea configurației. Poți "
        "depana în acest mediu sau să încarci configurația cu butonul de mai jos.\nSe aplică combinațiile de taste implicite: SUPER+Q pentru kitty, SUPER+R pentru un runner de "
        "bază."
        "SUPER+M pentru ieșire.\nLa repornire "
        "Hyprland se va lansa din nou în modul normal.");
    registerEntry("ro_RO", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Încarcă configurația");
    registerEntry("ro_RO", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Deschide locația rapoartelor de crash-uri");
    registerEntry("ro_RO", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "Ok, închide");

    // ru_RU (Russian)
    registerEntry("ru_RU", TXT_KEY_ANR_TITLE, "Приложение не отвечает");
    registerEntry("ru_RU", TXT_KEY_ANR_CONTENT, "Приложение {title} - {class} не отвечает.\nЧто вы хотите сделать?");
    registerEntry("ru_RU", TXT_KEY_ANR_OPTION_TERMINATE, "Завершить");
    registerEntry("ru_RU", TXT_KEY_ANR_OPTION_WAIT, "Подождать");
    registerEntry("ru_RU", TXT_KEY_ANR_PROP_UNKNOWN, "(неизвестно)");

    registerEntry("ru_RU", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Приложение <b>{app}</b> запрашивает неизвестное разрешение.");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Приложение <b>{app}</b> пытается получить доступ к вашему экрану.\n\nРазрешить?");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Приложение <b>{app}</b> пытается загрузить плагин: <b>{plugin}</b>.\n\nРазрешить?");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Обнаружена новая клавиатура: <b>{keyboard}</b>.\n\nРазрешить ей работать?");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(неизвестно)");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_TITLE, "Запрос разрешения");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Подсказка: вы можете настроить постоянные правила для этого в конфигурационном файле Hyprland.");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_ALLOW, "Разрешить");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Разрешить и запомнить");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_ALLOW_ONCE, "Разрешить в этот раз");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_DENY, "Отклонить");
    registerEntry("ru_RU", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Неизвестное приложение (wayland client ID {wayland_id})");

    registerEntry("ru_RU", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Переменная окружения XDG_CURRENT_DESKTOP установлена извне, текущее значение: {value}.\nЭто может вызвать проблемы, если только это не сделано намеренно.");
    registerEntry("ru_RU", TXT_KEY_NOTIF_NO_GUIUTILS, "Пакет hyprland-guiutils не установлен. Он необходим для некоторых диалогов. Рекомендуется установить его.");
    registerEntry("ru_RU", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Не удалось загрузить {count} критически важный ресурс, пожалуйтесь мейнтейнеру вашего дистрибутива за кривую сборку пакета!";
        return "Не удалось загрузить {count} критически важных ресурсов, пожалуйтесь мейнтейнеру вашего дистрибутива за кривую сборку пакета!";
    });
    registerEntry(
        "ru_RU", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Неправильно настроен макет мониторов. Монитор {name} перекрывает другие.\nПодробнее см. в документации (страница Monitors). Это <b>обязательно</b> вызовет проблемы.");
    registerEntry("ru_RU", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Монитор {name} не смог установить ни один из запрошенных режимов, выбран режим {mode}.");
    registerEntry("ru_RU", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Недопустимый масштаб для монитора {name}: {scale}, используется предложенный масштаб: {fixed_scale}");
    registerEntry("ru_RU", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Не удалось загрузить плагин {name}: {error}");
    registerEntry("ru_RU", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Не удалось перезагрузить CM shader, используется rgba/rgbx.");
    registerEntry("ru_RU", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Монитор {name}: расширенный цветовой охват включён, но дисплей не в 10-bit режиме.");
    registerEntry("ru_RU", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland был запущен без start-hyprland. Это крайне не рекомендуется, если только вы не в отладочной среде.");

    registerEntry("ru_RU", TXT_KEY_SAFE_MODE_TITLE, "Безопасный режим");
    registerEntry("ru_RU", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland запущен в безопасном режиме, это значит, что ваш прошлый сеанс завершился сбоем.\nБезопасный режим не загружает ваш конфиг. Вы можете "
                  "исправить проблему в этом окружении или загрузить конфиг кнопкой ниже.\nДействуют стандартные бинды: SUPER+Q запускает kitty, SUPER+R открывает лаунчер, "
                  "SUPER+M для выхода.\nПосле перезапуска Hyprland снова запустится в обычном режиме.");
    registerEntry("ru_RU", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Загрузить конфиг");
    registerEntry("ru_RU", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Открыть каталог отчётов о сбоях");
    registerEntry("ru_RU", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "Ок, закрыть");

    // sl_SI (Slovenian)
    registerEntry("sl_SI", TXT_KEY_ANR_TITLE, "Program se ne odziva");
    registerEntry("sl_SI", TXT_KEY_ANR_CONTENT, "Program {title} - {class} se ne odziva.\nKaj želite storiti?");
    registerEntry("sl_SI", TXT_KEY_ANR_OPTION_TERMINATE, "Prekini");
    registerEntry("sl_SI", TXT_KEY_ANR_OPTION_WAIT, "Počakaj");
    registerEntry("sl_SI", TXT_KEY_ANR_PROP_UNKNOWN, "(neznano)");

    registerEntry("sl_SI", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Program <b>{app}</b> zahteva neznano dovoljenje.");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Program <b>{app}</b> poskuša zajeti vaš zaslon.\n\nAli mu želite to dovoliti?");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Program <b>{app}</b> skuša naložiti vtičnik: <b>{plugin}</b>.\n\nAli mu želite to dovoliti?");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Nova tipkovnica je bila zaznana: <b>{keyboard}</b>.\n\nAli ji želite dovoliti delovanje?");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(neznano)");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_TITLE, "Zahteva za dovoljenje");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Namig: v Hyprlandovi konfiguracijski datoteki lahko nastavite stalna pravila za dovoljenja.");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_ALLOW, "Dovoli");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Dovoli in si zapomni");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_ALLOW_ONCE, "Dovoli enkrat");
    registerEntry("sl_SI", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Neznan program (ID stranke Wayland: {wayland_id})");

    registerEntry("sl_SI", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Zdi se, da je Vaše okolje XDG_CURRENT_DESKTOP upravljano od zunaj, trenutna vrednost je {value}.\nTo lahko povzroči težave, če ni namerno.");
    registerEntry("sl_SI", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "hyprland-guiutils ni nameščen na vaši napravi. To je odvisnost od izvajalnega okolja za nekatera pogovorna okna. Razmislite o namestitvi.");
    registerEntry("sl_SI", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assertNo = std::stoi(vars.at("count"));
        if (assertNo <= 1)
            return "Hyprlandu ni uspelo naložiti {count} bistvenega sredstva, krivite upravitelja paketov vaše distribucije za slabo opravljeno pakiranje!";
        return "Hyprlandu ni uspelo naložiti {count} bistvenih sredstev, krivite upravitelja paketov vaše distribucije za slabo opravljeno pakiranje!";
    });
    registerEntry("sl_SI", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Vaša zaslonska razporeditev je napačno nastavljena. Zaslon {monitor} se prekriva z drugimi zasloni v razporeditvi.\n"
                  "Prosimo poglejte wiki (stran Monitors) za več. To <b>bo</b> povzročilo težave.");
    registerEntry("sl_SI", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Zaslon {name} ni uspel nastaviti nobenega zahtevanega načina, vrnitev k načinu {mode}.");
    registerEntry("sl_SI", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Neveljavna skala je bila posredovana zaslonu {name}: {scale}, uporabljena je predlagana skala: {fixed_scale}");
    registerEntry("sl_SI", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Vtičnika {name} ni bilo mogoče naložiti: {error}");
    registerEntry("sl_SI", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Ponovno nalaganje senčnika CM ni uspelo, vrnitev k rgba/rgbx.");
    registerEntry("sl_SI", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Zaslon {name}: širok barvni razpon je omogočen, vendar zaslon ni v 10-bitnem načinu.");

    // sr_RS (Serbian)
    registerEntry("sr_RS", TXT_KEY_ANR_TITLE, "Апликација не реагује");
    registerEntry("sr_RS", TXT_KEY_ANR_CONTENT, "Апликација {title} - {class} не реагује.\nШта желите да урадите са њом?");
    registerEntry("sr_RS", TXT_KEY_ANR_OPTION_TERMINATE, "Прекини");
    registerEntry("sr_RS", TXT_KEY_ANR_OPTION_WAIT, "Чекај");
    registerEntry("sr_RS", TXT_KEY_ANR_PROP_UNKNOWN, "(непознато)");

    registerEntry("sr_RS", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Апликација <b>{app}</b> захтева непознату дозволу.");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Апликација <b>{app}</b> покушава да снима твој екран.\n\nДа ли желиш да то дозволиш?");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Апликација <b>{app}</b> покушава да учита додатак: <b>{plugin}</b>.\n\nДа ли желиш да то дозволиш?");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Нова тастатура је детектована: <b>{keyboard}</b>.\n\nДа ли желиш да дозволиш њен рад?");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(непознато)");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_TITLE, "Захтев за дозволу");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Савет: можеш направити трајна правила за ово у Hyprland конфигурационој датотеци.");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_ALLOW, "Дозволи");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Дозволи и запамти");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_ALLOW_ONCE, "Дозволи једном");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_DENY, "Одбиј");
    registerEntry("sr_RS", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Непозната апликација (wayland client ID {wayland_id})");

    registerEntry("sr_RS", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Изгледа да се твојим XDG_CURRENT_DESKTOP окружењем управља споља, и тренутна вредност је {value}.\nОво може правити проблеме осим ако је намерно.");
    registerEntry("sr_RS", TXT_KEY_NOTIF_NO_GUIUTILS, "Твој систем нема инсталиран hyprland-guiutils. Ово је зависност при покретању за неке дијалоге. Размотри инсталацију.");
    registerEntry("sr_RS", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland није успео да учита {count} кључни ресурс, криви пакера твоје дистрибуције за лоше одрађен посао!";
        if (assetsNo <= 4)
            return "Hyprland није успео да учита {count} кључна ресурса, криви пакера твоје дистрибуције за лоше одрађен посао!";
        return "Hyprland није успео да учита {count} кључних ресурса, криви пакера твоје дистрибуције за лоше одрађен посао!";
    });
    registerEntry(
        "sr_RS", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Твој распоред монитора је неправилно постављен. Монитор {name} се преклапа са другим монитором/мониторима у распореду.\nМолим те погледај вики (Monitors страницу) за "
        "више информација. Ово <b>ће</b> изазвати проблеме.");
    registerEntry("sr_RS", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Монитор {name} није успео да постави ниједан тражени режим, враћање на режим {mode}.");
    registerEntry("sr_RS", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Невалидна скала прослеђена монитору {name}: {scale}, користи се препоручена скала: {fixed_scale}");
    registerEntry("sr_RS", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Неуспешно учитавање додатка {name}: {error}");
    registerEntry("sr_RS", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Поново учитавање CM шејдера није успело, враћање на rgba/rgbx.");
    registerEntry("sr_RS", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Монитор {name}: широк спектар боја је омогућен али екран није у 10-битном режиму.");

    // sr_RS@latin (Serbian Latin)
    registerEntry("sr_RS@latin", TXT_KEY_ANR_TITLE, "Aplikacija ne reaguje");
    registerEntry("sr_RS@latin", TXT_KEY_ANR_CONTENT, "Aplikacija {title} - {class} ne reaguje.\nŠta želite da uradite sa njom?");
    registerEntry("sr_RS@latin", TXT_KEY_ANR_OPTION_TERMINATE, "Prekini");
    registerEntry("sr_RS@latin", TXT_KEY_ANR_OPTION_WAIT, "Čekaj");
    registerEntry("sr_RS@latin", TXT_KEY_ANR_PROP_UNKNOWN, "(nepoznato)");

    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Aplikacija <b>{app}</b> zahteva nepoznatu dozvolu.");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Aplikacija <b>{app}</b> pokušava da snima tvoj ekran.\n\nDa li želiš da to dozvoliš?");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Aplikacija <b>{app}</b> pokušava da učita dodatak: <b>{plugin}</b>.\n\nDa li želiš da to dozvoliš?");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Nova tastatura je detektovana: <b>{keyboard}</b>.\n\nDa li želiš da dozvoliš njen rad?");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(nepoznato)");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_TITLE, "Zahtev za dozvolu");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Savet: možeš napraviti trajna pravila za ovo u Hyprland konfiguracionoj datoteci.");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_ALLOW, "Dozvoli");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Dozvoli i zapamti");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_ALLOW_ONCE, "Dozvoli jednom");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_DENY, "Odbij");
    registerEntry("sr_RS@latin", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Nepoznata aplikacija (wayland client ID {wayland_id})");

    registerEntry("sr_RS@latin", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Izgleda da se tvojim XDG_CURRENT_DESKTOP okruženjem upravlja spolja, i trenutna vrednost je {value}.\nOvo može praviti probleme osim ako je namerno.");
    registerEntry("sr_RS@latin", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Tvoj sistem nema instaliran hyprland-guiutils. Ovo je zavisnost pri pokretanju za neke dijaloge. Razmotri instalaciju.");
    registerEntry("sr_RS@latin", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprland nije uspeo da učita {count} ključni resurs, krivi pakera tvoje distribucije za loše odrađen posao!";
        if (assetsNo <= 4)
            return "Hyprland nije uspeo da učita {count} ključna resursa, krivi pakera tvoje distribucije za loše odrađen posao!";
        return "Hyprland nije uspeo da učita {count} ključnih resursa, krivi pakera tvoje distribucije za loše odrađen posao!";
    });
    registerEntry(
        "sr_RS@latin", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
        "Tvoj raspored monitora je nepravilno postavljen. Monitor {name} se preklapa sa drugim monitorom/monitorima u rasporedu.\nMolim te pogledaj wiki (Monitors stranicu) za "
        "više informacija. Ovo <b>će</b> izazvati probleme.");
    registerEntry("sr_RS@latin", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitor {name} nije uspeo da postavi nijedan traženi režim, vraćanje na režim {mode}.");
    registerEntry("sr_RS@latin", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Nevalidna skala prosleđena monitoru {name}: {scale}, koristi se preporučena skala: {fixed_scale}");
    registerEntry("sr_RS@latin", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Neuspešno učitavanje dodatka {name}: {error}");
    registerEntry("sr_RS@latin", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Ponovno učitavanje CM šejdera nije uspelo, vraćanje na rgba/rgbx.");
    registerEntry("sr_RS@latin", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: širok spektar boja je omogućen ali ekran nije u 10-bitnom režimu.");

    // tr_TR (Turkish)
    registerEntry("tr_TR", TXT_KEY_ANR_TITLE, "Uygulama Yanıt Vermiyor");
    registerEntry("tr_TR", TXT_KEY_ANR_CONTENT, "Bir uygulama {title} - {class} yanıt vermiyor.\nBununla ne yapmak istiyorsun?");
    registerEntry("tr_TR", TXT_KEY_ANR_OPTION_TERMINATE, "Sonlandır");
    registerEntry("tr_TR", TXT_KEY_ANR_OPTION_WAIT, "Bekle");
    registerEntry("tr_TR", TXT_KEY_ANR_PROP_UNKNOWN, "(bilinmiyor)");

    registerEntry("tr_TR", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Bir uygulama <b>{app}</b> bilinmeyen bir izin istiyor.");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Bir uygulama <b>{app}</b> ekran kaydı yapmaya çalışıyor.\n\nİzin vermek istiyor musun?");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Bir uygulama <b>{app}</b> bir eklenti kurmaya çalışıyor: <b>{plugin}</b>.\n\nİzin vermek istiyor musun?");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Yeni bir klavye algılandı: <b>{keyboard}</b>.\n\nÇalışmasına izin vermek istiyor musun?");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(bilinmiyor)");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_TITLE, "İzin isteği");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "İpucu: Hyprland config dosyasında bunlar için kalıcı kurallar atayabilirsin.");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_ALLOW, "İzin ver");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "İzin ver ve seçimimi hatırla");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_ALLOW_ONCE, "Yalnızca bir defa izin ver");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_DENY, "Reddet");
    registerEntry("tr_TR", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Bilinmeyen uygulama (wayland istemci ID {wayland_id})");

    registerEntry("tr_TR", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "XDG_CURRENT_DESKTOP ortamın harici olarak yönetiliyor gibi gözüküyor, ve mevcut değeri {value}.\nEğer bu bilinçli değilse sorunlara yol açabilir.");
    registerEntry("tr_TR", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Sisteminde hyprland-guiutils yüklü değil. Bu bazı diyaloglar için bir çalışma zamanı bağımlılığı. İndirmeyi göz önünde bulundurabilirsin.");
    registerEntry("tr_TR", TXT_KEY_NOTIF_FAILED_ASSETS,
                  "Hyprland {count} gerekli dosyayı yüklemekte başarısız oldu, kötü bir iş çıkardığı için kullandığın distronun paketleyicisini suçla!");
    registerEntry("tr_TR", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Monitör düzenin yanlış ayarlanmış. Monitör {name} düzenindeki başka monitörlerle çakışıyor.\nLütfen daha fazla bilgi için wiki'ye (Monitörler sayfası) göz at. "
                  "Bu <b>sorunlara yol açacak</b>.");
    registerEntry("tr_TR", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitör {name} istenen modları ayarlamada başarısız oldu, {mode} moduna geri dönülüyor.");
    registerEntry("tr_TR", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Monitöre geçersiz ölçek iletildi {name}: {scale}, önerilen ölçek kullanılıyor: {fixed_scale}");
    registerEntry("tr_TR", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "{name} plugini yüklenemedi: {error}");
    registerEntry("tr_TR", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM shader yeniden yüklemesi başarısız, rgba/rgbx'e geri dönülüyor.");
    registerEntry("tr_TR", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitör {name}: wide color gamut etkinleştirildi ama ekran 10-bit modunda değil.");

    // tt_RU (Tatar)
    registerEntry("tt_RU", TXT_KEY_ANR_TITLE, "Программа җавап бирми");
    registerEntry("tt_RU", TXT_KEY_ANR_CONTENT, "Программасы {title} - {class} җавап бирми.\nСез аның белән нәрсә эшләргә телисез?");
    registerEntry("tt_RU", TXT_KEY_ANR_OPTION_TERMINATE, "Тәмам итү");
    registerEntry("tt_RU", TXT_KEY_ANR_OPTION_WAIT, "Көтү");
    registerEntry("tt_RU", TXT_KEY_ANR_PROP_UNKNOWN, "(билгесез)");

    registerEntry("tt_RU", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "<b>{app}</b> программасы билгесез рөхсәт сорый.");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "<b>{app}</b> программасы сезнең экранны яздырырга тели.\n\nРөхсәт бирәсезме?");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_REQUEST_CURSOR_POS, "<b>{app}</b> программасы курсор позициясен күзәтергә тели.\n\nРөхсәт бирәсезме?");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "<b>{app}</b> программасы плагин йөкләргә тели: <b>{plugin}</b>.\n\nРөхсәт бирәсезме?");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Яңа клавиатура табылды: <b>{keyboard}</b>.\n\nАның эшләргә рөхсәт бирәсезме?");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(билгесез)");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_TITLE, "Рөхсәт сорау");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Киңәш: сез Hyprland көйләү файлында даими кагыйдәләр куя аласыз.");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_ALLOW, "Рөхсәт бирү");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Рөхсәт бирү һәм истә калдыру");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_ALLOW_ONCE, "Бер тапкыр рөхсәт бирү");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_DENY, "Кире кагу");
    registerEntry("tt_RU", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Билгесез программа (wayland client ID {wayland_id})");

    registerEntry("tt_RU", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Сезнең XDG_CURRENT_DESKTOP мохите тыштан идарә ителә, хәзерге кыйммәте: {value}.\n"
                  "Бу теләгән булмаса, проблемалар китереп чыгарырга мөмкин.");
    registerEntry("tt_RU", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "Сезнең системада hyprland-guiutils урнаштырылмаган. Бу кайбер диалоглар өчен кирәкле вакыт бәйлелеге. Урнаштыруны карап чыгыгыз.");
    registerEntry("tt_RU", TXT_KEY_NOTIF_FAILED_ASSETS, "Hyprland {count} мөһим ресурсны йөкли алмады. Ул дистрибутивыгыз пакетлаучысының хатасы!");
    registerEntry("tt_RU", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Сезнең мониторлар урнашуы дөрес түгел. {name} мониторы башка монитор белән өстәлә.\n"
                  "Зинһар, өстәмә мәгълүмат өчен викидагы (Monitors бит) мөрәҗәгать итегез. Бу <b>һичшиксез</b> проблемалар тудырачак.");
    registerEntry("tt_RU", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "{name} мониторы соралган режимнарны куя алмады, {mode} режимына кайта.");
    registerEntry("tt_RU", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "{name} мониторы өчен яраксыз масштаб билгеләнгән: {scale}. Тәкъдим ителгән масштаб кулланыла: {fixed_scale}");
    registerEntry("tt_RU", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "{name} плагинны йөкләүдә хата: {error}");
    registerEntry("tt_RU", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "CM шейдерын яңадан йөкләү уңышсыз булды, rgba/rgbx режимына кайтыла.");
    registerEntry("tt_RU", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Монитор {name}: киң төсләр диапазоны кушылган, ләкин дисплей 10-бит режимында түгел.");
    registerEntry("tt_RU", TXT_KEY_NOTIF_NO_WATCHDOG, "Hyprland start-hyprland ярдәмендә эшләтелмәгән. Бу, төзәтү мохитеннән тыш, бик тәкъдим ителми.");

    registerEntry("tt_RU", TXT_KEY_SAFE_MODE_TITLE, "Куркынычсыз режим");
    registerEntry("tt_RU", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Hyprland куркынычсыз режимда эшләтелде, димәк сезнең соңгы сессия авария белән тәмамланган.\n"
                  "Куркынычсыз режим сезнең конфигурацияне йөкләүне тыя. Сез бу мохиттә проблемаларны тикшерә аласыз, яки түбәндәге төймә аша конфигурацияне йөкли аласыз.\n"
                  "Килешенгән төймә бәйләнешләре кулланыла: SUPER+Q kitty ачу, SUPER+R лаунчер ачу, SUPER+M чыгу.\n"
                  "Hyprland-ны яңадан эшләтү аны гадәти режимда ачачак.");
    registerEntry("tt_RU", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Конфигурацияне йөкләү");
    registerEntry("tt_RU", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Авария хисаплары папкасын ачу");
    registerEntry("tt_RU", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "Аңлашылды, ябу");

    // uk_UA (Ukrainian)
    registerEntry("uk_UA", TXT_KEY_ANR_TITLE, "Програма не відповідає");
    registerEntry("uk_UA", TXT_KEY_ANR_CONTENT, "Програма {title} - {class} не відповідає.\nЩо ви хочете з нею зробити?");
    registerEntry("uk_UA", TXT_KEY_ANR_OPTION_TERMINATE, "Завершити");
    registerEntry("uk_UA", TXT_KEY_ANR_OPTION_WAIT, "Чекати");
    registerEntry("uk_UA", TXT_KEY_ANR_PROP_UNKNOWN, "(невідомо)");

    registerEntry("uk_UA", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Програма <b>{app}</b> запитує невідомий дозвіл.");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Програма <b>{app}</b> намагається захопити екран.\n\nДозволити?");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Програма <b>{app}</b> намагається завантажити плагін: <b>{plugin}</b>.\n\nДозволити?");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Виявлено нову клавіатуру: <b>{keyboard}</b>.\n\nДозволити її використання?");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(невідомо)");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_TITLE, "Запит дозволу");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Підказка: ви можете встановити постійні правила для дозволів у файлі конфігурації Hyprland.");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_ALLOW, "Дозволити");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Дозволити та запам'ятати");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_ALLOW_ONCE, "Дозволити один раз");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_DENY, "Заборонити");
    registerEntry("uk_UA", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Невідома програма (ідентифікатор клієнта wayland {wayland_id})");

    registerEntry(
        "uk_UA", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Ваше середовище XDG_CURRENT_DESKTOP, схоже, керується ззовні, і поточне значення становить {value}.\nЦе може спричинити проблеми, якщо це не зроблено навмисно.");
    registerEntry("uk_UA", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "У вашій системі не встановлено hyprland-guiutils. Це залежність, потрібна для роботи деяких діалогових вікон. Розгляньте можливість його встановлення.");
    registerEntry("uk_UA", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Не вдалося завантажити {count} необхідний ресурс, звинувачуйте пакувальника свого дистрибутива у недобросовісній роботі!";
        return "Не вдалося завантажити {count} необхідних ресурсів, звинувачуйте пакувальника свого дистрибутива у недобросовісній роботі!";
    });
    registerEntry("uk_UA", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Макет моніторів налаштовано неправильно. Монітор {name} перекриває інші монітори у макеті.\nБудь ласка, перегляньте wiki (сторінка Monitors). "
                  "Це <b>обов'язково</b> створить проблеми.");
    registerEntry("uk_UA", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Монітор {name} не зміг встановити жодного із запитуваних режимів, повернення до режиму {mode}.");
    registerEntry("uk_UA", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Неправильний масштаб переданий монітору {name}: {scale}, використання запропонованого масштабу: {fixed_scale}");
    registerEntry("uk_UA", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Помилка завантаження плагіна {name}: {error}");
    registerEntry("uk_UA", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Не вдалося перезавантажити шейдер CM, повернення до rgba/rgbx.");
    registerEntry("uk_UA", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Монітор {name}: широка кольорова гама увімкнена, але дисплей не працює в 10-бітному режимі.");

    // vi_VN (Vietnamese)
    registerEntry("vi_VN", TXT_KEY_ANR_TITLE, "Ứng dụng không phản hồi");
    registerEntry("vi_VN", TXT_KEY_ANR_CONTENT, "Ứng dụng {title} - {class} đang bị treo.\nBạn muốn xử lý thế nào?");
    registerEntry("vi_VN", TXT_KEY_ANR_OPTION_TERMINATE, "Buộc dừng");
    registerEntry("vi_VN", TXT_KEY_ANR_OPTION_WAIT, "Chờ");
    registerEntry("vi_VN", TXT_KEY_ANR_PROP_UNKNOWN, "(không xác định)");

    registerEntry("vi_VN", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Ứng dụng <b>{app}</b> đang yêu cầu một quyền không xác định.");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Ứng dụng <b>{app}</b> đang cố gắng ghi hình màn hình của bạn.\n\nBạn muốn cho phép không?");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_REQUEST_CURSOR_POS, "Ứng dụng <b>{app}</b> đang cố gắng đọc vị trí chuột của bạn.\n\nBạn muốn cho phép không?");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Ứng dụng <b>{app}</b> đang cố gắng tải plugin: <b>{plugin}</b>.\n\nBạn muốn cho phép không?");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Phát hiện bàn phím mới: <b>{keyboard}</b>.\n\nBạn muốn cho phép bàn phím này hoạt động không?");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(không xác định)");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_TITLE, "Yêu cầu cấp quyền");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Gợi ý: bạn có thể thiết lập các quyền này trong tệp cấu hình Hyprland.");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_ALLOW, "Cho phép");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Cho phép và ghi nhớ");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_ALLOW_ONCE, "Chỉ một lần");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_DENY, "Từ chối");
    registerEntry("vi_VN", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Ứng dụng không xác định (wayland client ID {wayland_id})");

    registerEntry(
        "vi_VN", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
        "Biến môi trường XDG_CURRENT_DESKTOP dường như đang được thiết lập từ bên ngoài với giá trị là {value}.\nViệc này có thể gây ra lỗi trừ khi đó là chủ ý của bạn.");
    registerEntry("vi_VN", TXT_KEY_NOTIF_NO_GUIUTILS, "Hệ thống chưa cài hyprland-guiutils. Một số hộp thoại sẽ không hiển thị nếu thiếu nó.");
    registerEntry("vi_VN", TXT_KEY_NOTIF_FAILED_ASSETS,
                  "Hyprland không thể tải {count} tài nguyên quan trọng. Vui lòng báo lỗi cho người đóng gói (packager) của bản phân phối (distro) mà bạn dùng!");
    registerEntry("vi_VN", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Bố cục màn hình không hợp lệ. Màn hình {name} đang bị đè lên các màn hình khác.\nVui lòng xem trang Monitors trên wiki để "
                  "khắc phục, nếu không <b>chắc chắn</b> sẽ có lỗi xảy ra.");
    registerEntry("vi_VN", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Màn hình {name} không thể áp dụng chế độ nào được yêu cầu, đang dùng tạm chế độ {mode}.");
    registerEntry("vi_VN", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Tỉ lệ {scale} cho màn hình {name} không hợp lệ, chuyển sang tỷ lệ gợi ý: {fixed_scale}");
    registerEntry("vi_VN", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Lỗi tải plugin {name}: {error}");
    registerEntry("vi_VN", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Tải lại CM shader thất bại, đang dùng tạm rgba/rgbx.");
    registerEntry("vi_VN", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Màn hình {name}: dải màu rộng (wide color gamut) khả dụng nhưng màn hình không ở chế độ 10-bit.");
    registerEntry("vi_VN", TXT_KEY_NOTIF_NO_WATCHDOG,
                  "Hyprland đã được khởi động mà không thông qua start-hyprland. Việc này không được khuyến khích trừ khi dùng cho mục đích gỡ lỗi.");

    registerEntry("vi_VN", TXT_KEY_SAFE_MODE_TITLE, "Chế độ An toàn (Safe Mode)");
    registerEntry("vi_VN", TXT_KEY_SAFE_MODE_DESCRIPTION,
                  "Phiên hoạt động trước đó đã bị sập (crash).\nHyprland hiện đang chạy ở chế độ an toàn và không tải tệp cấu hình của bạn. Bạn có thể "
                  "khắc phục sự cố trong môi trường này, hoặc bấm nút bên dưới để thử tải lại cấu hình.\nCác phím tắt mặc định: SUPER+Q (kitty), SUPER+R (runner), "
                  "SUPER+M (exit).\nHyprland sẽ về chế độ bình thường sau khi khởi động lại.");
    registerEntry("vi_VN", TXT_KEY_SAFE_MODE_BUTTON_LOAD_CONFIG, "Tải cấu hình");
    registerEntry("vi_VN", TXT_KEY_SAFE_MODE_BUTTON_OPEN_CRASH_REPORT_DIR, "Mở thư mục báo cáo lỗi (crash report)");
    registerEntry("vi_VN", TXT_KEY_SAFE_MODE_BUTTON_UNDERSTOOD, "OK, đã hiểu");

    // cs_CZ (Czech)
    registerEntry("cs_CZ", TXT_KEY_ANR_TITLE, "Aplikace Neodpovídá");
    registerEntry("cs_CZ", TXT_KEY_ANR_CONTENT, "Aplikace {title} - {class} neodpovídá.\nCo s ní chcete udělat?");
    registerEntry("cs_CZ", TXT_KEY_ANR_OPTION_TERMINATE, "Ukončit");
    registerEntry("cs_CZ", TXT_KEY_ANR_OPTION_WAIT, "Počkat");
    registerEntry("cs_CZ", TXT_KEY_ANR_PROP_UNKNOWN, "(neznámé)");

    registerEntry("cs_CZ", TXT_KEY_PERMISSION_REQUEST_UNKNOWN, "Aplikace <b>{app}</b> vyžaduje neznámé oprávnění.");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_REQUEST_SCREENCOPY, "Aplikace <b>{app}</b> se pokouší zaznamenávat vaši obrazovku.\n\nChcete jí to povolit?");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_REQUEST_PLUGIN, "Aplikace <b>{app}</b> se pokouší načíst plugin: <b>{plugin}</b>.\n\nChcete jí to povolit?");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_REQUEST_KEYBOARD, "Byla detekována nová klávesnice: <b>{keyboard}</b>.\n\nChcete jí povolit?");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_UNKNOWN_NAME, "(neznámé)");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_TITLE, "Žádost o oprávnění");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_PERSISTENCE_HINT, "Tip: pro tyto případy můžete nastavit trvalá pravidla v konfiguračním souboru Hyprland.");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_ALLOW, "Povolit");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_ALLOW_AND_REMEMBER, "Povolit a zapamatovat");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_ALLOW_ONCE, "Povolit jednou");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_DENY, "Zamítnout");
    registerEntry("cs_CZ", TXT_KEY_PERMISSION_UNKNOWN_WAYLAND_APP, "Neznámá aplikace (ID klienta Wayland {wayland_id})");

    registerEntry("cs_CZ", TXT_KEY_NOTIF_EXTERNAL_XDG_DESKTOP,
                  "Proměnná prostředí XDG_CURRENT_DESKTOP se zdá být spravována externě a její aktuální hodnota je {value}.\nPokud to není záměr, může to způsobit problémy.");
    registerEntry("cs_CZ", TXT_KEY_NOTIF_NO_GUIUTILS,
                  "V systému není nainstalován balíček hyprland-guiutils. Jedná se o závislost pro běh některých dialogových oken. Zvažte jeho instalaci.");
    registerEntry("cs_CZ", TXT_KEY_NOTIF_FAILED_ASSETS, [](const Hyprutils::I18n::translationVarMap& vars) {
        int assetsNo = std::stoi(vars.at("count"));
        if (assetsNo <= 1)
            return "Hyprlandu se nepodařilo načíst {count} nezbytnou součást. Za špatně odvedenou práci viňte tvůrce balíčku vaší distribuce!";
        if (assetsNo <= 4)
            return "Hyprlandu se nepodařilo načíst {count} nezbytné součásti. Za špatně odvedenou práci viňte tvůrce balíčku vaší distribuce!";
        return "Hyprlandu se nepodařilo načíst {count} nezbytných součástí. Za špatně odvedenou práci viňte tvůrce balíčku vaší distribuce!";
    });
    registerEntry("cs_CZ", TXT_KEY_NOTIF_INVALID_MONITOR_LAYOUT,
                  "Rozložení vašich monitorů je nastaveno nesprávně. Monitor {name} se překrývá s ostatními monitory.\nVíce informací naleznete na wiki "
                  "(stránka Monitors). Toto <b>způsobí</b> problémy.");
    registerEntry("cs_CZ", TXT_KEY_NOTIF_MONITOR_MODE_FAIL, "Monitoru {name} se nepodařilo nastavit žádný z požadovaných režimů, vrací se k režimu {mode}.");
    registerEntry("cs_CZ", TXT_KEY_NOTIF_MONITOR_AUTO_SCALE, "Monitoru {name} bylo předáno neplatné měřítko: {scale}, použije se navrhované měřítko: {fixed_scale}");
    registerEntry("cs_CZ", TXT_KEY_NOTIF_FAILED_TO_LOAD_PLUGIN, "Nepodařilo se načíst plugin {name}: {error}");
    registerEntry("cs_CZ", TXT_KEY_NOTIF_CM_RELOAD_FAILED, "Nepodařilo se znovu načíst CM shader, vrací se k rgba/rgbx.");
    registerEntry("cs_CZ", TXT_KEY_NOTIF_WIDE_COLOR_NOT_10B, "Monitor {name}: široký barevný gamut je povolen, ale displej není v 10bitovém režimu.");
}

std::string I18n::CI18nEngine::localize(eI18nKeys key, const Hyprutils::I18n::translationVarMap& vars) {
    static auto CONFIG_LOCALE = CConfigValue<std::string>("general:locale");
    std::string locale        = *CONFIG_LOCALE != "" ? *CONFIG_LOCALE : localeStr;
    return huEngine->localizeEntry(locale, key, vars);
}
