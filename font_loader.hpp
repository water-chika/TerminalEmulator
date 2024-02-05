#pragma once

#include <exception>
#include <cassert>

#include <ft2build.h>
#include FT_FREETYPE_H

class font_loader {
public:
    font_loader() {
        FT_Library font_library;
		if (FT_Init_FreeType(&font_library)) {
			throw std::runtime_error{ "failed to initialize font library" };
		}
		FT_Face font_face;
		if (FT_New_Face(font_library, "C:\\Windows\\Fonts\\consola.ttf", 0, &font_face)) {
			throw std::runtime_error{ "failed to open font file" };
		}
		if (FT_Set_Char_Size(font_face, 0, 16 * 64, 512, 512)) {
			throw std::runtime_error{ "failed to set font size" };
		}
		auto glyph_index = FT_Get_Char_Index(font_face, 'A');
		assert(glyph_index != 0);
		if (FT_Load_Glyph(font_face, glyph_index, FT_LOAD_DEFAULT)) {
			throw std::runtime_error{ "failed to load glyph" };
		}
		if (FT_Render_Glyph(font_face->glyph, FT_RENDER_MODE_NORMAL)) {
			throw std::runtime_error{ "failed to render glyph" };
		}
        m_glyph = font_face->glyph;
    }
    auto get_bitmap() {
        return &m_glyph->bitmap;
    }
private:
    FT_GlyphSlot m_glyph;
};