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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include "screen.h"
#include "font.h"
#include "fbshellman.h"
#include "fbconfig.h"
#include "fbdev.h"
#include "config.h"
#ifdef ENABLE_VESA
#include "vesadev.h"
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define redraw(args...) (FbShellManager::instance()->redraw(args))

static const s8 show_cursor[] = "\e[?25h";
static const s8 hide_cursor[] = "\e[?25l";
static const s8 disable_blank[] = "\e[9;0]";
static const s8 enable_blank[] = "\e[9;10]";
static const s8 clear_screen[] = "\e[2J\e[H";

DEFINE_INSTANCE(Screen)

Screen *Screen::createInstance()
{
	if (!Font::instance() || !FW(1) || !FH(1)) {
		fprintf(stderr, "init font error!\n");
		return 0;
	}

	Screen *pScreen = 0;

#ifdef ENABLE_VESA
	s8 buf[16];
	Config::instance()->getOption("vesa-mode", buf, sizeof(buf));
	if (!strcmp(buf, "list")) {
		VesaDev::printModes();
		return 0;
	}

	u32 mode = 0;
	Config::instance()->getOption("vesa-mode", mode);

	if (!mode) pScreen = FbDev::initFbDev();
	if (!pScreen) pScreen = VesaDev::initVesaDev(mode);
#else
	pScreen = FbDev::initFbDev();
#endif

	if (!pScreen) return 0;

	if (pScreen->mRotateType == Rotate90 || pScreen->mRotateType == Rotate270) {
		u32 tmp = pScreen->mWidth;
		pScreen->mWidth = pScreen->mHeight;
		pScreen->mHeight = tmp;
	}

	if (!pScreen->mCols) pScreen->mCols = pScreen->mWidth / FW(1);
	if (!pScreen->mRows) pScreen->mRows = pScreen->mHeight / FH(1);

	if (!pScreen->mCols || !pScreen->mRows) {
		fprintf(stderr, "font size is too huge!\n");
		delete pScreen;
		return 0;
	}

	pScreen->initFillDraw();
	pScreen->initDoubleBuffer();
	return pScreen;
}

Screen::Screen()
{
	mWidth = mHeight = 0;
	mCols = mRows = 0;
	mBitsPerPixel = mBytesPerLine = 0;

	mScrollEnable = true;
	mScrollType = Redraw;
	mOffsetMax = 0;
	mOffsetCur = 0;

	mVMemBase = 0;
	mRealFbBase = 0;
	mBackBuffer = 0;
	mFbMemSize = 0;
	mDirty = false;
	mFbFd = -1;
	mPalette = 0;
	mHasCustomBackground = false;
	memset(mFillColors, 0, sizeof(mFillColors));
	mDirectColorTable = 0;

	u32 type = Rotate0;
	Config::instance()->getOption("screen-rotate", type);
	if (type > Rotate270) type = Rotate0;
	mRotateType = (RotateType)type;

	s32 ret = write(STDIN_FILENO, hide_cursor, sizeof(hide_cursor) - 1);
	ret = write(STDIN_FILENO, disable_blank, sizeof(disable_blank) - 1);
}

Screen::~Screen()
{
	Font::uninstance();
	endFillDraw();

	if (mBackBuffer) {
		delete[] mBackBuffer;
		mBackBuffer = 0;
	}

	s32 ret = write(STDIN_FILENO, show_cursor, sizeof(show_cursor) - 1);
	ret = write(STDIN_FILENO, enable_blank, sizeof(enable_blank) - 1);
	ret = write(STDIN_FILENO, clear_screen, sizeof(clear_screen) - 1);
}

void Screen::showInfo(bool verbose)
{
	if (!verbose) return;

	static const s8* const scrollstr[4] = {
		"redraw", "ypan", "ywrap", "xpan"
	};
	printf("[screen] driver: %s, mode: %dx%d-%dbpp, scrolling: %s\n",
		drvId(), mWidth, mHeight, mBitsPerPixel, scrollstr[mScrollType]);
}

void Screen::switchVc(bool enter)
{
	mOffsetCur = 0;
	setupOffset();

	setupPalette(!enter);
	if (enter && mPalette) eraseMargin(true, mRows);
}

