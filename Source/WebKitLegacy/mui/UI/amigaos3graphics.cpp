#include "amigaos3graphics.h"

#include <cybergraphx/cybergraphics.h>
#include <graphics/rastport.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

void AmigaOS3BlendARGBRow(const UBYTE* source, UBYTE* destination,
    ULONG pixels, UBYTE opacity, BOOL premultiplied)
{
    for (ULONG x = 0; x < pixels; ++x, source += 4, destination += 4) {
        ULONG alpha = (static_cast<ULONG>(source[0]) * opacity + 127) / 255;
        if (!alpha)
            continue;
        // Do not multiply Cairo's channels by their pixel alpha a second
        // time. Both paths apply the global opacity to alpha and color.
        ULONG sourceFactor = premultiplied ? opacity : alpha;
        for (unsigned channel = 1; channel < 4; ++channel) {
            ULONG value = (source[channel] * sourceFactor
                + destination[channel] * (255 - alpha) + 127) / 255;
            destination[channel] = value > 255 ? 255 : value;
        }
        destination[0] = 255;
    }
}

static ULONG writeAlpha(APTR source, UWORD sourceX, UWORD sourceY,
    UWORD sourceStride, RastPort* destination, UWORD destinationX,
    UWORD destinationY, UWORD width, UWORD height, ULONG globalAlpha,
    BOOL premultiplied)
{
    UBYTE opacity = globalAlpha >> 24;
    if (!CyberGfxBase || !source || !destination || !destination->BitMap || !width || !height || !opacity)
        return 0;
    ULONG rowBytes = static_cast<ULONG>(width) * 4;
    // CGX stride/coordinates are 16-bit. Reject wrapping source rows or
    // destination coordinates rather than silently addressing another row.
    if ((static_cast<ULONG>(sourceX) + width) * 4 > sourceStride
        || static_cast<ULONG>(sourceY) + height > 65536
        || static_cast<ULONG>(destinationX) + width > 65536
        || static_cast<ULONG>(destinationY) + height > 65536)
        return 0;
    ULONG sourceEnd = (static_cast<ULONG>(sourceY) + height - 1) * sourceStride
        + (static_cast<ULONG>(sourceX) + width) * 4;
    if (reinterpret_cast<uintptr_t>(source) > UINTPTR_MAX - sourceEnd)
        return 0;
    auto* row = static_cast<UBYTE*>(malloc(rowBytes));
    if (!row)
        return 0;
    auto* pixels = static_cast<const UBYTE*>(source)
        + static_cast<ULONG>(sourceY) * sourceStride + static_cast<ULONG>(sourceX) * 4;
    ULONG written = 0;
    for (ULONG y = 0; y < height; ++y) {
        // Layer clipping may leave some positions untouched by the read.
        // The matching write uses the same RastPort and clipping region.
        memset(row, 0, rowBytes);
        if (ReadPixelArray(row, 0, 0, rowBytes, destination, destinationX,
                destinationY + y, width, 1, RECTFMT_ARGB)) {
            AmigaOS3BlendARGBRow(pixels + y * sourceStride, row, width, opacity, premultiplied);
            written += WritePixelArray(row, 0, 0, rowBytes, destination,
                destinationX, destinationY + y, width, 1, RECTFMT_ARGB);
        }
    }
    free(row);
    return written;
}

ULONG AmigaOS3WritePixelArrayAlpha(APTR source, UWORD sourceX, UWORD sourceY,
    UWORD sourceStride, RastPort* destination, UWORD destinationX,
    UWORD destinationY, UWORD width, UWORD height, ULONG globalAlpha)
{
    return writeAlpha(source, sourceX, sourceY, sourceStride, destination,
        destinationX, destinationY, width, height, globalAlpha, FALSE);
}

ULONG AmigaOS3WritePixelArrayPremultipliedAlpha(APTR source, UWORD sourceX,
    UWORD sourceY, UWORD sourceStride, RastPort* destination, UWORD destinationX,
    UWORD destinationY, UWORD width, UWORD height, ULONG globalAlpha)
{
    return writeAlpha(source, sourceX, sourceY, sourceStride, destination,
        destinationX, destinationY, width, height, globalAlpha, TRUE);
}

void AmigaOS3DrawSeparator(RastPort* destination, LONG x, LONG y, ULONG height,
    ULONG lightPenIndex, ULONG darkPenIndex)
{
    if (!destination || !destination->BitMap || !height || x <= 0 || x > 32767
        || y < 0 || y > 32767 || height > 32768UL - y)
        return;
    // Match a recessed MUI separator: shadow on the left, shine on the
    // right. A local RastPort preserves the caller's pen/pattern/cursor.
    RastPort raster = *destination;
    raster.AreaPtrn = nullptr;
    raster.AreaPtSz = 0;
    SetDrMd(&raster, JAM1);
    SetAPen(&raster, lightPenIndex);
    RectFill(&raster, x, y, x, y + height - 1);
    SetAPen(&raster, darkPenIndex);
    RectFill(&raster, x - 1, y, x - 1, y + height - 1);
}
