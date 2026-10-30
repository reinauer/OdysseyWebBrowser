#include "config.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "OdysseyRawKeyCodes.h"
#include "LocalEncodingAmigaOS3.h"
#include "KeyboardCodes.h"
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/keymap.h>
#include <libraries/keymap.h>
#include <intuition/intuition.h>
#include <wtf/text/CString.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using WebCore::PlatformKeyboardEvent;
using WebCore::PlatformEvent;
static unsigned failures;
static void check(bool pass, const char* name)
{
    std::printf("%s %s\n", pass ? "PASS" : "FAIL", name);
    if (!pass) ++failures;
}

static IntuiMessage message(unsigned code, unsigned qualifiers = 0, ULONG* previous = nullptr)
{
    IntuiMessage result { };
    result.Class = IDCMP_RAWKEY;
    result.Code = code;
    result.Qualifier = qualifiers;
    result.IAddress = previous;
    return result;
}

int main()
{
    struct Library* KeymapBase = OpenLibrary("keymap.library", 36);
    if (!KeymapBase) return 20;
    char encoding[80];
    check(WebCore::AmigaOS3::localEncodingName(encoding, sizeof(encoding)), "system charset query");
    std::printf("System charset: %s\n", encoding);
    SetVar("ODYSSEY_LOCAL_CHARSET", "ISO-8859-1", -1, 0);
    char pairs[8];
    LONG count = MapANSI("a", 1, pairs, sizeof(pairs), nullptr);
    check(count == 1, "native MapANSI finds letter key");
    if (count != 1) return 20;
    unsigned letter = static_cast<unsigned char>(pairs[0]);

    auto event = message(letter);
    PlatformKeyboardEvent plain(&event);
    check(plain.text() == "a" && plain.key() == "a" && plain.windowsVirtualKeyCode() == WebCore::VK_A,
        "plain key text and accelerator");
    event = message(letter, IEQUALIFIER_LSHIFT | IEQUALIFIER_REPEAT);
    PlatformKeyboardEvent shifted(&event);
    check(shifted.text() == "A" && shifted.unmodifiedText() == "A" && shifted.shiftKey() && shifted.isAutoRepeat(),
        "shift and repeat qualifiers");
    event = message(letter | IECODE_UP_PREFIX, IEQUALIFIER_LSHIFT | IEQUALIFIER_REPEAT);
    PlatformKeyboardEvent released(&event);
    check(released.type() == PlatformEvent::KeyUp && released.text().isEmpty() && released.unmodifiedText().isEmpty()
        && released.key() == "A" && !released.isAutoRepeat(), "key release has identity without text");
    event = message(letter, IEQUALIFIER_CONTROL);
    PlatformKeyboardEvent control(&event);
    check(control.controlKey() && control.text().length() == 1 && control.text()[0] == 1
        && control.unmodifiedText() == "a" && control.key() == "a", "control text and unmodified shortcut text");
    event = message(letter, IEQUALIFIER_RALT);
    PlatformKeyboardEvent alt(&event);
    check(alt.altKey() && !alt.controlKey() && alt.unmodifiedText() == "a", "right Alt does not invent Control");
    event = message(letter, IEQUALIFIER_LCOMMAND | IEQUALIFIER_CAPSLOCK);
    PlatformKeyboardEvent command(&event);
    check(command.metaKey() && command.text() == "A" && PlatformKeyboardEvent::currentCapsLockState(), "Amiga and CapsLock state");
    bool shift, ctrl, altState, meta;
    PlatformKeyboardEvent::getCurrentModifierState(shift, ctrl, altState, meta);
    check(!shift && !ctrl && !altState && meta, "current modifier snapshot");
    event = message(RAWKEY_ENTER, IEQUALIFIER_NUMERICPAD);
    PlatformKeyboardEvent keypad(&event);
    check(keypad.isKeypad() && keypad.text() == "\r" && keypad.code() == "NumpadEnter", "numeric keypad Enter");
    event = message(0x1d, IEQUALIFIER_NUMERICPAD);
    PlatformKeyboardEvent keypadOne(&event);
    check(keypadOne.text() == "1" && keypadOne.code() == "Numpad1"
        && keypadOne.windowsVirtualKeyCode() == WebCore::VK_NUMPAD1, "keypad digit keeps separate accelerator identity");
    event = message(RAWKEY_CRSRLEFT);
    PlatformKeyboardEvent cursor(&event);
    check(cursor.text().isEmpty() && cursor.key() == "ArrowLeft" && cursor.windowsVirtualKeyCode() == WebCore::VK_LEFT,
        "cursor escape sequence is not inserted as text");
    event = message(RAWKEY_F1);
    PlatformKeyboardEvent function(&event);
    check(function.text().isEmpty() && function.key() == "F1", "function key is not terminal text");

    const unsigned wheelCodes[] = { RAWKEY_WHEEL_UP, RAWKEY_WHEEL_DOWN, RAWKEY_WHEEL_LEFT, RAWKEY_WHEEL_RIGHT };
    const int wheelX[] = { 0, 0, 1, -1 }, wheelY[] = { 1, -1, 0, 0 };
    for (unsigned i = 0; i < 4; ++i) {
        event = message(wheelCodes[i]);
        WebCore::PlatformWheelEvent wheel(&event);
        check(wheel.deltaX() == wheelX[i] * 40 && wheel.deltaY() == wheelY[i] * 40
            && wheel.wheelTicksX() == wheelX[i] && wheel.wheelTicksY() == wheelY[i], "native wheel direction and line step");
    }
    event = message(RAWKEY_WHEEL_UP, IEQUALIFIER_LALT);
    WebCore::PlatformWheelEvent horizontal(&event);
    check(horizontal.deltaX() == 40 && horizontal.deltaY() == 0 && horizontal.wheelTicksX() == 1
        && horizontal.wheelTicksY() == 0, "Alt wheel swaps deltas and ticks together");
    event = message(RAWKEY_WHEEL_UP, IEQUALIFIER_CONTROL);
    WebCore::PlatformWheelEvent zoom(&event);
    check(zoom.controlKey() && zoom.deltaY() == 0 && zoom.wheelTicksY() == 1, "Control wheel preserves zoom ticks without scrolling");

    // The ROM US keymap maps e-acute through a dead accent and the e key.
    // Obtain both codes from the real API, not hardcoded synthetic mappings.
    count = MapANSI("\xe9", 1, pairs, sizeof(pairs), nullptr);
    check(count == 2, "native keymap supplies a dead-key sequence");
    if (count == 2) {
        event = message(static_cast<unsigned char>(pairs[0]), static_cast<unsigned char>(pairs[1]));
        PlatformKeyboardEvent dead(&event);
        check(dead.text().isEmpty() && dead.key() == "Dead", "dead accent emits no character");
        dead.disambiguateKeyDownEvent(PlatformEvent::Char);
        check(dead.text().isEmpty(), "dead Char event does not fabricate accelerator text");
        InputEvent context { };
        context.ie_Prev1DownCode = pairs[0];
        context.ie_Prev1DownQual = pairs[1];
        ULONG previous;
        std::memcpy(&previous, &context.ie_position, sizeof(previous));
        event = message(static_cast<unsigned char>(pairs[2]), static_cast<unsigned char>(pairs[3]), &previous);
        PlatformKeyboardEvent composed(&event);
        check(composed.text() == String::fromUTF8("\xc3\xa9") && composed.keyIdentifier() == "U+00C9",
            "Intuition dead-key context composes and converts to Unicode");
        composed.disambiguateKeyDownEvent(PlatformEvent::RawKeyDown);
        check(composed.text().isEmpty() && composed.unmodifiedText().isEmpty(), "RawKeyDown clears text");
    }

    SetVar("ODYSSEY_LOCAL_CHARSET", "ISO-8859-15", -1, 0);
    char* unicode = WebCore::AmigaOS3::localToUTF8("\xa4", 1);
    char* local = WebCore::AmigaOS3::utf8ToLocal("\xe2\x82\xac", 3);
    check(unicode && !std::strcmp(unicode, "\xe2\x82\xac") && local && !std::strcmp(local, "\xa4"),
        "configured local charset converts Euro both directions");
    std::free(unicode); std::free(local);
    size_t length;
    char* embeddedNull = WebCore::AmigaOS3::localToUTF8("a\0b", 3, &length);
    check(embeddedNull && length == 3 && !std::memcmp(embeddedNull, "a\0b", 3), "local conversion preserves explicit embedded NUL length");
    std::free(embeddedNull);
    char* invalid = WebCore::AmigaOS3::utf8ToLocal("\xff", 1);
    check(!invalid, "malformed UTF-8 is rejected");
    std::free(invalid);
    DeleteVar("ODYSSEY_LOCAL_CHARSET", 0);
    CloseLibrary(KeymapBase);
    std::printf("keyboard test: %u failures\n", failures);
    return failures ? 20 : 0;
}
