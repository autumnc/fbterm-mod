/*
 *   Copyright © 2008-2010 dragchan <zgchan317@gmail.com>
 *   This file is part of FbTerm.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_GLYPH_H
#include FT_SYNTHESIS_H
#include "font.h"
#include "screen.h"
#include "fbconfig.h"

#define OFFSET(TYPE, MEMBER) ((size_t)(&(((TYPE *)0)->MEMBER)))
#define SUBS(a, b) ((a) > (b) ? (a) - (b) : (b) - (a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static FcCharSet *unicodeMap;
static FcFontSet *fontList;
static FcFontSet *fontListBold;
static FcFontSet *fontListItalic;

static FT_Library ftlib;
static FT_Face *fontFaces;
static FT_Face *fontFacesBold;
static FT_Face *fontFacesItalic;
static u32 *fontFlags;
static u32 *fontFlagsBold;
static u32 *fontFlagsItalic;

static Font::Glyph **glyphCache;
static bool *glyphCacheInited;
static Font::Glyph **glyphCacheBold;
static bool *glyphCacheInitedBold;
static Font::Glyph **glyphCacheItalic;
static bool *glyphCacheInitedItalic;

static void openFont(u32 index, FcFontSet *list, FT_Face *faces, u32 *flags);

DEFINE_INSTANCE(Font)


static FcFontSet *createFontList(const s8 *fontNames, u32 pixelSize)
{
	if (!fontNames || !*fontNames) return 0;

	FcPattern *pat = FcNameParse((FcChar8 *)fontNames);
	FcPatternAddDouble(pat, FC_PIXEL_SIZE, (double)pixelSize);
	FcPatternAddString(pat, FC_LANG, (FcChar8 *)"en");
	FcConfigSubstitute(NULL, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);

	FcResult result;
	FcCharSet *cs;
	FcFontSet *fs = FcFontSort(NULL, pat, FcTrue, &cs, &result);

	FcFontSet *list = 0;
	if (fs) {
		list = FcFontSetCreate();
		FcObjectSet *family = FcObjectSetCreate();
		FcObjectSetAdd(family, FC_FAMILY);

		for (u32 i = 0; i < fs->nfont; i++) {
			FcPattern *font = FcFontRenderPrepare(NULL, pat, fs->fonts[i]);
			if (!font) continue;

			bool same = false;
			for (u32 j = 0; j < list->nfont; j++) {
				if (FcPatternEqualSubset(list->fonts[j], font, family)) {
					same = true;
					break;
				}
			}
			if (same) FcPatternDestroy(font);
			else FcFontSetAdd(list, font);
		}
		FcObjectSetDestroy(family);
		if (!list->nfont) {
			FcFontSetDestroy(list);
			list = 0;
		}
		FcFontSetDestroy(fs);
	}
	FcPatternDestroy(pat);
	if (cs) FcCharSetDestroy(cs);
	return list;
}

Font *Font::createInstance()
{
	FcInit();

	s8 buf[64];
	Config::instance()->getOption("font-names", buf, sizeof(buf));

	FcPattern *pat = FcNameParse((FcChar8 *)(*buf ? buf : "mono"));

	u32 pixel_size = 12;
	Config::instance()->getOption("font-size", pixel_size);
	FcPatternAddDouble(pat, FC_PIXEL_SIZE, (double)pixel_size);

	FcPatternAddString(pat, FC_LANG, (FcChar8 *)"en");

	FcConfigSubstitute(NULL, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);

	FcResult result;
	FcFontSet *fs = FcFontSort(NULL, pat, FcTrue, &unicodeMap, &result);

	if (fs) {
		fontList = FcFontSetCreate();

		FcObjectSet *family = FcObjectSetCreate();
		FcObjectSetAdd(family, FC_FAMILY);

		for (u32 i = 0; i < fs->nfont; i++) {
			FcPattern *font = FcFontRenderPrepare(NULL, pat, fs->fonts[i]);
			if (!font) continue;

			bool same = false;
			for (u32 j = 0; j < fontList->nfont; j++) {
				if (FcPatternEqualSubset(fontList->fonts[j], font, family)) {
					same = true;
					break;
				}
			}

			if (same) {
				FcPatternDestroy(font);
			} else {
				FcFontSetAdd(fontList, font);
			}
		}

		FcObjectSetDestroy(family);
	}

	FcPatternDestroy(pat);
	if (fs) FcFontSetDestroy(fs);

	if (fontList) {
		s8 boldNames[256];
		Config::instance()->getOption("font-names-bold", boldNames, sizeof(boldNames));
		fontListBold = createFontList(boldNames, pixel_size);
	}

	if (fontList) {
		s8 italicNames[256];
		Config::instance()->getOption("font-names-italic", italicNames, sizeof(italicNames));
		fontListItalic = createFontList(italicNames, pixel_size);
	}

	if (fontList && fontList->nfont) return new Font();

	if (unicodeMap) FcCharSetDestroy(unicodeMap);
	if (fontList) FcFontSetDestroy(fontList);
	FcFini();
	return 0;
}

Font::Font()
{
	mHeight = mWidth = 0;

	fontFaces = new FT_Face[fontList->nfont];
	fontFlags = new u32[fontList->nfont];
	memset(fontFaces, 0, sizeof(FT_Face) * fontList->nfont);

	fontFacesBold = fontListBold ? new FT_Face[fontListBold->nfont] : 0;
	fontFlagsBold = fontListBold ? new u32[fontListBold->nfont] : 0;
	if (fontListBold) memset(fontFacesBold, 0, sizeof(FT_Face) * fontListBold->nfont);

	fontFacesItalic = fontListItalic ? new FT_Face[fontListItalic->nfont] : 0;
	fontFlagsItalic = fontListItalic ? new u32[fontListItalic->nfont] : 0;
	if (fontListItalic) memset(fontFacesItalic, 0, sizeof(FT_Face) * fontListItalic->nfont);

	glyphCache = new Glyph *[0x30000];
	glyphCacheInited = new bool[0x300];
	memset(glyphCacheInited, 0, sizeof(bool) * 0x300);

	glyphCacheBold = new Glyph *[0x30000];
	glyphCacheInitedBold = new bool[0x300];
	memset(glyphCacheInitedBold, 0, sizeof(bool) * 0x300);

	glyphCacheItalic = new Glyph *[0x30000];
	glyphCacheInitedItalic = new bool[0x300];
	memset(glyphCacheInitedItalic, 0, sizeof(bool) * 0x300);

	FT_Init_FreeType(&ftlib);
	openFont(0, fontList, fontFaces, fontFlags);

	FT_Face face = fontFaces[0];
	if (face == (FT_Face)-1) return;

	if (face->face_flags & FT_FACE_FLAG_SCALABLE) {
		mHeight = face->size->metrics.height >> 6;
		mWidth = face->size->metrics.max_advance >> 6;
		mDescender = face->size->metrics.descender >> 6;
	} else if (face->num_fixed_sizes) {
		double dsize;
		FcPatternGetDouble(fontList->fonts[0], FC_PIXEL_SIZE, 0, &dsize);

		FT_Bitmap_Size *sizes = face->available_sizes;
		u32 index = 0, diffmin = (u32)-1;
		for (u32 i = 0; i < face->num_fixed_sizes; i++) {
			u32 diff = SUBS(sizes[i].size >> 6, (u32)dsize);
			if (diff < diffmin ) {
				index = i;
				diffmin = diff;
			}
		}

		mHeight = sizes[index].height;
		mWidth = sizes[index].width;
		mDescender = face->size->metrics.descender >> 6;
	}


	u32 width = 0;
	Config::instance()->getOption("font-width", width);

	if (width) {
		s8 buf[64];
		Config::instance()->getOption("font-width", buf, sizeof(buf));

		if (buf[0] == '+' || buf[0] == '-') mWidth += (s32)width;
		else mWidth = width;
	}

	u32 height = 0;
	Config::instance()->getOption("font-height", height);

	if (height) {
		s8 buf[64];
		Config::instance()->getOption("font-height", buf, sizeof(buf));

		if (buf[0] == '+' || buf[0] == '-') mHeight += (s32)height;
		else mHeight = height;
	}
}

Font::~Font()
{
	for (u32 i = 0; i < 256; i++) {
		if (glyphCacheInited[i]) {
			for (u32 j = 0; j < 256; j++) {
				if (glyphCache[i * 256 + j]) {
					delete[] (u8 *)glyphCache[i * 256 + j];
				}
			}
		}
		if (glyphCacheInitedBold[i]) {
			for (u32 j = 0; j < 256; j++) {
				if (glyphCacheBold[i * 256 + j]) {
					delete[] (u8 *)glyphCacheBold[i * 256 + j];
				}
			}
		}
		if (glyphCacheInitedItalic[i]) {
			for (u32 j = 0; j < 256; j++) {
				if (glyphCacheItalic[i * 256 + j]) {
					delete[] (u8 *)glyphCacheItalic[i * 256 + j];
				}
			}
		}
	}

	delete[] glyphCache;
	delete[] glyphCacheInited;
	delete[] glyphCacheBold;
	delete[] glyphCacheInitedBold;
	delete[] glyphCacheItalic;
	delete[] glyphCacheInitedItalic;

	for (u32 i = 0; i < fontList->nfont; i++) {
		if (fontFaces[i] && fontFaces[i] != (FT_Face)-1) {
			FT_Done_Face(fontFaces[i]);
		}
	}
	delete[] fontFaces;
	delete[] fontFlags;

	if (fontFacesBold) {
		for (u32 i = 0; i < fontListBold->nfont; i++) {
			if (fontFacesBold[i] && fontFacesBold[i] != (FT_Face)-1) {
				FT_Done_Face(fontFacesBold[i]);
			}
		}
		delete[] fontFacesBold;
		delete[] fontFlagsBold;
	}

	if (fontFacesItalic) {
		for (u32 i = 0; i < fontListItalic->nfont; i++) {
			if (fontFacesItalic[i] && fontFacesItalic[i] != (FT_Face)-1) {
				FT_Done_Face(fontFacesItalic[i]);
			}
		}
		delete[] fontFacesItalic;
		delete[] fontFlagsItalic;
	}

	FT_Done_FreeType(ftlib);
	FcCharSetDestroy(unicodeMap);
	FcFontSetDestroy(fontList);
	if (fontListBold) FcFontSetDestroy(fontListBold);
	if (fontListItalic) FcFontSetDestroy(fontListItalic);
	FcFini();
}

void Font::showInfo(bool verbose)
{
	if (!verbose) return;

	printf("[font] width: %dpx, height: %dpx, ordered list: ", mWidth, mHeight);

	u32 index;
	FcChar8 *family;
	for (index = 0; index < fontList->nfont - 1; index++) {
		FcPatternGetString(fontList->fonts[index], FC_FAMILY, 0, &family);
		printf("%s, ", family);
	}

	FcPatternGetString(fontList->fonts[index], FC_FAMILY, 0, &family);
	printf("%s\n", family);
}

static void openFont(u32 index, FcFontSet *list, FT_Face *faces, u32 *flags)
{
	if (index >= list->nfont) return;

	FcPattern *pattern = list->fonts[index];

	FcChar8 *name = (FcChar8 *)"";
	FcPatternGetString(pattern, FC_FILE, 0, &name);

	int id = 0;
	FcPatternGetInteger (pattern, FC_INDEX, 0, &id);

	FT_Face face;
	if (FT_New_Face(ftlib, (const char *)name, id, &face)) {
		fontFaces[index] = (FT_Face)-1;
		return;
	}

	double ysize;
	FcPatternGetDouble(pattern, FC_PIXEL_SIZE, 0, &ysize);
	FT_Set_Pixel_Sizes(face, 0, (FT_UInt)ysize);

	int load_flags = FT_LOAD_DEFAULT;

	FcBool scalable, antialias;
	FcPatternGetBool(pattern, FC_SCALABLE, 0, &scalable);
	FcPatternGetBool(pattern, FC_ANTIALIAS, 0, &antialias);

	if (scalable && antialias) load_flags |= FT_LOAD_NO_BITMAP;

	if (antialias) {
		FcBool hinting;
		int hint_style;
		FcPatternGetBool(pattern, FC_HINTING, 0, &hinting);
		FcPatternGetInteger(pattern, FC_HINT_STYLE, 0, &hint_style);

		load_flags |= FT_LOAD_TARGET_NORMAL; /* hardcode hinting to normal because there're some issues with infinality */
	} else {
		load_flags |= FT_LOAD_TARGET_MONO;
	}

	faces[index] = face;
	flags[index] = load_flags;
}

