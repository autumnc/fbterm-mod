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

#include <stdlib.h>
#include <string.h>
#include "screen.h"
#include "fbconfig.h"

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

#define writeb(addr, val) (*(volatile u8 *)(addr) = (val))
#define writew(addr, val) (*(volatile u16 *)(addr) = (val))
#define writel(addr, val) (*(volatile u32 *)(addr) = (val))

static u32 bytes_per_pixel;
static u32 ppl, ppw, ppb;

static u8 *bgimage_mem;
static u8 bgcolor;

void Screen::setPalette(const Color *palette)
{
	if (mPalette == palette) return;
	mPalette = palette;

	for (u32 i = 0; i < NR_COLORS; i++) {
		switch (mBitsPerPixel) {
		case 8:
			mFillColors[i] = (i << 24) | (i << 16) | (i << 8) | i;
			break;
		case 15:
			mFillColors[i] = ((palette[i].red >> 3) << 10) | ((palette[i].green >> 3) << 5) | (palette[i].blue >> 3);
			mFillColors[i] |= mFillColors[i] << 16;
			break;
		case 16:
			mFillColors[i] = ((palette[i].red >> 3) << 11) | ((palette[i].green >> 2) << 5) | (palette[i].blue >> 3);
			mFillColors[i] |= mFillColors[i] << 16;
			break;
		case 32:
			mFillColors[i] = (palette[i].red << 16) | (palette[i].green << 8) | palette[i].blue;
			break;
		}
	}


	if (mHasCustomBackground) {
		u8 r = mCustomBackgroundColor.red;
		u8 g = mCustomBackgroundColor.green;
		u8 b = mCustomBackgroundColor.blue;
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
	}
	setupPalette(false);
	eraseMargin(true, mRows);
}

void Screen::initFillDraw()
{
	if (mBitsPerPixel == 15) bytes_per_pixel = 2;
	else bytes_per_pixel = (mBitsPerPixel >> 3);

	ppl = 4 / bytes_per_pixel;
	ppw = ppl >> 1;
	ppb = ppl >> 2;

	bool bg = false;
	if (getenv("FBTERM_BACKGROUND_IMAGE")) {
		bg = true;
		mScrollType = Redraw;

		u32 color = 0;
		Config::instance()->getOption("color-background", color);
		if (color > 7) color = 0;
		bgcolor = color;

		u32 size = mBytesPerLine * ((mRotateType == Rotate0 || mRotateType == Rotate180) ? mHeight : mWidth);
		bgimage_mem = new u8[size];
		memcpy(bgimage_mem, mVMemBase, size);
	}

	fill = bg ? &Screen::fillXBg : &Screen::fillX;

	switch (mBitsPerPixel) {
	case 8:
		draw = bg ? &Screen::draw8Bg : &Screen::draw8;
		break;
	case 15:
		draw = bg ? &Screen::draw15Bg : &Screen::draw15;
		break;
	case 16:
		draw = bg ? &Screen::draw16Bg : &Screen::draw16;
		break;
	case 32:
		draw = bg ? &Screen::draw32Bg : &Screen::draw32;
		break;
	}
}

void Screen::endFillDraw()
{
	if (bgimage_mem) delete[] bgimage_mem;
}

void Screen::fillX(u32 x, u32 y, u32 w, u32 pixel)
{
	u8 *dst = mVMemBase + y * mBytesPerLine + x * bytes_per_pixel;

#ifdef __ARM_NEON__
	/* NEON bulk fill: 16 bytes per store, block-aligned */
	if (w >= 16 / bytes_per_pixel) {
		u32 ncount;
		u8 *nd = dst;

		if (bytes_per_pixel == 4) {
			ncount = w & ~3;
			uint32x4_t vp = vdupq_n_u32(pixel);
			for (u32 i = ncount; i; i -= 4, nd += 16)
				vst1q_u32((u32 *)nd, vp);
		} else if (bytes_per_pixel == 2) {
			ncount = w & ~7;
			uint16x8_t vp = vdupq_n_u16((u16)pixel);
			for (u32 i = ncount; i; i -= 8, nd += 16)
				vst1q_u16((u16 *)nd, vp);
		} else {
			ncount = w & ~15;
			uint8x16_t vp = vdupq_n_u8((u8)pixel);
			for (u32 i = ncount; i; i -= 16, nd += 16)
				vst1q_u8(nd, vp);
		}

		dst = nd;
		w -= ncount;
	}
#endif

	// get better performance if write-combining not enabled for video memory
	for (u32 i = w / ppl; i--; dst += 4) {
		writel(dst, pixel);
	}

	if (w & ppw) {
		writew(dst, pixel);
		dst += 2;
	}

	if (w & ppb) {
		writeb(dst, pixel);
	}
}