bool Screen::move(u16 scol, u16 srow, u16 dcol, u16 drow, u16 w, u16 h)
{
	if (!mScrollEnable || mScrollType == Redraw || scol != dcol) return false;

	u16 top = MIN(srow, drow), bot = MAX(srow, drow) + h;
	u16 left = scol, right = scol + w;

	u32 noaccel_redraw_area = w * (bot - top - 1);
	u32 accel_redraw_area = mCols * mRows - w * h;

	if (noaccel_redraw_area <= accel_redraw_area) return false;

	if (mRotateType == Rotate0 || mRotateType == Rotate270) mOffsetCur += FH((s32)srow - drow);
	else mOffsetCur -= FH((s32)srow - drow);

	bool redraw_all = false;
	if (mScrollType == YPan || mScrollType == XPan) {
		redraw_all = true;

		if (mOffsetCur < 0) mOffsetCur = mOffsetMax;
		else if ((u32)mOffsetCur > mOffsetMax) mOffsetCur = 0;
		else redraw_all = false;
	} else {
		if (mOffsetCur < 0) mOffsetCur += mOffsetMax + 1;
		else if ((u32)mOffsetCur > mOffsetMax) mOffsetCur -= mOffsetMax + 1;
	}

	setupOffset();

	if (top) redraw(0, 0, mCols, top);
	if (bot < mRows) redraw(0, bot, mCols, mRows - bot);
	if (left > 0) redraw(0, top, left, bot - top - 1);
	if (right < mCols) redraw(right, top, mCols - right, bot - top - 1);

	if (redraw_all) {
		eraseMargin(true, mRows);
	} else {
		eraseMargin(drow > srow, drow > srow ? (drow - srow) : (srow - drow));
	}

	return !redraw_all;
}

void Screen::eraseMargin(bool top, u16 h)
{
	// Margins must always use palette black, never custom background
	u32 bg = mFillColors[0];

	if (mWidth % FW(1)) {
		fillRectPixel(FW(mCols), top ? 0 : FH(mRows - h), mWidth % FW(1), FH(h), bg);
	}

	if (mHeight % FH(1)) {
		fillRectPixel(0, FH(mRows), mWidth, mHeight % FH(1), bg);
	}
}

void Screen::drawText(u32 x, u32 y, u8 fc, u8 bc, bool direct_fg, bool direct_bg,
		      u16 num, u32 *text, bool *dw, bool bold, bool italic,
		      bool underline, bool strikethrough)
{
	RenderColor fg = resolveColor(fc, direct_fg);
	RenderColor bg = resolveColor(bc, direct_bg);

	u32 startx, fw = FW(1);

	u16 startnum; u32 *starttext;
	bool *startdw, draw_space = false, draw_text = false;

	for (; num; num--, text++, dw++, x += fw) {
		if (*text == 0x20) {
			if (draw_text) {
				draw_text = false;
				drawGlyphs(startx, y, fg, bg, startnum - num, starttext, startdw, bold, italic, underline, strikethrough);
			}

			if (!draw_space) {
				draw_space = true;
				startx = x;
			}
		} else {
			if (draw_space) {
				draw_space = false;
				fillRectPixel(startx, y, x - startx, FH(1), bg.pixel);
			}

			if (!draw_text) {
				draw_text = true;
				starttext = text;
				startdw = dw;
				startnum = num;
				startx = x;
			}

			if (*dw) x += fw;
		}
	}

	if (draw_text) {
		drawGlyphs(startx, y, fg, bg, startnum - num, starttext, startdw, bold, italic, underline, strikethrough);
	} else if (draw_space) {
		fillRectPixel(startx, y, x - startx, FH(1), bg.pixel);
	}
}

void Screen::drawGlyphs(u32 x, u32 y, const RenderColor& fg, const RenderColor& bg,
			u16 num, u32 *text, bool *dw, bool bold, bool italic,
			bool underline, bool strikethrough)
{
	for (; num--; text++, dw++) {
		drawGlyph(x, y, fg, bg, *text, *dw, bold, italic, underline, strikethrough);
		x += *dw ? FW(2) : FW(1);
	}
}

