#include <cybergraphx/cybergraphics.h>
#include <graphics/gfxbase.h>
#include <graphics/layers.h>
#include <graphics/regions.h>
#include <intuition/intuitionbase.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <stdio.h>
#include <string.h>
#include "amigaos3graphics.h"
#include "amigaos3funcs.h"

struct Library* CyberGfxBase;
struct GfxBase* GfxBase;
struct IntuitionBase* IntuitionBase;
struct Library* LayersBase;

static unsigned failures;
static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) { \
    if (failures < 20) printf("FAIL graphics line %u: %s\n", __LINE__, #value); \
    ++failures; } } while (0)

static void checkRows()
{
    MinList list;
    NewList(reinterpret_cast<List*>(&list));
    CHECK(IsMinListEmpty(&list));
    MinNode node = { };
    AddTail(reinterpret_cast<List*>(&list), reinterpret_cast<Node*>(&node));
    CHECK(!IsMinListEmpty(&list));
    Remove(reinterpret_cast<Node*>(&node));
    CHECK(IsMinListEmpty(&list));
    CHECK(!stricmp("MiXeD", "mixed"));
    _Msg_ message = { 123 };
    Msg messagePointer = &message;
    CHECK(messagePointer->MethodID == 123);
    // Native byte layout, including odd addresses: no ULONG loads allowed.
    UBYTE straight[] = { 99, 128, 255, 0, 0, 77 };
    UBYTE premultiplied[] = { 88, 128, 128, 0, 0, 66 };
    UBYTE blue[] = { 55, 255, 0, 0, 255, 44 };
    UBYTE destination[6];
    memcpy(destination, blue, sizeof(blue));
    AmigaOS3BlendARGBRow(straight + 1, destination + 1, 1, 255, FALSE);
    CHECK(destination[0] == 55 && destination[5] == 44);
    CHECK(destination[1] == 255 && destination[2] == 128
        && destination[3] == 0 && destination[4] == 127);
    memcpy(destination, blue, sizeof(blue));
    AmigaOS3BlendARGBRow(premultiplied + 1, destination + 1, 1, 255, TRUE);
    CHECK(destination[2] == 128 && destination[4] == 127);
    AmigaOS3BlendARGBRow(straight + 1, destination + 1, 0, 128, FALSE);
    CHECK(destination[2] == 128 && destination[4] == 127);
    memcpy(destination, blue, sizeof(blue));
    AmigaOS3BlendARGBRow(premultiplied + 1, destination + 1, 1, 128, TRUE);
    CHECK(destination[2] == 64 && destination[4] == 191);
    memcpy(destination, blue, sizeof(blue));
    AmigaOS3BlendARGBRow(straight + 1, destination + 1, 1, 128, FALSE);
    CHECK(destination[2] == 64 && destination[4] == 191);
    for (unsigned alpha = 0; alpha < 256; ++alpha) {
        for (unsigned opacity = 0; opacity < 256; ++opacity) {
            UBYTE source[] = { static_cast<UBYTE>(alpha), 255, 0, 0 };
            UBYTE result[] = { 255, 0, 0, 255 };
            AmigaOS3BlendARGBRow(source, result, 1, opacity, FALSE);
            unsigned effective = (alpha * opacity + 127) / 255;
            CHECK(result[0] == 255 && result[1] == effective
                && !result[2] && result[3] == 255 - effective);
            // Opaque sources have identical straight/premultiplied output.
            source[0] = 255;
            source[1] = alpha;
            source[2] = 255 - alpha;
            UBYTE first[] = { 255, 29, 143, 217 };
            UBYTE second[] = { 255, 29, 143, 217 };
            AmigaOS3BlendARGBRow(source, first, 1, opacity, FALSE);
            AmigaOS3BlendARGBRow(source, second, 1, opacity, TRUE);
            CHECK(!memcmp(first, second, sizeof(first)));
        }
        UBYTE source[] = { static_cast<UBYTE>(alpha), static_cast<UBYTE>(alpha), 0, 0 };
        UBYTE result[] = { 255, 0, 0, 255 };
        AmigaOS3BlendARGBRow(source, result, 1, 255, TRUE);
        CHECK(result[1] == alpha && result[3] == 255 - alpha);
    }
    printf("ARGB straight/premultiplied row math: %u checks, %u failures\n", checks, failures);
}

