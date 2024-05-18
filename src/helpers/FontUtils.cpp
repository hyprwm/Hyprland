#include <pango/pangocairo.h>

#include "FontUtils.hpp"

int count_missing_glyphs(const std::string& text, const std::string& fontname) {
    auto* context   = pango_font_map_create_context(pango_cairo_font_map_get_default());
    auto* layout    = pango_layout_new(context);
    auto* font_desc = pango_font_description_from_string(fontname.c_str());

    pango_layout_set_font_description(layout, font_desc);
    pango_layout_set_text(layout, text.c_str(), -1);

    auto ct = pango_layout_get_unknown_glyphs_count(layout);

    pango_font_description_free(font_desc);
    g_object_unref(layout);
    g_object_unref(context);

    return ct;
}
