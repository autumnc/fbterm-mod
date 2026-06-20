/*
 *   Sixel rendering for FbTerm
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>
#include "screen.h"
#include "font.h"

#define writeb(addr, val) (*(volatile u8 *)(addr) = (val))
#define writew(addr, val) (*(volatile u16 *)(addr) = (val))
#define writel(addr, val) (*(volatile u32 *)(addr) = (val))

static inline u32 load_u32le(const u8 *p)
{
	u32 v;
	memcpy(&v, p, 4);
	return v;
}

/* Dispatch bpp once outside loops; each case is a specialized tight loop. */
void Screen::drawSixelCell(u32 x, u32 y, const u8 *pixmap, u32 bg_pixel)
{
	if (!pixmap || !mVMemBase) return;

	u32 cell_w = FW(1);
	u32 cell_h = FH(1);

	u32 draw_w = cell_w, draw_h = cell_h;
	if (x + draw_w > mWidth)  draw_w = mWidth - x;
	if (y + draw_h > mHeight) draw_h = mHeight - y;
	if (!draw_w || !draw_h) return;

	u32 row_skip = (cell_w - draw_w) * 4;
	const u8 *src = pixmap;

	/* prefetch first two rows of source */
	__builtin_prefetch(src, 0, 3);
	if (cell_w * 4 < 256)
		__builtin_prefetch(src + cell_w * 4, 0, 3);

	switch (mBitsPerPixel) {
	case 8: {
		u8 bg8 = (u8)bg_pixel;
		for (u32 py = 0; py < draw_h; py++) {
			u8 *dst = mVMemBase + (y + py) * mBytesPerLine + x;
			u8 *end = dst + draw_w;
			/* prefetch destination 2 rows ahead */
			if (py + 2 < draw_h)
				__builtin_prefetch(mVMemBase + (y + py + 2) * mBytesPerLine + x, 1, 3);
			for (; dst < end; dst++, src += 4) {
				u32 p = load_u32le(src);
				if (p == 0) { *dst = bg8; continue; }
				u8 r = (p >> 16) & 0xFF;
				u8 g = (p >> 8) & 0xFF;
				u8 b = p & 0xFF;
				*dst = (r * 77 + g * 150 + b * 29) >> 8;
			}
			src += row_skip;
		}
		break;
	}
	case 15: {
		u16 bg16 = (u16)bg_pixel;
		for (u32 py = 0; py < draw_h; py++) {
			u16 *dst = (u16 *)(mVMemBase + (y + py) * mBytesPerLine + x * 2);
			u16 *end = dst + draw_w;
			if (py + 2 < draw_h)
				__builtin_prefetch((u16 *)(mVMemBase + (y + py + 2) * mBytesPerLine) + x, 1, 3);
			for (; dst < end; dst++, src += 4) {
				u32 p = load_u32le(src);
				if (p == 0) { *dst = bg16; continue; }
				u8 r = (p >> 16) & 0xFF;
				u8 g = (p >> 8) & 0xFF;
				u8 b = p & 0xFF;
				*dst = (u16)(((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
			}
			src += row_skip;
		}
		break;
	}
	case 16: {
		u16 bg16 = (u16)bg_pixel;
		for (u32 py = 0; py < draw_h; py++) {
			u16 *dst = (u16 *)(mVMemBase + (y + py) * mBytesPerLine + x * 2);
			u16 *end = dst + draw_w;
			if (py + 2 < draw_h)
				__builtin_prefetch((u16 *)(mVMemBase + (y + py + 2) * mBytesPerLine) + x, 1, 3);
			for (; dst < end; dst++, src += 4) {
				u32 p = load_u32le(src);
				if (p == 0) { *dst = bg16; continue; }
				u8 r = (p >> 16) & 0xFF;
				u8 g = (p >> 8) & 0xFF;
				u8 b = p & 0xFF;
				*dst = (u16)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
			}
			src += row_skip;
		}
		break;
	}
	case 32:
	default: {
		for (u32 py = 0; py < draw_h; py++) {
			u32 *dst = (u32 *)(mVMemBase + (y + py) * mBytesPerLine + x * 4);
			u32 *end = dst + draw_w;
			if (py + 2 < draw_h)
				__builtin_prefetch((u32 *)(mVMemBase + (y + py + 2) * mBytesPerLine) + x, 1, 3);
			for (; dst < end; dst++, src += 4) {
				u32 p = load_u32le(src);
				if (p == 0) { *dst = bg_pixel; continue; }
				u8 r = (p >> 16) & 0xFF;
				u8 g = (p >> 8) & 0xFF;
				u8 b = p & 0xFF;
				*dst = (r << 16) | (g << 8) | b;
			}
			src += row_skip;
		}
		break;
	}
	}
}