static void checkRTG()
{
    GfxBase = reinterpret_cast<struct GfxBase*>(OpenLibrary("graphics.library", 39));
    IntuitionBase = reinterpret_cast<struct IntuitionBase*>(OpenLibrary("intuition.library", 39));
    LayersBase = OpenLibrary("layers.library", 39);
    CyberGfxBase = OpenLibrary("cybergraphics.library", 41);
    CHECK(GfxBase && IntuitionBase && LayersBase && CyberGfxBase);
    if (!GfxBase || !IntuitionBase || !LayersBase || !CyberGfxBase)
        goto closeLibraries;
    {
        ULONG mode = BestCModeIDTags(CYBRBIDTG_Depth, 24,
            CYBRBIDTG_NominalWidth, 640, CYBRBIDTG_NominalHeight, 480, TAG_DONE);
        printf("RTG mode: %08lx\n", mode);
        CHECK(mode != ~0UL && IsCyberModeID(mode));
        if (mode == ~0UL)
            goto closeLibraries;
        auto* screen = OpenScreenTags(nullptr, SA_DisplayID, mode,
            SA_Width, 640, SA_Height, 480, SA_Depth, 24,
            SA_Quiet, TRUE, SA_ShowTitle, FALSE, SA_Type, CUSTOMSCREEN, TAG_DONE);
        CHECK(screen);
        if (!screen)
            goto closeLibraries;
        auto* bitmap = AllocBitMap(32, 16, GetBitMapAttr(screen->RastPort.BitMap, BMA_DEPTH),
            BMF_CLEAR | BMF_DISPLAYABLE, screen->RastPort.BitMap);
        CHECK(bitmap && GetCyberMapAttr(bitmap, CYBRMATTR_ISCYBERGFX));
        if (bitmap) {
            RastPort raster;
            InitRastPort(&raster);
            raster.BitMap = bitmap;
            UBYTE background[32 * 16 * 4];
            UBYTE readback[sizeof(background)];
            for (unsigned i = 0; i < sizeof(background); i += 4) {
                background[i] = 255;
                background[i + 1] = 20;
                background[i + 2] = 60;
                background[i + 3] = 100;
            }
            // Nontrivial source offset, padding, two rows and alpha forms.
            UBYTE source[3 * 48];
            memset(source, 0xcd, sizeof(source));
            for (unsigned y = 1; y < 3; ++y)
                for (unsigned x = 2; x < 6; ++x) {
                    UBYTE* pixel = source + y * 48 + x * 4;
                    pixel[0] = 128;
                    pixel[1] = 200;
                    pixel[2] = 100;
                    pixel[3] = 50;
                }
            CHECK(WritePixelArray(background, 0, 0, 128, &raster, 0, 0, 32, 16, RECTFMT_ARGB));
            CHECK(AmigaOS3WritePixelArrayAlpha(source, 2, 1, 48, &raster, 5, 3, 4, 2, 0xffffffff));
            CHECK(ReadPixelArray(readback, 0, 0, 128, &raster, 0, 0, 32, 16, RECTFMT_ARGB));
            for (unsigned y = 0; y < 16; ++y)
                for (unsigned x = 0; x < 32; ++x) {
                    const UBYTE* pixel = readback + (y * 32 + x) * 4;
                    bool inside = x >= 5 && x < 9 && y >= 3 && y < 5;
                    CHECK(pixel[1] == (inside ? 110 : 20)
                        && pixel[2] == (inside ? 80 : 60)
                        && pixel[3] == (inside ? 75 : 100));
                }
            CHECK(!AmigaOS3WritePixelArrayAlpha(source, 2, 1, 16, &raster, 5, 3, 4, 2, ~0UL));
            CHECK(!AmigaOS3WritePixelArrayAlpha(source, 2, 1, 48, &raster, 65535, 3, 4, 2, ~0UL));
            CHECK(!AmigaOS3WritePixelArrayAlpha(source, 2, 1, 48, &raster, 5, 3, 4, 2, 0));
            CHECK(!AmigaOS3WritePixelArrayAlpha(reinterpret_cast<APTR>(0xfffffff0UL), 2, 1, 48,
                &raster, 5, 3, 4, 2, ~0UL));
            // Native theme pens replace MorphOS ProcessPixelArray in the
            // toolbar spacer. Verify both one-pixel lines and all bounds.
            auto* drawInfo = GetScreenDrawInfo(screen);
            CHECK(drawInfo);
            if (drawInfo) {
                ULONG lightIndex = drawInfo->dri_Pens[SHINEPEN];
                ULONG darkIndex = drawInfo->dri_Pens[SHADOWPEN];
                ULONG lightRGB[3], darkRGB[3];
                GetRGB32(screen->ViewPort.ColorMap, lightIndex, 1, lightRGB);
                GetRGB32(screen->ViewPort.ColorMap, darkIndex, 1, darkRGB);
                CHECK(WritePixelArray(background, 0, 0, 128, &raster, 0, 0, 32, 16, RECTFMT_ARGB));
                ULONG oldPen = GetAPen(&raster);
                ULONG oldMode = GetDrMd(&raster);
                AmigaOS3DrawSeparator(&raster, 12, 6, 4, lightIndex, darkIndex);
                CHECK(GetAPen(&raster) == oldPen && GetDrMd(&raster) == oldMode);
                CHECK(ReadPixelArray(readback, 0, 0, 128, &raster, 0, 0, 32, 16, RECTFMT_ARGB));
                for (unsigned y = 0; y < 16; ++y)
                    for (unsigned x = 0; x < 32; ++x) {
                        const UBYTE* pixel = readback + (y * 32 + x) * 4;
                        const ULONG* rgb = y >= 6 && y < 10
                            ? (x == 12 ? lightRGB : (x == 11 ? darkRGB : nullptr)) : nullptr;
                        CHECK(pixel[1] == (rgb ? rgb[0] >> 24 : 20)
                            && pixel[2] == (rgb ? rgb[1] >> 24 : 60)
                            && pixel[3] == (rgb ? rgb[2] >> 24 : 100));
                    }
                FreeScreenDrawInfo(screen, drawInfo);
            }
            // A real layers clip region exercises partial read/write rows.
            auto* layerInfo = NewLayerInfo();
            auto* layer = layerInfo ? CreateUpfrontLayer(layerInfo, bitmap, 0, 0, 31, 15, LAYERSIMPLE, nullptr) : nullptr;
            auto* region = NewRegion();
            CHECK(layer && region);
            if (layer && region) {
                CHECK(WritePixelArray(background, 0, 0, 128, &raster, 0, 0, 32, 16, RECTFMT_ARGB));
                Rectangle clip = { 6, 4, 7, 4 };
                CHECK(OrRectRegion(region, &clip));
                InstallClipRegion(layer, region);
                for (unsigned y = 1; y < 3; ++y)
                    for (unsigned x = 2; x < 6; ++x) {
                        UBYTE* pixel = source + y * 48 + x * 4;
                        pixel[1] = 100;
                        pixel[2] = 50;
                        pixel[3] = 25;
                    }
                CHECK(AmigaOS3WritePixelArrayPremultipliedAlpha(source, 2, 1, 48,
                    layer->rp, 5, 3, 4, 2, 0xffffffff));
                InstallClipRegion(layer, nullptr);
                CHECK(ReadPixelArray(readback, 0, 0, 128, &raster, 0, 0, 32, 16, RECTFMT_ARGB));
                for (unsigned y = 0; y < 16; ++y)
                    for (unsigned x = 0; x < 32; ++x) {
                        const UBYTE* pixel = readback + (y * 32 + x) * 4;
                        bool inside = x >= 6 && x <= 7 && y == 4;
                        CHECK(pixel[1] == (inside ? 110 : 20)
                            && pixel[2] == (inside ? 80 : 60)
                            && pixel[3] == (inside ? 75 : 100));
                    }
            }
            if (region)
                DisposeRegion(region);
            if (layer)
                DeleteLayer(0, layer);
            if (layerInfo)
                DisposeLayerInfo(layerInfo);
            FreeBitMap(bitmap);
        }
        CloseScreen(screen);
    }
closeLibraries:
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    if (LayersBase) CloseLibrary(LayersBase);
    if (IntuitionBase) CloseLibrary(reinterpret_cast<Library*>(IntuitionBase));
    if (GfxBase) CloseLibrary(reinterpret_cast<Library*>(GfxBase));
    printf("RTG pixel-array compositing and layer clipping: %u failures\n", failures);
}

int main()
{
    checkRows();
    checkRTG();
    printf("Odyssey OS3 graphics test: %u checks, %u failures\n", checks, failures);
    return failures ? 20 : 0;
}
