#pragma once

#include <exception>
#include <cassert>

#include <ft2build.h>
#include FT_FREETYPE_H

class font_loader {
public:
    font_loader() {
		if (FT_Init_FreeType(&m_library)) {
			throw std::runtime_error{ "failed to initialize font library" };
		}
		if (FT_New_Face(m_library, "C:\\Windows\\Fonts\\consola.ttf", 0, &m_face)) {
			throw std::runtime_error{ "failed to open font file" };
		}
		if (FT_Set_Char_Size(m_face, 0, 16 * 64, 512, 512)) {
			throw std::runtime_error{ "failed to set font size" };
		}
        m_glyph = m_face->glyph;
    }
	void render_char(char c) {
		auto glyph_index = FT_Get_Char_Index(m_face, c);
		assert(glyph_index != 0);
		if (FT_Load_Glyph(m_face, glyph_index, FT_LOAD_DEFAULT)) {
			throw std::runtime_error{ "failed to load glyph" };
		}
		if (FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL)) {
			throw std::runtime_error{ "failed to render glyph" };
		}
	}
    auto get_bitmap() {
        return &m_glyph->bitmap;
    }
private:
	FT_Library m_library;
	FT_Face m_face;
    FT_GlyphSlot m_glyph;
};