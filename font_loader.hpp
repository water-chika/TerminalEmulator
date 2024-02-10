#pragma once

#include <exception>
#include <cassert>
#include <map>
#include <string>
#include <stdexcept>

#include <ft2build.h>
#include FT_FREETYPE_H

enum os{
    eWindows,
    eLinux
};

namespace build_info {
    constexpr os runtime_os =
#if WIN32
	    os::eWindows
#elif __unix__
	    os::eLinux
#endif
	    ;
};

class font_loader {
public:
    font_loader() {
		if (FT_Init_FreeType(&m_library)) {
			throw std::runtime_error{ "failed to initialize font library" };
		}
		auto os_font_paths = std::map<os, std::string>{};
		os_font_paths.emplace(os::eWindows, "C:/Windows/Fonts/consola.ttf");
		os_font_paths.emplace(os::eLinux,   "/usr/share/fonts/gnu-free/FreeMono.otf");
		auto font_path = os_font_paths[build_info::runtime_os];
		if (FT_New_Face(m_library, font_path.c_str(), 0, &m_face)) {
			throw std::runtime_error{ "failed to open font file" };
		}
		if (FT_Set_Char_Size(m_face, 0, 16 * 64, 512, 512)) {
			throw std::runtime_error{ "failed to set font size" };
		}
        m_glyph = m_face->glyph;
    }
	void set_char_size(uint32_t width, uint32_t height) {
		if (FT_Set_Char_Size(m_face, width<<6, height<<6, 72, 72)) {
			throw std::runtime_error{ "failed to set font size" };
		}
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
    auto get_glyph() {
        return m_glyph;
    }
private:
	FT_Library m_library;
	FT_Face m_face;
    FT_GlyphSlot m_glyph;
};
