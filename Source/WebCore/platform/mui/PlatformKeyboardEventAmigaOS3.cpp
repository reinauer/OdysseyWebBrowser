/* Classic keymap.library event translation. See Tools/AmigaOS3/KEYBOARD.md. */
#include "config.h"
#include "PlatformKeyboardEvent.h"
#include "KeyboardCodes.h"
#include "LocalEncodingAmigaOS3.h"
#include <wtf/HexNumber.h>
#include <wtf/text/StringConcatenate.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <proto/exec.h>
#include <proto/keymap.h>
#include <libraries/keymap.h>
#include <intuition/intuition.h>
#include <unicode/uchar.h>
#include <cstdlib>
#include <cstring>

namespace WebCore {

static OptionSet<PlatformEvent::Modifier> lastModifiers;

static void clearDeadContext(InputEvent& input)
{
    input.ie_position = { };
    // MapRawKey ignores communication codes. Raw code zero is a real
    // key and could itself be dead in a national/custom keymap.
    input.ie_Prev1DownCode = IECODE_COMM_CODE_FIRST;
    input.ie_Prev2DownCode = IECODE_COMM_CODE_FIRST;
}

struct SpecialKey {
    unsigned raw;
    int windows;
    const char* key;
    const char* code;
    const char* identifier;
    const char* text;
};

static const SpecialKey specialKeys[] = {
    { RAWKEY_TAB, VK_TAB, "Tab", "Tab", "U+0009", "\t" },
    { RAWKEY_RETURN, VK_RETURN, "Enter", "Enter", "Enter", "\r" },
    { RAWKEY_ENTER, VK_RETURN, "Enter", "NumpadEnter", "Enter", "\r" },
    { RAWKEY_BACKSPACE, VK_BACK, "Backspace", "Backspace", "U+0008", "\b" },
    { RAWKEY_ESC, VK_ESCAPE, "Escape", "Escape", "U+001B", "\x1b" },
    { RAWKEY_DEL, VK_DELETE, "Delete", "Delete", "U+007F", "\x7f" },
    { RAWKEY_INSERT, VK_INSERT, "Insert", "Insert", "Insert", "" },
    { RAWKEY_PAGEUP, VK_PRIOR, "PageUp", "PageUp", "PageUp", "" },
    { RAWKEY_PAGEDOWN, VK_NEXT, "PageDown", "PageDown", "PageDown", "" },
    { RAWKEY_HOME, VK_HOME, "Home", "Home", "Home", "" },
    { RAWKEY_END, VK_END, "End", "End", "End", "" },
    { RAWKEY_CRSRUP, VK_UP, "ArrowUp", "ArrowUp", "Up", "" },
    { RAWKEY_CRSRDOWN, VK_DOWN, "ArrowDown", "ArrowDown", "Down", "" },
    { RAWKEY_CRSRLEFT, VK_LEFT, "ArrowLeft", "ArrowLeft", "Left", "" },
    { RAWKEY_CRSRRIGHT, VK_RIGHT, "ArrowRight", "ArrowRight", "Right", "" },
    { RAWKEY_LSHIFT, VK_SHIFT, "Shift", "ShiftLeft", "Shift", "" },
    { RAWKEY_RSHIFT, VK_SHIFT, "Shift", "ShiftRight", "Shift", "" },
    { RAWKEY_LCTRL, VK_CONTROL, "Control", "ControlLeft", "Control", "" },
    { RAWKEY_LALT, VK_MENU, "Alt", "AltLeft", "Alt", "" },
    { RAWKEY_RALT, VK_MENU, "Alt", "AltRight", "Alt", "" },
    { RAWKEY_LCOMMAND, VK_LWIN, "Meta", "MetaLeft", "Meta", "" },
    { RAWKEY_RCOMMAND, VK_RWIN, "Meta", "MetaRight", "Meta", "" },
    { RAWKEY_CAPSLOCK, VK_CAPITAL, "CapsLock", "CapsLock", "CapsLock", "" },
    { RAWKEY_HELP, VK_HELP, "Help", "Help", "Help", "" },
    { RAWKEY_PRINTSCR, VK_PRINT, "PrintScreen", "PrintScreen", "PrintScreen", "" },
    { RAWKEY_BREAK, VK_PAUSE, "Pause", "Pause", "Pause", "" },
};

static String mappedText(struct Library* KeymapBase, const InputEvent& input)
{
    char bytes[80];
    auto length = MapRawKey(&input, bytes, sizeof(bytes), nullptr);
    if (length <= 0)
        return { };
    size_t utf8Length;
    char* utf8 = AmigaOS3::localToUTF8(bytes, length, &utf8Length);
    if (!utf8)
        return { };
    String result = String::fromUTF8(utf8, utf8Length);
    std::free(utf8);
    return result;
}

static int virtualKeyForCharacter(UChar character)
{
    if (character >= 'a' && character <= 'z')
        return character - 'a' + VK_A;
    if ((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9'))
        return character;
    switch (character) {
    case ' ': return VK_SPACE;
    case ';': return VK_OEM_1;
    case '=': return VK_OEM_PLUS;
    case ',': return VK_OEM_COMMA;
    case '-': return VK_OEM_MINUS;
    case '.': return VK_OEM_PERIOD;
    case '/': return VK_OEM_2;
    case '`': return VK_OEM_3;
    case '[': return VK_OEM_4;
    case '\\': return VK_OEM_5;
    case ']': return VK_OEM_6;
    case '\'': return VK_OEM_7;
    default: return 0;
    }
}

static String physicalCode(unsigned raw)
{
    // Physical positions, independent of the active national keymap.
    // Values match zune68's gcc/devices/rawkeycodes.h classic layout.
    static constexpr struct { unsigned raw; const char* name; } positions[] = {
        {0x10,"KeyQ"}, {0x11,"KeyW"}, {0x12,"KeyE"}, {0x13,"KeyR"},
        {0x14,"KeyT"}, {0x15,"KeyY"}, {0x16,"KeyU"}, {0x17,"KeyI"},
        {0x18,"KeyO"}, {0x19,"KeyP"}, {0x20,"KeyA"}, {0x21,"KeyS"},
        {0x22,"KeyD"}, {0x23,"KeyF"}, {0x24,"KeyG"}, {0x25,"KeyH"},
        {0x26,"KeyJ"}, {0x27,"KeyK"}, {0x28,"KeyL"}, {0x31,"KeyZ"},
        {0x32,"KeyX"}, {0x33,"KeyC"}, {0x34,"KeyV"}, {0x35,"KeyB"},
        {0x36,"KeyN"}, {0x37,"KeyM"}, {0x00,"Backquote"}, {0x0a,"Digit0"},
        {0x0b,"Minus"}, {0x0c,"Equal"}, {0x0d,"Backslash"},
        {0x1a,"BracketLeft"}, {0x1b,"BracketRight"}, {0x29,"Semicolon"},
        {0x2a,"Quote"}, {0x30,"IntlBackslash"}, {0x38,"Comma"},
        {0x39,"Period"}, {0x3a,"Slash"}, {RAWKEY_SPACE,"Space"},
        {0x0f,"Numpad0"}, {0x1d,"Numpad1"}, {0x1e,"Numpad2"},
        {0x1f,"Numpad3"}, {0x2d,"Numpad4"}, {0x2e,"Numpad5"},
        {0x2f,"Numpad6"}, {0x3d,"Numpad7"}, {0x3e,"Numpad8"},
        {0x3f,"Numpad9"}, {0x3c,"NumpadDecimal"}, {0x4a,"NumpadSubtract"},
        {0x5c,"NumpadDivide"}, {0x5d,"NumpadMultiply"}, {0x5e,"NumpadAdd"},
    };
    if (raw >= 1 && raw <= 9)
        return makeString("Digit", raw);
    for (const auto& position : positions) {
        if (position.raw == raw)
            return position.name;
    }
    return "Unidentified";
}

PlatformKeyboardEvent::PlatformKeyboardEvent(BalEventKey* event)
    : m_balEventKey(event)
{
    if (!event || event->Class != IDCMP_RAWKEY)
        return;
    unsigned raw = event->Code & ~IECODE_UP_PREFIX;
    bool keyUp = event->Code & IECODE_UP_PREFIX;
    m_type = keyUp ? KeyUp : KeyDown;
    m_timestamp = WallTime::now();
    m_autoRepeat = !keyUp && (event->Qualifier & IEQUALIFIER_REPEAT);
    m_isKeypad = (event->Qualifier & IEQUALIFIER_NUMERICPAD) || raw == RAWKEY_ENTER;
    if (event->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) m_modifiers.add(Modifier::ShiftKey);
    if (event->Qualifier & IEQUALIFIER_CAPSLOCK) m_modifiers.add(Modifier::CapsLockKey);
    if (event->Qualifier & IEQUALIFIER_CONTROL) m_modifiers.add(Modifier::ControlKey);
    if (event->Qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) m_modifiers.add(Modifier::AltKey);
    if (event->Qualifier & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) m_modifiers.add(Modifier::MetaKey);
    lastModifiers = m_modifiers;

    for (const auto& special : specialKeys) {
        if (special.raw != raw)
            continue;
        m_windowsVirtualKeyCode = special.windows;
        m_key = special.key;
        m_code = special.code;
        m_keyIdentifier = special.identifier;
        if (!keyUp)
            m_text = m_unmodifiedText = special.text;
        return;
    }
    unsigned function = raw >= RAWKEY_F1 && raw <= RAWKEY_F10 ? raw - RAWKEY_F1 + 1
        : raw == RAWKEY_F11 ? 11 : raw == RAWKEY_F12 ? 12 : 0;
    if (function) {
        m_windowsVirtualKeyCode = VK_F1 + function - 1;
        m_key = m_code = m_keyIdentifier = makeString("F", function);
        return;
    }

    m_code = physicalCode(raw);
    m_key = "Unidentified";
    m_keyIdentifier = "U+0000";
    struct Library* KeymapBase = OpenLibrary("keymap.library", 36);
    if (!KeymapBase)
        return;
    InputEvent input { };
    input.ie_Class = IECLASS_RAWKEY;
    input.ie_Code = raw;
    input.ie_Qualifier = event->Qualifier;
    clearDeadContext(input);
    // IAddress points to four bytes of previous down codes/qualifiers,
    // not an InputEvent pointer. Never retain it past this event dispatch.
    if (event->IAddress)
        std::memcpy(&input.ie_position, event->IAddress, sizeof(input.ie_position));
    String text = mappedText(KeymapBase, input);
    input.ie_Qualifier &= ~(IEQUALIFIER_CONTROL | IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND);
    String key = mappedText(KeymapBase, input);
    input.ie_Qualifier &= ~(IEQUALIFIER_LALT | IEQUALIFIER_RALT);
    String unmodified = mappedText(KeymapBase, input);
    input.ie_Qualifier &= IEQUALIFIER_NUMERICPAD;
    clearDeadContext(input); // A prior dead key must not change accelerator identity.
    String base = mappedText(KeymapBase, input);
    CloseLibrary(KeymapBase);
    if (!base.isEmpty())
        m_windowsVirtualKeyCode = virtualKeyForCharacter(base[0]);
    if (m_isKeypad) {
        if (m_code.length() == 7 && m_code.startsWith("Numpad") && m_code[6] >= '0' && m_code[6] <= '9')
            m_windowsVirtualKeyCode = VK_NUMPAD0 + m_code[6] - '0';
        else if (m_code == "NumpadDecimal") m_windowsVirtualKeyCode = VK_DECIMAL;
        else if (m_code == "NumpadAdd") m_windowsVirtualKeyCode = VK_ADD;
        else if (m_code == "NumpadSubtract") m_windowsVirtualKeyCode = VK_SUBTRACT;
        else if (m_code == "NumpadMultiply") m_windowsVirtualKeyCode = VK_MULTIPLY;
        else if (m_code == "NumpadDivide") m_windowsVirtualKeyCode = VK_DIVIDE;
    }
    if (!key.isEmpty()) {
        m_key = key;
        m_keyIdentifier = makeString("U+", hex(u_toupper(key.characterStartingAt(0)), 4));
    } else if (!base.isEmpty())
        m_key = "Dead";
    if (!keyUp) {
        m_text = text;
        m_unmodifiedText = unmodified;
    }
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool backwardCompatibilityMode)
{
    ASSERT(m_type == KeyDown);
    m_type = type;
    if (backwardCompatibilityMode)
        return;
    if (type == RawKeyDown)
        m_text = m_unmodifiedText = String();
    else {
        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

bool PlatformKeyboardEvent::currentCapsLockState() { return lastModifiers.contains(Modifier::CapsLockKey); }
BalEventKey* PlatformKeyboardEvent::balEventKey() const { return m_balEventKey; }
OptionSet<PlatformEvent::Modifier> PlatformKeyboardEvent::currentStateOfModifierKeys() { return lastModifiers; }
void PlatformKeyboardEvent::getCurrentModifierState(bool& shift, bool& control, bool& alt, bool& meta)
{
    shift = lastModifiers.contains(Modifier::ShiftKey);
    control = lastModifiers.contains(Modifier::ControlKey);
    alt = lastModifiers.contains(Modifier::AltKey);
    meta = lastModifiers.contains(Modifier::MetaKey);
}

}
