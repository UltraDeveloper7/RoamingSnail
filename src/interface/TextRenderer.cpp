#include "../precompiled.h"
#include "TextRenderer.hpp"

TextRenderer::TextRenderer() 
	: text_shader_(std::make_unique<Shader>(Config::text_vertex_path, Config::text_fragment_path))
	, vao_{}, vbo_{}
{
	Load();

	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);
	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

TextRenderer::~TextRenderer() {
	for (auto f : faces_) {
		if (f) FT_Done_Face(f);
	}
	faces_.clear();
	if (ft_) { FT_Done_FreeType(ft_); ft_ = nullptr; }
}

void TextRenderer::Init()
{
	projection_matrix_ = glm::ortho(0.0f, static_cast<float>(Config::width), 0.0f, static_cast<float>(Config::height));
	Update();
}

void TextRenderer::UpdateProjectionMatrix(const int width, const int height)
{
	projection_matrix_ = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height));
	font_scale_ = glm::vec2(width / static_cast<float>(Config::width), height / static_cast<float>(Config::height));
}

void TextRenderer::Update() const
{
	text_shader_->Bind();
	text_shader_->SetMat4(projection_matrix_, "projectionMatrix");
}

static void BlitBGRAtoAlpha(const FT_Bitmap& src, std::vector<unsigned char>& outAlpha) {
	// Convert BGRA rows (pitch may be negative) to a tight 8-bit alpha buffer
	const int w = src.width;
	const int h = src.rows;
	outAlpha.resize(static_cast<size_t>(w) * h);

	const bool negPitch = (src.pitch < 0);
	const unsigned char* row = src.buffer + (negPitch ? (h - 1) * static_cast<size_t>(-src.pitch) : 0);
	const int step = negPitch ? -src.pitch : src.pitch;

	for (int y = 0; y < h; ++y) {
		const unsigned char* p = row;
		for (int x = 0; x < w; ++x) {
			// BGRA -> take A
			outAlpha[static_cast<size_t>(y) * w + x] = p[3];
			p += 4;
		}
		row += step;
	}
}

bool TextRenderer::EnsureGlyph(uint32_t cp) {
	if (characters_.find(cp) != characters_.end()) return true;
	if (faces_.empty()) return false;

	// Try each face until one has the glyph
	for (FT_Face face : faces_) {
		if (!face) continue;

		// quick check: missing glyph?
		if (FT_Get_Char_Index(face, cp) == 0) continue;

		// Try to load & render; include FT_LOAD_COLOR so color bitmaps work (we’ll convert)
		const FT_Int32 flags = FT_LOAD_RENDER | FT_LOAD_COLOR;
		if (FT_Load_Char(face, cp, flags)) continue;

		const FT_GlyphSlot g = face->glyph;
		const FT_Bitmap& bm = g->bitmap;

		std::unique_ptr<Texture> tex;
		if (bm.pixel_mode == FT_PIXEL_MODE_BGRA) {
			// Convert to 1-channel alpha so your existing text shader keeps working
			std::vector<unsigned char> A;
			BlitBGRAtoAlpha(bm, A);
			tex = std::make_unique<Texture>(A.data(), bm.width, bm.rows, 1);
		}
		else if (bm.pixel_mode == FT_PIXEL_MODE_GRAY) {
			// Most normal monochrome glyphs end here
			tex = std::make_unique<Texture>(bm.buffer, bm.width, bm.rows, 1);
		}
		else {
			// Other modes (mono bitmap, etc.): expand to GRAY
			// Do a slow fallback (treat non-zero as opaque)
			std::vector<unsigned char> A(static_cast<size_t>(bm.width) * bm.rows, 0);
			const unsigned char* row = bm.buffer;
			for (int y = 0; y < bm.rows; ++y) {
				for (int x = 0; x < bm.width; ++x) {
					A[static_cast<size_t>(y) * bm.width + x] = row[x] ? 255 : 0;
				}
				row += bm.pitch;
			}
			tex = std::make_unique<Texture>(A.data(), bm.width, bm.rows, 1);
		}

		Character ch{
			std::move(tex),
			glm::ivec2(bm.width, bm.rows),
			glm::ivec2(g->bitmap_left, g->bitmap_top),
			static_cast<unsigned>(g->advance.x)
		};
		characters_.emplace(cp, std::move(ch));
		return true;
	}
	return false; // none of the faces had this codepoint
}

