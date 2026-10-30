#ifndef ODYSSEY_AMIGAOS3_GRAPHICS_H
#define ODYSSEY_AMIGAOS3_GRAPHICS_H

#include <exec/types.h>
struct RastPort;

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

#ifdef __cplusplus
}
#endif
#endif