void Screen::fillXBg(u32 x, u32 y, u32 w, u32 pixel)
{
	u32 offset = y * mBytesPerLine + x * bytes_per_pixel;
	if (mFillColors[bgcolor] == pixel) {
		memcpy(mVMemBase + offset, bgimage_mem + offset, w * bytes_per_pixel);
	} else {
		fillX(x, y, w, pixel);
	}
}

void Screen::draw8(u32 x, u32 y, u32 w, const RenderColor& fg, const RenderColor& bg, u8 *pixmap)
{
	u8 *dst = mVMemBase + y * mBytesPerLine + x * bytes_per_pixel;
	u32 fg8 = (u8)(fg.pixel & 0xff);
	u32 bg8 = (u8)(bg.pixel & 0xff);

	/* process 4 pixels per iteration: read u32, extract 4 bytes */
	for (; w >= 4; w -= 4, pixmap += 4, dst += 4) {
		u32 pix4;
		memcpy(&pix4, pixmap, 4);

		writeb(dst + 0, (pix4 & 0x80) ? fg8 : bg8);
		writeb(dst + 1, (pix4 & 0x8000) ? fg8 : bg8);
		writeb(dst + 2, (pix4 & 0x800000) ? fg8 : bg8);
		writeb(dst + 3, (pix4 & 0x80000000) ? fg8 : bg8);

		if (w >= 8) __builtin_prefetch(pixmap + 8, 0, 3);
	}

	for (; w--; pixmap++, dst++) {
		writeb(dst, (*pixmap & 0x80) ? fg8 : bg8);
	}
}

