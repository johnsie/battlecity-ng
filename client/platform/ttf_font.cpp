#include "ttf_font.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imstb_truetype.h"

bool BCTtfFont::Load(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) return false;
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size <= 0) { fclose(f); return false; }
	data.resize((size_t)size);
	size_t got = fread(data.data(), 1, (size_t)size, f);
	fclose(f);
	if (got != (size_t)size) { data.clear(); return false; }
	return true;
}

bool BCTtfFont::Bake(float pixelHeight, int atlasSize, std::vector<unsigned char> &rgba) {
	if (data.empty()) return false;
	std::vector<unsigned char> alpha((size_t)atlasSize * atlasSize);
	stbtt_bakedchar baked[95];
	int rows = stbtt_BakeFontBitmap(data.data(), 0, pixelHeight, alpha.data(), atlasSize, atlasSize, 32, 95, baked);
	if (rows <= 0) return false;     // negative: not everything fitted
	for (int i = 0; i < 95; i++) {
		glyphs[i].x0 = baked[i].x0; glyphs[i].y0 = baked[i].y0;
		glyphs[i].x1 = baked[i].x1; glyphs[i].y1 = baked[i].y1;
		glyphs[i].xoff = baked[i].xoff; glyphs[i].yoff = baked[i].yoff;
		glyphs[i].advance = baked[i].xadvance;
	}
	rgba.assign((size_t)atlasSize * atlasSize * 4, 255);
	for (size_t i = 0; i < alpha.size(); i++) rgba[i * 4 + 3] = alpha[i];
	return true;
}

const BCGlyph *BCTtfFont::Glyph(unsigned char c) const {
	if (c < 32 || c > 126) return nullptr;
	return &glyphs[c - 32];
}
