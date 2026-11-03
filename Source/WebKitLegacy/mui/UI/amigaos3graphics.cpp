#include "amigaos3graphics.h"

#include <cybergraphx/cybergraphics.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
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
    // Pens cannot be read back as ARGB to blend with, and a planar write leaves garbage.
    if (GetBitMapAttr(destination->BitMap, BMA_DEPTH) <= 8)
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

static constexpr ULONG maximumPaletteLevels = 6;
static constexpr ULONG maximumPalettePens = maximumPaletteLevels * maximumPaletteLevels * maximumPaletteLevels;
static constexpr ULONG ditherThresholds = 16;

struct AmigaOS3Palette {
    ColorMap* colorMap;
    ULONG penCount;
    LONG obtained[maximumPalettePens];
    UBYTE pens[maximumPalettePens];
    // Cube offsets of a red and green level. Multiplying per pixel is too slow.
    UBYTE redOffset[maximumPaletteLevels];
    UBYTE greenOffset[maximumPaletteLevels];
    // The cube level of a channel value at each dither threshold.
    UBYTE levels[ditherThresholds][256];
};

AmigaOS3Palette* AmigaOS3CreatePalette(ColorMap* colorMap, ULONG depth)
{
    auto* palette = static_cast<AmigaOS3Palette*>(malloc(sizeof(AmigaOS3Palette)));
    if (!palette)
        return nullptr;
    // Leave room for the pens which the screen and MUI already use.
    ULONG levels = depth < 5 ? 2 : depth - 2;
    if (levels > maximumPaletteLevels)
        levels = maximumPaletteLevels;
    TagItem tags[] = { { OBP_Precision, static_cast<ULONG>(PRECISION_EXACT) }, { TAG_END, 0 } };
    palette->colorMap = colorMap;
    palette->penCount = 0;
    for (ULONG red = 0; red < levels; ++red) {
        for (ULONG green = 0; green < levels; ++green) {
            for (ULONG blue = 0; blue < levels; ++blue) {
                ULONG r = red * 255 / (levels - 1) * 0x01010101;
                ULONG g = green * 255 / (levels - 1) * 0x01010101;
                ULONG b = blue * 255 / (levels - 1) * 0x01010101;
                LONG pen = ObtainBestPenA(colorMap, r, g, b, tags);
                palette->obtained[palette->penCount] = pen;
                palette->pens[palette->penCount++] = pen >= 0 ? pen : FindColor(colorMap, r, g, b, -1);
            }
        }
    }
    for (ULONG level = 0; level < levels; ++level) {
        palette->redOffset[level] = level * levels * levels;
        palette->greenOffset[level] = level * levels;
    }
    for (ULONG threshold = 0; threshold < ditherThresholds; ++threshold) {
        for (ULONG value = 0; value < 256; ++value) {
            palette->levels[threshold][value] = (value * (levels - 1) * ditherThresholds
                + threshold * 255 + 127) / (255 * ditherThresholds);
        }
    }
    return palette;
}

void AmigaOS3DestroyPalette(AmigaOS3Palette* palette)
{
    if (!palette)
        return;
    for (ULONG index = 0; index < palette->penCount; ++index) {
        if (palette->obtained[index] >= 0)
            ReleasePen(palette->colorMap, palette->obtained[index]);
    }
    free(palette);
}

void AmigaOS3DitherARGB(const AmigaOS3Palette* palette, const UBYTE* source,
    ULONG sourceStride, UBYTE* pens, UWORD width, UWORD height, UWORD originX,
    UWORD originY)
{
    static const UBYTE bayer[4][4] = {
        { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 },
    };
    for (ULONG y = 0; y < height; ++y) {
        const UBYTE* pixel = source + y * sourceStride;
        const UBYTE* thresholds = bayer[(originY + y) & 3];
        for (ULONG x = 0; x < width; ++x, pixel += 4) {
            const UBYTE* levels = palette->levels[thresholds[(originX + x) & 3]];
            *pens++ = palette->pens[palette->redOffset[levels[pixel[1]]]
                + palette->greenOffset[levels[pixel[2]]] + levels[pixel[3]]];
        }
    }
}

ULONG AmigaOS3WritePaletteARGB(const AmigaOS3Palette* palette, const UBYTE* source,
    ULONG sourceStride, RastPort* destination, UWORD x, UWORD y, UWORD width, UWORD height)
{
    ULONG pixels = static_cast<ULONG>(width) * height;
    if (!pixels)
        return 0;
    // Convert in strips of at most 32 KB rather than one pen buffer as large
    // as the update, which is 480 KB for a full 800x600 view.
    const ULONG stripBytes = 32768;
    UWORD stripRows = width >= stripBytes ? 1 : static_cast<UWORD>(stripBytes / width);
    if (stripRows > height)
        stripRows = height;
    auto* pens = static_cast<UBYTE*>(malloc(static_cast<ULONG>(width) * stripRows));
    if (!pens)
        return 0;
    for (UWORD row = 0; row < height; row += stripRows) {
        UWORD rows = height - row < stripRows ? height - row : stripRows;
        UWORD top = y + row;
        AmigaOS3DitherARGB(palette, source + top * sourceStride + x * 4UL, sourceStride, pens, width, rows, x, top);
        WriteChunkyPixels(destination, x, top, x + width - 1, top + rows - 1, pens, width);
    }
    free(pens);
    return pixels;
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