void Screen::draw8Bg(u32 x, u32 y, u32 w, const RenderColor& fg, const RenderColor& bg, u8 *pixmap)
{
	if (bg.index != bgcolor) {
		draw8(x, y, w, fg, bg, pixmap);
		return;
	}

	u32 offset = y * mBytesPerLine + x * bytes_per_pixel;
	u8 *dst = mVMemBase + offset;
	u8 *bgimg = bgimage_mem + offset;
	u32 fg8 = (u8)(fg.pixel & 0xff);

	for (; w >= 4; w -= 4, pixmap += 4, dst += 4, bgimg += 4) {
		u32 pix4;
		memcpy(&pix4, pixmap, 4);

		writeb(dst + 0, (pix4 & 0x80) ? fg8 : bgimg[0]);
		writeb(dst + 1, (pix4 & 0x8000) ? fg8 : bgimg[1]);
		writeb(dst + 2, (pix4 & 0x800000) ? fg8 : bgimg[2]);
		writeb(dst + 3, (pix4 & 0x80000000) ? fg8 : bgimg[3]);

		if (w >= 8) __builtin_prefetch(pixmap + 8, 0, 3);
	}

	for (; w--; pixmap++, dst++, bgimg++) {
		writeb(dst, (*pixmap & 0x80) ? fg8 : (*bgimg));
	}
}

	#define drawX(bits, lred, lgreen, lblue, type, fbwrite) \
	 \
	void Screen::draw##bits(u32 x, u32 y, u32 w, const RenderColor& fg, const RenderColor& bg, u8 *pixmap) \
	{ \
		type *dst = (type *)(mVMemBase + y * mBytesPerLine + x * bytes_per_pixel); \
		const type fgp = (type)fg.pixel; \
		const type bgp = (type)bg.pixel; \
		const u32 fr = fg.red, fgr_ = fg.green, fb_ = fg.blue; \
		const u32 br = bg.red, bg_ = bg.green, bb = bg.blue; \
		const s32 dr = (s32)fr - (s32)br; \
		const s32 dg = (s32)fgr_ - (s32)bg_; \
		const s32 db = (s32)fb_ - (s32)bb; \
	 \
		u32 pix4, px, r_, g_, b_; \
		type c; \
	 \
		for (; w >= 4; w -= 4, pixmap += 4, dst += 4) { \
			memcpy(&pix4, pixmap, 4); \
	 \
			px = pix4 & 0xff; \
			if (!px) { fbwrite(dst + 0, bgp); } \
			else if (px == 0xff) { fbwrite(dst + 0, fgp); } \
			else { \
				r_ = br + ((dr * (s32)px) >> 8); \
				g_ = bg_ + ((dg * (s32)px) >> 8); \
				b_ = bb + ((db * (s32)px) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst + 0, c); \
			} \
	 \
			px = (pix4 >> 8) & 0xff; \
			if (!px) { fbwrite(dst + 1, bgp); } \
			else if (px == 0xff) { fbwrite(dst + 1, fgp); } \
			else { \
				r_ = br + ((dr * (s32)px) >> 8); \
				g_ = bg_ + ((dg * (s32)px) >> 8); \
				b_ = bb + ((db * (s32)px) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst + 1, c); \
			} \
	 \
			px = (pix4 >> 16) & 0xff; \
			if (!px) { fbwrite(dst + 2, bgp); } \
			else if (px == 0xff) { fbwrite(dst + 2, fgp); } \
			else { \
				r_ = br + ((dr * (s32)px) >> 8); \
				g_ = bg_ + ((dg * (s32)px) >> 8); \
				b_ = bb + ((db * (s32)px) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst + 2, c); \
			} \
	 \
			px = pix4 >> 24; \
			if (!px) { fbwrite(dst + 3, bgp); } \
			else if (px == 0xff) { fbwrite(dst + 3, fgp); } \
			else { \
				r_ = br + ((dr * (s32)px) >> 8); \
				g_ = bg_ + ((dg * (s32)px) >> 8); \
				b_ = bb + ((db * (s32)px) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst + 3, c); \
			} \
	 \
			if (w >= 8) __builtin_prefetch(pixmap + 8, 0, 3); \
		} \
	 \
		u8 pixel; \
		for (; w--; pixmap++, dst++) { \
			pixel = *pixmap; \
			if (!pixel) fbwrite(dst, bgp); \
			else if (pixel == 0xff) fbwrite(dst, fgp); \
			else { \
				r_ = br + ((dr * pixel) >> 8); \
				g_ = bg_ + ((dg * pixel) >> 8); \
				b_ = bb + ((db * pixel) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst, c); \
			} \
		} \
	}
