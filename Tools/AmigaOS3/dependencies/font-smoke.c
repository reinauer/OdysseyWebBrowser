/* Discover an installed font, shape a ligature and render it with Cairo. */
#include <stdio.h>
#include <string.h>
#include <fontconfig/fontconfig.h>
#include <cairo.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <hb-icu.h>

int main(void)
{
    hb_unicode_funcs_t* unicode = hb_icu_get_unicode_funcs();
    if (!unicode || hb_unicode_script(unicode, 0x0627) != HB_SCRIPT_ARABIC
        || hb_unicode_general_category(unicode, 'A') != HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER)
        return 9;
    puts("PASS HarfBuzz ICU Unicode properties");
    puts("font smoke: loading native fontconfig configuration");
    FcConfig* config = FcInitLoadConfigAndFonts();
    if (!config) return 1;
    FcPattern* pattern = FcNameParse((const FcChar8*)"Noto Sans:size=20");
    FcConfigSubstitute(config, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result;
    FcPattern* match = FcFontMatch(config, pattern, &result);
    FcChar8* path = NULL;
    if (!match || FcPatternGetString(match, FC_FILE, 0, &path) != FcResultMatch)
        return 2;
    printf("font match: %s\n", path);
    if (!strstr((const char*)path, "NotoSans-Regular.ttf")) return 3;
    FT_Library library;
    FT_Face face;
    if (FT_Init_FreeType(&library) || FT_New_Face(library, (const char*)path, 0, &face)
        || FT_Set_Char_Size(face, 0, 20 * 64, 72, 72))
        return 4;
    hb_font_t* font = hb_ft_font_create_referenced(face);
    hb_buffer_t* buffer = hb_buffer_create();
    hb_buffer_set_unicode_funcs(buffer, unicode);
    hb_buffer_add_utf8(buffer, "office", -1, 0, -1);
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(font, buffer, NULL, 0);
    unsigned count;
    hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, &count);
    hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, NULL);
    int advance = 0;
    if (!count || count >= 6) return 5;
    for (unsigned i = 0; i < count; ++i) {
        if (!info[i].codepoint) return 6;
        advance += positions[i].x_advance;
    }
    if (advance <= 0) return 7;
    printf("PASS FreeType/HarfBuzz shaping: office -> %u glyphs, advance %d\n", count, advance);

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 160, 40);
    cairo_t* context = cairo_create(surface);
    cairo_set_source_rgb(context, 1, 1, 1);
    cairo_paint(context);
    cairo_font_face_t* cairoFace = cairo_ft_font_face_create_for_ft_face(face, 0);
    cairo_set_font_face(context, cairoFace);
    cairo_set_font_size(context, 20);
    cairo_set_source_rgb(context, 0, 0, 0);
    cairo_move_to(context, 4, 26);
    cairo_show_text(context, "Odyssey");
    cairo_surface_flush(surface);
    unsigned changed = 0;
    unsigned char* pixels = cairo_image_surface_get_data(surface);
    for (int y = 0; y < 40; ++y)
        for (int x = 0; x < 160; ++x)
            if (pixels[y * cairo_image_surface_get_stride(surface) + x * 4 + 1] != 255)
                ++changed;
    if (cairo_status(context) || changed < 50
        || cairo_surface_write_to_png(surface, "SYS:font-smoke.png"))
        return 8;
    printf("PASS Cairo glyph rendering: %u covered pixels\n", changed);
    cairo_destroy(context);
    cairo_surface_destroy(surface);
    cairo_font_face_destroy(cairoFace);
    hb_buffer_destroy(buffer);
    hb_font_destroy(font);
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    FcPatternDestroy(match);
    FcPatternDestroy(pattern);
    FcConfigDestroy(config);
    puts("PASS native font smoke");
    return 0;
}