void TextRenderer::Render(std::vector<Text>& texts) {
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	text_shader_->Bind();
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(vao_);

	for (auto& [position_x, position_y, text, scale, alignment, selected] : texts) {
		// Subtle accent for selected items instead of hard red
		const glm::vec3 base = glm::vec3(1.0f);
		const glm::vec3 accent = glm::vec3(0.98f, 0.86f, 0.35f); // warm highlight
		glm::vec3 color = selected ? glm::mix(base, accent, 0.65f) : base;

		// Positioning and per-item scale
		const glm::vec2 saved_scale = font_scale_;
		font_scale_ *= scale;

		float width_px = CalculateTextWidth(text);
		float posX = position_x;
		if (alignment == Alignment::CENTER)      posX -= width_px * 0.5f;
		else if (alignment == Alignment::RIGHT)  posX -= width_px;

		float posY = position_y;

		// Shadow pass (small offset, lower opacity)
		if (draw_shadow_) {
			text_shader_->SetVec3(shadow_color_, "textColor");      // keep pure black
			text_shader_->SetFloat(shadow_alpha_, "alphaMul");       // NEW: modulate alpha in shader
			const glm::vec2 lift = selected ? glm::vec2(3.0f, -3.0f) : shadow_px_;
			float sx = posX + lift.x, sy = posY + lift.y;
			for (size_t i = 0; i < text.size();) {
				uint32_t cp = NextCodepoint(text, i);
				RenderGlyph(sx, sy, cp);
			}
		}

		// Main text pass (full opacity)
		text_shader_->SetVec3(color, "textColor");
		text_shader_->SetFloat(1.0f, "alphaMul");                    // NEW: reset alpha
		for (size_t i = 0; i < text.size();) {
			uint32_t cp = NextCodepoint(text, i);
			RenderGlyph(posX, posY, cp);
		}

		font_scale_ = saved_scale;
	}

	texts.clear();
	glBindVertexArray(0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glEnable(GL_DEPTH_TEST);
}


void TextRenderer::RenderGlyph(float& x, float& y, uint32_t cp) {
	if (!EnsureGlyph(cp)) {
		cp = 0xFFFD; // replacement
		if (!EnsureGlyph(cp)) return;
	}
	const Character& chd = characters_.at(cp);

	const float px = x + chd.bearing.x * font_scale_.x;
	const float py = y - (chd.size.y - chd.bearing.y) * font_scale_.y;
	const float w = chd.size.x * font_scale_.x;
	const float h = chd.size.y * font_scale_.y;

	const float vertices[6][4] = {
		{ px,     py + h, 0.0f, 0.0f },
		{ px,     py,     0.0f, 1.0f },
		{ px + w, py,     1.0f, 1.0f },
		{ px,     py + h, 0.0f, 0.0f },
		{ px + w, py,     1.0f, 1.0f },
		{ px + w, py + h, 1.0f, 0.0f }
	};

	if (chd.texture) chd.texture->Bind();
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof vertices, vertices);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	x += (chd.advance >> 6) * font_scale_.x;
}

float TextRenderer::CalculateTextWidth(const std::string& text) {
	float width = 0.0f;
	for (size_t i = 0; i < text.size();) {
		uint32_t cp = NextCodepoint(text, i);
		if (!EnsureGlyph(cp)) cp = 0xFFFD;
		const auto& ch = characters_.at(cp);
		width += (ch.advance >> 6) * font_scale_.x;
	}
	return width;
}

uint32_t TextRenderer::NextCodepoint(const std::string& s, size_t& i) const {
	if (i >= s.size()) return 0;
	unsigned char c0 = static_cast<unsigned char>(s[i++]);
	if (c0 < 0x80) return c0;
	uint32_t cp = 0; int extra = 0;
	if ((c0 & 0xE0) == 0xC0) { cp = (c0 & 0x1F); extra = 1; }
	else if ((c0 & 0xF0) == 0xE0) { cp = (c0 & 0x0F); extra = 2; }
	else if ((c0 & 0xF8) == 0xF0) { cp = (c0 & 0x07); extra = 3; }
	else return 0xFFFD;
	while (extra-- > 0) {
		if (i >= s.size()) return 0xFFFD;
		unsigned char cx = static_cast<unsigned char>(s[i++]);
		if ((cx & 0xC0) != 0x80) return 0xFFFD;
		cp = (cp << 6) | (cx & 0x3F);
	}
	return cp;
}

bool TextRenderer::AddFaceFromPath(const std::filesystem::path& p) {
	if (!std::filesystem::exists(p)) return false;
	FT_Face f{};
	if (FT_New_Face(ft_, p.string().c_str(), 0, &f)) return false;
	FT_Set_Pixel_Sizes(f, 0, Config::default_font_size);
	FT_Select_Charmap(f, FT_ENCODING_UNICODE);
	faces_.push_back(f);
	face_paths_.push_back(p);
	return true;
}

void TextRenderer::Load() {
	if (FT_Init_FreeType(&ft_))
		throw std::exception("Could not init FreeType Library");

	// 1) Primary UI font (KEEP THIS AS A NORMAL TEXT FONT)
	//    e.g. "NotoSans-Regular.ttf" (not the symbols file)
	const auto primary = std::filesystem::current_path()
		/ "assets" / "fonts" / Config::font_path;
	if (!AddFaceFromPath(primary))
		throw std::exception("Failed to load primary UI font");

	// 2) Fallbacks (try symbols + system)
	// assets fallback: NotoSansSymbols2-Regular.ttf
	auto fb1 = std::filesystem::current_path()
		/ "assets" / "fonts" / "NotoSansSymbols2-Regular.ttf";
	AddFaceFromPath(fb1);

#ifdef _WIN32
	// Windows: Segoe UI Symbol
	std::filesystem::path fb2 = "C:/Windows/Fonts/seguisym.ttf";
	AddFaceFromPath(fb2);
#endif

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	// Warm up ASCII from primary
	for (uint32_t cp = 32; cp < 128; ++cp) {
		EnsureGlyph(cp);
	}
}