\
	drawX(15, 5, 5, 5, u16, writew)
	drawX(16, 5, 6, 5, u16, writew)
	drawX(32, 8, 8, 8, u32, writel)

	#define drawXBg(bits, lred, lgreen, lblue, type, fbwrite) \
	 \
	void Screen::draw##bits##Bg(u32 x, u32 y, u32 w, const RenderColor& fg, const RenderColor& bg, u8 *pixmap) \
	{ \
		if (bg.index != bgcolor) { \
			draw##bits(x, y, w, fg, bg, pixmap); \
			return; \
		} \
	 \
		u32 offset = y * mBytesPerLine + x * bytes_per_pixel; \
		type *dst = (type *)(mVMemBase + offset); \
		type *bgimg = (type *)(bgimage_mem + offset); \
		const type fgp = (type)fg.pixel; \
		const u32 fr = fg.red, fgr_ = fg.green, fb_ = fg.blue; \
	 \
		u32 pix4, px, r_, g_, b_, rbg, gbg, bbg; \
		type c, bgi; \
	 \
		for (; w >= 4; w -= 4, pixmap += 4, dst += 4, bgimg += 4) { \
			memcpy(&pix4, pixmap, 4); \
	 \
			px = pix4 & 0xff; \
			if (!px) { fbwrite(dst + 0, bgimg[0]); } \
			else if (px == 0xff) { fbwrite(dst + 0, fgp); } \
			else { \
				bgi = bgimg[0]; \
				rbg = ((bgi >> (lgreen + lblue)) & ((1 << lred) - 1)) << (8 - lred); \
				gbg = ((bgi >> lblue) & ((1 << lgreen) - 1)) << (8 - lgreen); \
				bbg = (bgi & ((1 << lblue) - 1)) << (8 - lblue); \
				r_ = rbg + (((fr - rbg) * (s32)px) >> 8); \
				g_ = gbg + (((fgr_ - gbg) * (s32)px) >> 8); \
				b_ = bbg + (((fb_ - bbg) * (s32)px) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst + 0, c); \
			} \
	 \
			px = (pix4 >> 8) & 0xff; \
			if (!px) { fbwrite(dst + 1, bgimg[1]); } \
			else if (px == 0xff) { fbwrite(dst + 1, fgp); } \
			else { \
				bgi = bgimg[1]; \
				rbg = ((bgi >> (lgreen + lblue)) & ((1 << lred) - 1)) << (8 - lred); \
				gbg = ((bgi >> lblue) & ((1 << lgreen) - 1)) << (8 - lgreen); \
				bbg = (bgi & ((1 << lblue) - 1)) << (8 - lblue); \
				r_ = rbg + (((fr - rbg) * (s32)px) >> 8); \
				g_ = gbg + (((fgr_ - gbg) * (s32)px) >> 8); \
				b_ = bbg + (((fb_ - bbg) * (s32)px) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst + 1, c); \
			} \
	 \
			px = (pix4 >> 16) & 0xff; \
			if (!px) { fbwrite(dst + 2, bgimg[2]); } \
			else if (px == 0xff) { fbwrite(dst + 2, fgp); } \
			else { \
				bgi = bgimg[2]; \
				rbg = ((bgi >> (lgreen + lblue)) & ((1 << lred) - 1)) << (8 - lred); \
				gbg = ((bgi >> lblue) & ((1 << lgreen) - 1)) << (8 - lgreen); \
				bbg = (bgi & ((1 << lblue) - 1)) << (8 - lblue); \
				r_ = rbg + (((fr - rbg) * (s32)px) >> 8); \
				g_ = gbg + (((fgr_ - gbg) * (s32)px) >> 8); \
				b_ = bbg + (((fb_ - bbg) * (s32)px) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst + 2, c); \
			} \
	 \
			px = pix4 >> 24; \
			if (!px) { fbwrite(dst + 3, bgimg[3]); } \
			else if (px == 0xff) { fbwrite(dst + 3, fgp); } \
			else { \
				bgi = bgimg[3]; \
				rbg = ((bgi >> (lgreen + lblue)) & ((1 << lred) - 1)) << (8 - lred); \
				gbg = ((bgi >> lblue) & ((1 << lgreen) - 1)) << (8 - lgreen); \
				bbg = (bgi & ((1 << lblue) - 1)) << (8 - lblue); \
				r_ = rbg + (((fr - rbg) * (s32)px) >> 8); \
				g_ = gbg + (((fgr_ - gbg) * (s32)px) >> 8); \
				b_ = bbg + (((fb_ - bbg) * (s32)px) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst + 3, c); \
			} \
	 \
			if (w >= 8) __builtin_prefetch(pixmap + 8, 0, 3); \
		} \
	 \
		u8 pixel; \
		for (; w--; pixmap++, dst++, bgimg++) { \
			pixel = *pixmap; \
			if (!pixel) fbwrite(dst, *bgimg); \
			else if (pixel == 0xff) fbwrite(dst, fgp); \
			else { \
				bgi = *bgimg; \
				rbg = ((bgi >> (lgreen + lblue)) & ((1 << lred) - 1)) << (8 - lred); \
				gbg = ((bgi >> lblue) & ((1 << lgreen) - 1)) << (8 - lgreen); \
				bbg = (bgi & ((1 << lblue) - 1)) << (8 - lblue); \
				r_ = rbg + (((fr - rbg) * pixel) >> 8); \
				g_ = gbg + (((fgr_ - gbg) * pixel) >> 8); \
				b_ = bbg + (((fb_ - bbg) * pixel) >> 8); \
				c = (type)(((r_ >> (8 - lred)) << (lgreen + lblue)) | ((g_ >> (8 - lgreen)) << lblue) | (b_ >> (8 - lblue))); \
				fbwrite(dst, c); \
			} \
		} \
	}
\
	drawXBg(15, 5, 5, 5, u16, writew)
	drawXBg(16, 5, 6, 5, u16, writew)
	drawXBg(32, 8, 8, 8, u32, writel)