void Screen::adjustOffset(u32 &x, u32 &y)
{
	if (mScrollType == XPan) x += mOffsetCur;
	else y += mOffsetCur;
}

void Screen::fillRect(u32 x, u32 y, u32 w, u32 h, u8 color)
{
	u32 pixel = (color == 0 && mHasCustomBackground) ? mCustomBackgroundPixel : mFillColors[color];
	fillRectPixel(x, y, w, h, pixel);
}

void Screen::setBackgroundColor(const Color *color)
{
	if (color) {
		mHasCustomBackground = true;
		mCustomBackgroundColor = *color;

		if (mBitsPerPixel == 0) return;
		u8 r = color->red, g = color->green, b = color->blue;
		switch (mBitsPerPixel) {
		case 8:
			mCustomBackgroundPixel = ((r + g + b) / 3) & 0xff;
			mCustomBackgroundPixel |= mCustomBackgroundPixel << 8;
			mCustomBackgroundPixel |= mCustomBackgroundPixel << 16;
			break;
		case 15:
			mCustomBackgroundPixel = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
			mCustomBackgroundPixel |= mCustomBackgroundPixel << 16;
			break;
		case 16:
			mCustomBackgroundPixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
			mCustomBackgroundPixel |= mCustomBackgroundPixel << 16;
			break;
		default:
			mCustomBackgroundPixel = (r << 16) | (g << 8) | b;
			break;
		}
	} else {
		mHasCustomBackground = false;
	}
}

void Screen::fillRectPixel(u32 x, u32 y, u32 w, u32 h, u32 pixel)
{
	if (x >= mWidth || y >= mHeight || !w || !h) return;
	if (x + w > mWidth) w = mWidth - x;
	if (y + h > mHeight) h = mHeight - y;

	rotateRect(x, y, w, h);
	adjustOffset(x, y);

	for (; h--;) {
		if (mScrollType == YWrap && y > mOffsetMax) y -= mOffsetMax + 1;
		(this->*fill)(x, y++, w, pixel);
	}
}

void Screen::drawGlyph(u32 x, u32 y, const RenderColor& fg, const RenderColor& bg,
		       u32 code, bool dw, bool bold, bool italic,
		       bool underline, bool strikethrough)
{
	if (x >= mWidth || y >= mHeight) return;

	s32 w = (dw ? FW(2) : FW(1)), h = FH(1);
	u32 cellX = x, cellY = y, cellW = w, cellH = h;
	if (x + w > mWidth) { w = mWidth - x; cellW = w; }
	if (y + h > mHeight) { h = mHeight - y; cellH = h; }

	Font::Glyph *glyph = (Font::Glyph *)Font::instance()->getGlyph(code, bold, italic);
	if (!glyph) {
		fillRectPixel(x, y, w, h, bg.pixel);
	} else {
		s32 top = glyph->top;
	if (top < 0) top = 0;

	s32 left = glyph->left;
	if ((s32)x + left < 0) left = -x;

	s32 width = glyph->width;
	if (width > w - left) width = w - left;
	if ((s32)x + left + width > (s32)mWidth) width = mWidth - ((s32)x + left);
	if (width < 0) width = 0;

	s32 height = glyph->height;
	if (height > h - top) height = h - top;
	if (y + top + height > mHeight) height = mHeight - (y + top);
	if (height < 0) height = 0;

	if (top) fillRectPixel(x, y, w, top, bg.pixel);
	if (left > 0) fillRectPixel(x, y + top, left, height, bg.pixel);

	s32 right = width + left;
	if (w > right) fillRectPixel((s32)x + right, y + top, w - right, height, bg.pixel);

	s32 bot = top + height;
	if (h > bot) fillRectPixel(x, y + bot, w, h - bot, bg.pixel);

	x += left;
	y += top;
	if (x < mWidth && y < mHeight && width && height) {

	u32 nwidth = width, nheight = height;
	rotateRect(x, y, nwidth, nheight);

	u8 *pixmap = glyph->pixmap;
	u32 wdiff = glyph->width - width, hdiff = glyph->height - height;

	if (wdiff) {
		if (mRotateType == Rotate180) pixmap += wdiff;
		else if (mRotateType == Rotate270) pixmap += wdiff * glyph->pitch;
	}

	if (hdiff) {
		if (mRotateType == Rotate90) pixmap += hdiff;
		else if (mRotateType == Rotate180) pixmap += hdiff * glyph->pitch;
	}

	adjustOffset(x, y);
	for (; nheight--; y++, pixmap += glyph->pitch) {
		if ((mScrollType == YWrap) && y > mOffsetMax) y -= mOffsetMax + 1;
		(this->*draw)(x, y, nwidth, fg, bg, pixmap);
	}
	}
	}

	if (underline) {
		fillRectPixel(cellX, cellY + cellH - 2, cellW, 1, fg.pixel);
	}
	if (strikethrough) {
		fillRectPixel(cellX, cellY + cellH / 2, cellW, 1, fg.pixel);
	}
}

