/*
 *   Sixel rendering for FbTerm
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 */

#include <stdlib.h>
#include "screen.h"
#include "font.h"

#define writeb(addr, val) (*(volatile u8 *)(addr) = (val))
#define writew(addr, val) (*(volatile u16 *)(addr) = (val))
#define writel(addr, val) (*(volatile u32 *)(addr) = (val))

void Screen::drawSixelCell(u32 x, u32 y, const u8 *pixmap, u32 bg_pixel)
{
	if (!pixmap || !mVMemBase) return;

	u32 cell_w = FW(1);
	u32 cell_h = FH(1);
	u32 bytes_per_pixel = (mBitsPerPixel == 15) ? 2 : (mBitsPerPixel >> 3);

	for (u32 py = 0; py < cell_h; py++) {
		u32 abs_y = y + py;
		if (abs_y >= mHeight) break;

		for (u32 px = 0; px < cell_w; px++) {
			u32 abs_x = x + px;
			if (abs_x >= mWidth) break;

			// Read 32-bit BGRA pixel from pixmap
			u32 pixel_val = *(const u32 *)(pixmap + (py * cell_w + px) * 4);

			// Transparent pixel (A=0): use background
			if (pixel_val == 0) {
				u32 offset = abs_y * mBytesPerLine + abs_x * bytes_per_pixel;
				u8 *dst = mVMemBase + offset;
				switch (bytes_per_pixel) {
				case 1: writeb(dst, bg_pixel); break;
				case 2: writew(dst, (u16)bg_pixel); break;
				case 4: writel(dst, bg_pixel); break;
				}
				continue;
			}

			u8 r = (pixel_val >> 16) & 0xFF;
			u8 g = (pixel_val >> 8) & 0xFF;
			u8 b = pixel_val & 0xFF;

			u32 offset = abs_y * mBytesPerLine + abs_x * bytes_per_pixel;
			u8 *dst = mVMemBase + offset;

			switch (mBitsPerPixel) {
			case 8: {
				u8 gray = (r * 77 + g * 150 + b * 29) >> 8;
				writeb(dst, gray);
				break;
			}
			case 15: {
				u16 pixel = ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3);
				writew(dst, pixel);
				break;
			}
			case 16: {
				u16 pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
				writew(dst, pixel);
				break;
			}
			case 32:
			default: {
				u32 pixel = (r << 16) | (g << 8) | b;
				writel(dst, pixel);
				break;
			}
			}
		}
	}
}