static int fontIndex(u32 unicode, FcFontSet *list)
{
	if (!list) return -1;

	if (unicode <= 0xffff) {
		if (!FcCharSetHasChar(unicodeMap, unicode)) return -1;

		FcCharSet *charset;
		for (u32 i = 0; i < list->nfont; i++) {
			FcPatternGetCharSet(list->fonts[i], FC_CHARSET, 0, &charset);
			if (FcCharSetHasChar(charset, unicode)) return i;
		}

		return -1;
	}

	for (u32 i = 0; i < list->nfont; i++) {
		FT_Face face = fontFaces[i];
		if (!face || face == (FT_Face)-1) {
			if (list == fontList) openFont(i, fontList, fontFaces, fontFlags);
			else if (list == fontListBold) openFont(i, fontListBold, fontFacesBold, fontFlagsBold);
			else if (list == fontListItalic) openFont(i, fontListItalic, fontFacesItalic, fontFlagsItalic);
			else return -1;
			face = (list == fontList) ? fontFaces[i] :
				(list == fontListBold) ? fontFacesBold[i] : fontFacesItalic[i];
		}
		if (face && face != (FT_Face)-1 && FT_Get_Char_Index(face, (FT_ULong)unicode))
			return i;
	}

	return -1;
}

Font::Glyph *Font::getGlyph(u32 unicode, bool bold, bool italic)
{
	Glyph ***cache = &glyphCache;
	bool *cacheInited = glyphCacheInited;
	FcFontSet *list = fontList;
	FT_Face *faces = fontFaces;
	u32 *loadFlags = fontFlags;
	bool needEmbolding = false;
	bool needShear = false;
	bool noCache = (unicode >= 0x30000);

	if (bold && italic) {
			noCache = true;
			int fi = fontIndex(unicode, fontListItalic);
			if (fi >= 0) {
				list = fontListItalic;
				faces = fontFacesItalic;
				loadFlags = fontFlagsItalic;
				needEmbolding = true;
			} else {
				fi = fontIndex(unicode, fontListBold);
				if (fi >= 0) {
					list = fontListBold;
					faces = fontFacesBold;
					loadFlags = fontFlagsBold;
					needShear = true;
				} else {
					fi = fontIndex(unicode, fontList);
					if (fi == -1) return 0;
					needEmbolding = true;
					needShear = true;
				}
			}
		} else if (bold) {
		cache = &glyphCacheBold;
		cacheInited = glyphCacheInitedBold;
		if (fontListBold) {
			list = fontListBold;
			faces = fontFacesBold;
			loadFlags = fontFlagsBold;
		} else {
			needEmbolding = true;
		}
	} else if (italic) {
		cache = &glyphCacheItalic;
		cacheInited = glyphCacheInitedItalic;
		if (fontListItalic) {
			list = fontListItalic;
			faces = fontFacesItalic;
			loadFlags = fontFlagsItalic;
		} else {
			needShear = true;
		}
	}

	if (!noCache) {
		if (!cacheInited[unicode >> 8]) {
			cacheInited[unicode >> 8] = true;
			memset(&(*cache)[unicode & ~0xff], 0, sizeof(Glyph *) * 0x100);
		}
		if ((*cache)[unicode]) return (*cache)[unicode];
	}

	int i = fontIndex(unicode, list);
	bool useFallback = false;

	if (i == -1) {
		i = fontIndex(unicode, fontList);
		if (i == -1) return 0;
		useFallback = true;
		if (bold) needEmbolding = true;
		if (italic) needShear = true;
	}

	if (useFallback) {
		if (!fontFaces[i]) openFont(i, fontList, fontFaces, fontFlags);
		if (fontFaces[i] == (FT_Face)-1) return 0;
	} else {
		if (!faces[i]) openFont(i, list, faces, loadFlags);
		if (faces[i] == (FT_Face)-1) return 0;
	}

	FT_Face face = useFallback ? fontFaces[i] : faces[i];
	u32 flags = useFallback ? fontFlags[i] : loadFlags[i];

	
	FT_UInt index = FT_Get_Char_Index(face, (FT_ULong)unicode);
	if (!index) return 0;

	if ((needEmbolding || needShear) && (face->face_flags & FT_FACE_FLAG_SCALABLE))
		flags |= FT_LOAD_NO_BITMAP;
	FT_Load_Glyph(face, index, FT_LOAD_RENDER | flags);


	if (needShear && !(face->face_flags & FT_FACE_FLAG_SCALABLE))
		needShear = false;

	FT_Bitmap &bitmap = face->glyph->bitmap;

	s32 shearOffset = 0;
	u32 extraWidth = 0;
	if (needShear) {
		shearOffset = (face->glyph->metrics.height >> 6) / 4;
		if (shearOffset < 0) shearOffset = 0;
		extraWidth = shearOffset;
	}

	u32 x, y, w, h, nx, ny, nw, nh;
	x = y = 0;
	w = nw = bitmap.width + extraWidth;
	h = nh = bitmap.rows;
	Screen::instance()->rotateRect(x, y, nw, nh);

	Glyph *glyph = (Glyph *)new u8[OFFSET(Glyph, pixmap) + nw * nh];
	memset(glyph->pixmap, 0, nw * nh);
	glyph->left = (face->glyph->metrics.horiBearingX >> 6) - (s32)extraWidth;
	glyph->top = mHeight - 1 + mDescender - (face->glyph->metrics.horiBearingY >> 6);
	glyph->width = (face->glyph->metrics.width >> 6) + extraWidth;
	glyph->height = face->glyph->metrics.height >> 6;
	glyph->pitch = nw;

	u8 *buf = bitmap.buffer;
	for (y = 0; y < h; y++, buf += bitmap.pitch) {
		s32 shearShift = 0;
		if (needShear) {
			shearShift = shearOffset * ((s32)h - 1 - (s32)y) / ((s32)h - 1);
			if (shearShift < 0) shearShift = 0;
			if (shearShift > (s32)extraWidth) shearShift = extraWidth;
		}

		for (x = 0; x < w; x++) {
			nx = x, ny = y;
			Screen::instance()->rotatePoint(w, h, nx, ny);

			if (x < extraWidth) continue;

			u8 val;
			s32 srcX = (s32)(x - extraWidth) - shearShift;
			if (srcX < 0 || srcX >= (s32)bitmap.width) {
				val = 0;
			} else {
				val = (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) ?
					((buf[(srcX >> 3)] & (0x80 >> (srcX & 7))) ? 0xff : 0) : buf[srcX];
			}

			glyph->pixmap[ny * nw + nx] = val;
		}
	}

	if (needEmbolding) {
		for (y = 0; y < nh; y++) {
			for (x = nw - 1; x > 0; x--) {
				u32 idx = y * nw + x;
				u16 blended = glyph->pixmap[idx] + (glyph->pixmap[idx - 1] * 3 / 4);
				glyph->pixmap[idx] = blended > 255 ? 255 : (u8)blended;
			}
		}
	}

	if (!noCache) (*cache)[unicode] = glyph;
	return glyph;
}

s32 Font::glyphWidth(u32 unicode)
{
	Glyph *g = getGlyph(unicode);
	return g ? g->width : 0;
}
