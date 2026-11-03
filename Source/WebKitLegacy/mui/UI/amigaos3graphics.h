#ifndef ODYSSEY_AMIGAOS3_GRAPHICS_H
#define ODYSSEY_AMIGAOS3_GRAPHICS_H

#include <exec/types.h>
struct ColorMap;
struct RastPort;
struct AmigaOS3Palette;

#ifdef __cplusplus
extern "C" {
#endif

// ARGB bytes, opaque destination. Global opacity uses the high byte, as in
// the existing WritePixelArrayAlpha API. Cairo ARGB32 is premultiplied;
// built-in PNG arrays and the legacy plugin interface use straight alpha.
void AmigaOS3DrawSeparator(struct RastPort*, LONG x, LONG y, ULONG height,
    ULONG lightPenIndex, ULONG darkPenIndex);
void AmigaOS3BlendARGBRow(const UBYTE* source, UBYTE* destination,
    ULONG pixels, UBYTE opacity, BOOL premultiplied);
ULONG AmigaOS3WritePixelArrayAlpha(APTR source, UWORD sourceX, UWORD sourceY,
    UWORD sourceStride, struct RastPort*, UWORD destinationX, UWORD destinationY,
    UWORD width, UWORD height, ULONG globalAlpha);
ULONG AmigaOS3WritePixelArrayPremultipliedAlpha(APTR source, UWORD sourceX,
    UWORD sourceY, UWORD sourceStride, struct RastPort*, UWORD destinationX,
    UWORD destinationY, UWORD width, UWORD height, ULONG globalAlpha);

// CyberGraphX cannot reduce ARGB to the pens of a palette or planar bitmap, so
// content for one is dithered to a color cube of pens obtained from its screen.
struct AmigaOS3Palette* AmigaOS3CreatePalette(struct ColorMap*, ULONG depth);
void AmigaOS3DestroyPalette(struct AmigaOS3Palette*);
void AmigaOS3DitherARGB(const struct AmigaOS3Palette*, const UBYTE* source,
    ULONG sourceStride, UBYTE* pens, UWORD width, UWORD height, UWORD originX,
    UWORD originY);
// Source and destination share coordinates, as the view's surface and its
// offscreen bitmap do. Returns the pixels written, or zero without memory.
ULONG AmigaOS3WritePaletteARGB(const struct AmigaOS3Palette*, const UBYTE* source,
    ULONG sourceStride, struct RastPort*, UWORD x, UWORD y, UWORD width, UWORD height);

#ifdef __cplusplus
}
#endif
#endif
