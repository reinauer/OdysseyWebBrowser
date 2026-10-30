/* Native language preferences, independent of the browser's MUI frontend. */
#include "config.h"
#include "Language.h"

#include <wtf/text/WTFString.h>

#include <libraries/locale.h>
#include <proto/exec.h>
#include <proto/locale.h>
#include <strings.h>

namespace WTF {

static const char* languageTag(const char* name)
{
    if (!name)
        return nullptr;

    // Classic language names are native catalog directory names, not UTF-8.
    // Octal escapes preserve their Latin-1 (and Czech Latin-2) bytes. Accept
    // English aliases as well, since language packs also use those names.
    static const struct {
        const char* name;
        const char* tag;
    } languages[] = {
        { "albanian", "sq" }, { "arabic", "ar" },
        { "belarusian", "be" }, { "bosnian", "bs" },
        { "bulgarian", "bg" }, { "catalan", "ca" },
        { "croatian", "hr" }, { "czech", "cs" },
        { "\350e\271tina", "cs" },
        { "danish", "da" }, { "dansk", "da" },
        { "dutch", "nl" }, { "nederlands", "nl" },
        { "english", "en" }, { "english-british", "en-GB" },
        { "esperanto", "eo" }, { "estonian", "et" },
        { "faroese", "fo" }, { "finnish", "fi" }, { "suomi", "fi" },
        { "french", "fr" }, { "fran\347ais", "fr" },
        { "galician", "gl" }, { "german", "de" }, { "deutsch", "de" },
        { "greek", "el" }, { "hebrew", "he" },
        { "hungarian", "hu" }, { "magyar", "hu" },
        { "icelandic", "is" }, { "irish", "ga" },
        { "italian", "it" }, { "italiano", "it" },
        { "japanese", "ja" }, { "korean", "ko" },
        { "latvian", "lv" }, { "lithuanian", "lt" },
        { "macedonian", "mk" }, { "malay-indonesian", "id" },
        { "malay-malaysian", "ms" }, { "maltese", "mt" },
        { "norwegian", "no" }, { "norsk", "no" },
        { "norwegian-nynorsk", "nn" },
        { "polish", "pl" }, { "polski", "pl" },
        { "portuguese", "pt" }, { "portugu\352s", "pt" },
        { "portuguese-brazil", "pt-BR" },
        { "romanian", "ro" }, { "russian", "ru" },
        { "serbian", "sr" }, { "slovak", "sk" }, { "slovenian", "sl" },
        { "spanish", "es" }, { "espa\361ol", "es" },
        { "swedish", "sv" }, { "svenska", "sv" },
        { "thai", "th" }, { "turkish", "tr" }, { "turkish_int", "tr" },
        { "t\374rk\347e", "tr" }, { "t\374rkiye", "tr" },
        { "ukrainian", "uk" }
    };
    for (const auto& language : languages) {
        if (!strcasecmp(name, language.name))
            return language.tag;
    }
    return nullptr;
}

Vector<String> platformUserPreferredLanguages(ShouldMinimizeLanguages)
{
    Vector<String> languages;
    // Each call owns its library reference and locale snapshot. In particular,
    // the command-line JavaScript shell must not depend on a GUI LocaleBase.
    struct LocaleBase* LocaleBase = reinterpret_cast<struct LocaleBase*>(
        OpenLibrary("locale.library", 38));
    if (LocaleBase) {
        if (auto* locale = OpenLocale(nullptr)) {
            for (const auto* name : locale->loc_PrefLanguages) {
                if (!name)
                    break;
                if (auto* tag = languageTag(name)) {
                    String language(tag);
                    if (!languages.contains(language))
                        languages.append(WTFMove(language));
                }
            }
            if (languages.isEmpty()) {
                if (auto* tag = languageTag(locale->loc_LanguageName))
                    languages.append(String(tag));
            }
            CloseLocale(locale);
        }
        CloseLibrary(reinterpret_cast<struct Library*>(LocaleBase));
    }
    if (languages.isEmpty())
        languages.append("en"_s);
    return languages;
}

} // namespace WTF