void Screen::rotateRect(u32 &x, u32 &y, u32 &w, u32 &h)
{
	u32 tmp;
	switch (mRotateType) {
	case Rotate0:
		break;

	case Rotate90:
		tmp = x;
		x = mHeight - y - h;
		y = tmp;

		tmp = w;
		w = h;
		h = tmp;
		break;

	case Rotate180:
		x = mWidth - x - w;
		y = mHeight - y - h;
		break;

	case Rotate270:
		tmp = y;
		y = mWidth - x - w;
		x = tmp;

		tmp = w;
		w = h;
		h = tmp;
		break;
	}
}

void Screen::rotatePoint(u32 W, u32 H, u32 &x, u32 &y)
{
	u32 tmp;
	switch (mRotateType) {
	case Rotate0:
		break;

	case Rotate90:
		tmp = x;
		x = H - y - 1;
		y = tmp;
		break;

	case Rotate180:
		x = W - x - 1;
		y = H - y - 1;
		break;

	case Rotate270:
		tmp = y;
		y = W - x - 1;
		x = tmp;
		break;
	}
}

RenderColor Screen::resolveColor(u8 index, bool direct) const
{
	RenderColor rc;
	if (direct && mDirectColorTable) {
		const Color *rgb = &mDirectColorTable[index];
		rc.red = rgb->red;
		rc.green = rgb->green;
		rc.blue = rgb->blue;
		rc.index = 255;
		switch (mBitsPerPixel) {
		case 8:
			rc.pixel = (index << 24) | (index << 16) | (index << 8) | index;
			break;
		case 15:
			rc.pixel = ((rc.red >> 3) << 10) | ((rc.green >> 3) << 5) | (rc.blue >> 3);
			rc.pixel |= rc.pixel << 16;
			break;
		case 16:
			rc.pixel = ((rc.red >> 3) << 11) | ((rc.green >> 2) << 5) | (rc.blue >> 3);
			rc.pixel |= rc.pixel << 16;
			break;
		default:
			rc.pixel = (rc.red << 16) | (rc.green << 8) | rc.blue;
			break;
		}
	} else {
		rc.pixel = mFillColors[index];
		const Color *rgb2 = &mPalette[index];
		rc.red = rgb2->red;
		rc.green = rgb2->green;
		rc.blue = rgb2->blue;
		rc.index = index;
	}
	return rc;
}

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, u32)
#endif

void Screen::initDoubleBuffer()
{
	if (mBackBuffer) return;
	// mFbMemSize is set by FbDev to finfo.smem_len (total fb allocation,
	// which may be larger than visible area for YPan/YWrap scrolling)
	if (!mFbMemSize) return;

	mRealFbBase = mVMemBase;
	mBackBuffer = new u8[mFbMemSize];
	memcpy(mBackBuffer, mRealFbBase, mFbMemSize);
	mVMemBase = mBackBuffer;
}

void Screen::swapBuffers()
{
	if (!mBackBuffer) return;

	memcpy(mRealFbBase, mBackBuffer, mFbMemSize);
}

void Screen::waitVsync()
{
	if (mFbFd < 0) return;
	u32 crt = 0;
	ioctl(mFbFd, FBIO_WAITFORVSYNC, &crt);
}
