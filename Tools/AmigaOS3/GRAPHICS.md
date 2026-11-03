# Classic RTG alpha compositing

Classic CyberGraphX provides ReadPixelArray and WritePixelArray, but does
not provide Odyssey's MorphOS WritePixelArrayAlpha entry point. The OS3
adapter in `Source/WebKitLegacy/mui/UI/amigaos3graphics.cpp` reads a row
from the destination RastPort, blends it in ARGB byte order, and writes it
back through CyberGraphX. Both operations use the original RastPort so
layer clipping is preserved. The application owns CyberGfxBase for the
whole UI lifetime.

There are two explicit source formats:

- Favicon/icon surfaces and PNG-backed animations use Cairo's premultiplied
  ARGB32. Their RGB channels already include pixel alpha and must not be
  multiplied by it again. The Cairo PNG loader also fills the unused high
  byte of opaque RGB24 images with 255.
- The built-in Throbber/TabThrobber arrays contain straight ARGB, including
  partially transparent values such as `0x1aaaaaaa`. The legacy plugin
  interface retains the straight-alpha WritePixelArrayAlpha path.

On big-endian m68k, each pixel is the byte sequence A, R, G, B. The row
routine uses byte accesses, including for unaligned sources. Global
opacity comes from the high byte of the existing ULONG argument and
multiplies both alpha and color; it is not subtracted from pixel alpha.
The destination is an opaque raster. Fully transparent pixels preserve
the destination, and global opacity zero does not touch the RastPort.

The toolbar spacer uses native MUI shine/shadow pens for its recessed
separator instead of MorphOS-only ProcessPixelArray brightness operations.
Its helper draws two one-pixel RectFill columns through graphics.library,
using a local RastPort so the caller's pen, drawing mode and pattern remain
unchanged. This follows the selected MUI theme and also works on classic
planar RastPorts.

The adapter validates 16-bit source-stride and coordinate limits and
rejects wrapping source addresses. It allocates one row buffer per call;
allocation failure returns zero without drawing. The buffer is initialized
before each read because clipping can leave parts of a row untouched.

## Palette and planar screens

CyberGraphX does not reduce ARGB to pens, so WritePixelArray leaves a bitmap
of 8 bits or fewer unchanged and a page stayed blank there with no error. A
default Workbench is often 8-bit. On those screens each browser view obtains
a color cube of pens from the screen's ColorMap when it is shown and releases
them when it is hidden: 216 pens at 8 bits, 125, 64 and 27 at 7, 6 and 5 bits,
and 8 at 4 bits or fewer. Where the screen has no free pen, ObtainBestPen
returns the closest shared one. The view's offscreen bitmap takes the screen's
depth, and its content is dithered with a 4x4 ordered matrix and written with
WriteChunkyPixels, which handles RTG and planar bitmaps. Depths of 15 bits and
more keep the WritePixelArray path.

The alpha adapter draws nothing at these depths: pens cannot be read back as
ARGB to blend with. Favicons and the activity animations are therefore missing
there.

## Native verification

The isolated `GraphicsTest.cpp` probe uses the production adapter. Its row
checks cover both alpha conventions, partial global opacity, exhaustive
alpha/opacity combinations, opaque-source equivalence, transparent pixels,
odd addresses and guard bytes. It also checks the native `_Msg_` alias,
stricmp mapping and real MinList empty sentinel helper.

The RTG part opens a 24-bit CyberGraphX mode, allocates a displayable friend
bitmap, and verifies actual ReadPixelArray/WritePixelArray results. It
covers source offsets, padded rows, two-row blits, invalid strides,
coordinate/address overflow rejection, transparent global opacity and a
real layers.library clip region. The test reads back every pixel of the
bitmap to check both affected and untouched regions. It also reads back
the theme-pen separator and verifies that APen and drawing mode are unchanged.

Build the standalone `odyssey-graphics-test` target, then run it in a
copied AmigaOS fixture with Picasso96/CyberGraphX and its UAE monitor:

```sh
python3 Tools/AmigaOS3/run-platform-test.py \
  --system build/amigaos3-amissl-fixture \
  --rom /path/to/kicka1200.rom \
  --emulator /path/to/winuae \
  --binary build/amigaos3-platform-runtime-final/odyssey-graphics-test \
  --rtg-memory 16 --gdb-port 2348 \
  --output build/amigaos3-graphics-check \
  --success-marker 'RTG pixel-array compositing and layer clipping: 0 failures'
```

`--rtg-memory` enables the emulated Zorro III graphics card and runs the
copied fixture's LoadMonDrvs. It does not modify the source installation.
The final direct probe build uses `-m68020 -O3` and the shared pthread C++
runtime, malloc lock and four compiler-helper objects. Results and input
hashes are saved in `build/amigaos3-graphics-2/`. This is native guest RTG
pixel/readback evidence, not a full-browser rendering or physical graphics
card result. The actual favicon, icon and both animation translation units
also compile with the production browser flags.

The palette path is checked on an 8-bit RTG screen opened with shared pens.
Cube colors must keep their exact pens at every dither threshold, mixed colors
must keep their average, and writes to RTG and planar bitmaps must read back
exactly and leave the rest of the bitmap untouched. All pens must be released.

The integrated CMake target also passed the same 132,379 native checks
with the October 29 SDK in `build/amigaos3-graphics-cmake/output.txt`.

The separator extension passed the native RTG probe in
`build/amigaos3-graphics-spacer/output.txt`: 132,895 checks, zero failures,
RETURN=0. The actual spacer translation unit also compiles with production
flags. The full browser is still the integration test for final layout and
appearance.
