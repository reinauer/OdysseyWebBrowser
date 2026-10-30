# Native keyboard and local text conversion

`PlatformKeyboardEventAmigaOS3.cpp` is selected only for AmigaOS 3.
It uses the active `keymap.library` map for printable text and copies the
four-byte previous-key context from `IntuiMessage.IAddress` before translation.
No pointer to that transient context is retained. `MapRawKey` receives an
80-byte bounded buffer; an overflow or failed conversion emits no text.

The adapter keeps key identity on release while suppressing release text,
handles Shift/Caps/Control/Alt/Amiga qualifiers and repeat, separates keypad
accelerator codes, and prevents terminal escape sequences from arrow/function
keys entering editable text. Right Alt does not fabricate a Control modifier.
Physical DOM codes describe the classic keyboard positions, independently of
national character mappings. Modifier queries return the last dispatched
keyboard event snapshot; they do not poll input.device asynchronously.

`LocalEncodingAmigaOS3.cpp` is shared by the keyboard adapter and the native
`utf8_to_local` / `local_to_utf8` UI helpers, including clipboard text paths.
It queries classic diskfont.library V47 `DFCTRL_CHARSET` / `ObtainCharsetInfo`
metadata, falling back to ISO-8859-1 when unavailable. Users of older systems
or a custom keymap can set `ODYSSEY_LOCAL_CHARSET` to an ICU converter name,
for example `ISO-8859-15`. The process-local environment takes precedence over
the global value. This is a character encoding setting, not a keymap selector.

ICU performs actual conversion in both directions. Malformed source text and
unavailable converters return failure; Unicode not representable in the local
encoding uses that converter's substitution sequence. Results are allocated
with malloc and freed with free. The keyboard requests the explicit UTF-8
length so embedded NUL bytes are preserved.

`OdysseyRawKeyCodes.h` bridges existing UI spellings to the actual classic SDK
cursor/escape/wheel definitions; keypad positions match the classic layout
recorded in zune68. Other ports retain their original rawkeycodes header.
The existing wheel adapter now swaps both delta and tick axes correctly for
Alt-wheel scrolling.

## Build and native validation

After configuring the browser build and dependencies described in BROWSER.md:

```sh
cmake --build build/amigaos3-browser --target amigaos3-keyboard-test -j4
python3 Tools/AmigaOS3/run-platform-test.py \
  --system build/amigaos3-amissl-fixture \
  --rom /path/to/matching-kickstart.rom \
  --emulator /path/to/winuae \
  --binary build/amigaos3-browser/bin/amigaos3-keyboard-test \
  --output build/amigaos3-keyboard-check \
  --success-marker 'keyboard test: 0 failures' --gdb-port 2350
```

Use a fresh output directory and a free GDB port. The test links the production
keyboard, wheel and encoding sources with the real staged WTF/ICU/runtime.
It obtains letter and dead-accent raw sequences through native `MapANSI` and
feeds them through the actual production adapter. Coverage includes key-up,
Shift/repeat, Control, right Alt, Amiga/Caps, keypad Enter/digits, arrows/functions,
dead-key composition via the Intuition context word, charset selection,
ISO-8859-15 Euro roundtrip, explicit embedded NUL length and malformed UTF-8.
Wheel checks cover four directions, pixel steps, Alt-axis swapping and Control
zoom ticks. Exact production WebCore keyboard/encoding/wheel objects and the
UI utils object also compile with their generated browser build commands.

The supplied fixture uses the ROM US keymap. This is isolated native emulator
proof, not a completed browser or end-to-end physical-keyboard/MUI dispatch
test; national/custom layouts and physical hardware remain untested.

## Optional classic UI facilities

Classic Intuition has no equivalent of the MorphOS `SA_StopBlanker` inhibition
tag. The native helper leaves normal OS/commodity blanking behavior intact.
MagicBeacon/Ringhio notification delivery is unavailable on this port and the
helper returns its existing unavailable result (20). MorphOS/OS4 implementations
remain selected on those systems. The MorphOS-only spellchecker reports no
native dictionary/learning capability.

## API references

- [MapRawKey autodoc](https://developer.amigaos3.net/autodocs/keymap.library/MapRawKey.html)
- [Keymap library documentation](https://wiki.amigaos.net/wiki/Keymap_Library)
- [Intuition keyboard documentation](https://wiki.amigaos.net/wiki/Intuition_Keyboard)
- Installed SDK `libraries/keymap.h`, `devices/inputevent.h`, and
  `ndk/lib/sfd/diskfont_lib.sfd` (ObtainCharsetInfo is a classic V47 function).
- zune68 `workbench/libs/muimaster/gcc/devices/rawkeycodes.h` for physical
  keypad positions; its OS-specific extended key definitions are not imported.
