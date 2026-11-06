// Minimal TrueType text support for the in-game text: bakes Vera.ttf (ASCII) into an alpha atlas at an exact
// pixel size, so text can be drawn at the window's real resolution instead of being scaled up from a bitmap font.
#ifndef BC_TTF_FONT_H
#define BC_TTF_FONT_H
#include <vector>

struct BCGlyph {
	float x0, y0, x1, y1;     // rectangle in the atlas (pixels)
	float xoff, yoff;         // draw offset from the pen position / baseline
	float advance;            // pen advance
};

class BCTtfFont {
public:
	bool Load(const char *path);
	// Bakes ASCII 32..126 at `pixelHeight`; rgba is atlasSize x atlasSize (white with coverage in alpha)
	bool Bake(float pixelHeight, int atlasSize, std::vector<unsigned char> &rgba);
	const BCGlyph *Glyph(unsigned char c) const;
	bool Loaded() const { return !data.empty(); }
private:
	std::vector<unsigned char> data;
	BCGlyph glyphs[95];
};

#endif
