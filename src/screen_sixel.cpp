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

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

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

	adjustOffset(x, y);

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
			u32 w = draw_w;
			if (py + 2 < draw_h)
				__builtin_prefetch(mVMemBase + (y + py + 2) * mBytesPerLine + x, 1, 3);
#ifdef __ARM_NEON__
			{
				u32 nw = w & ~7;
				uint8x8_t bg_v = vdup_n_u8(bg8);
				uint8x8_t zero = vdup_n_u8(0);
				for (u32 i = 0; i < nw; i += 8, dst += 8, src += 32) {
					uint8x8x4_t bgra = vld4_u8(src);
					uint8x8_t any = vorr_u8(
						vorr_u8(bgra.val[0], bgra.val[1]),
						vorr_u8(bgra.val[2], bgra.val[3]));
					uint8x8_t transp = vceq_u8(any, zero);
					uint16x8_t gray = vmull_u8(bgra.val[2], vdup_n_u8(77));
					gray = vmlal_u8(gray, bgra.val[1], vdup_n_u8(150));
					gray = vmlal_u8(gray, bgra.val[0], vdup_n_u8(29));
					uint8x8_t gray8 = vshrn_n_u16(gray, 8);
					uint8x8_t result = vbsl_u8(transp, bg_v, gray8);
					vst1_u8(dst, result);
				}
				w -= nw;
			}
#endif
			for (; w--; dst++, src += 4) {
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
			u32 w = draw_w;
			if (py + 2 < draw_h)
				__builtin_prefetch((u16 *)(mVMemBase + (y + py + 2) * mBytesPerLine) + x, 1, 3);
#ifdef __ARM_NEON__
			{
				u32 nw = w & ~7;
				uint16x4_t bg_lo = vdup_n_u16(bg16);
				uint16x4_t bg_hi = vdup_n_u16(bg16);
				uint8x8_t zero = vdup_n_u8(0);
				for (u32 i = 0; i < nw; i += 8, dst += 8, src += 32) {
					uint8x8x4_t bgra = vld4_u8(src);
					uint8x8_t any = vorr_u8(
						vorr_u8(bgra.val[0], bgra.val[1]),
						vorr_u8(bgra.val[2], bgra.val[3]));
					uint8x8_t transp = vceq_u8(any, zero);
					uint16x8_t transp16 = vmovl_u8(transp);
					uint16x4_t mask_lo = vmul_n_u16(vget_low_u16(transp16), 0x0101);
					uint16x4_t mask_hi = vmul_n_u16(vget_high_u16(transp16), 0x0101);

					uint8x8_t r5 = vshr_n_u8(bgra.val[2], 3);
					uint8x8_t g5 = vshr_n_u8(bgra.val[1], 3);
					uint8x8_t b5 = vshr_n_u8(bgra.val[0], 3);

					uint16x8_t packed = vorrq_u16(
						vorrq_u16(vshlq_n_u16(vmovl_u8(r5), 10), vshlq_n_u16(vmovl_u8(g5), 5)),
						vmovl_u8(b5));

					uint16x4_t p_lo = vbsl_u16(mask_lo, bg_lo, vget_low_u16(packed));
					uint16x4_t p_hi = vbsl_u16(mask_hi, bg_hi, vget_high_u16(packed));
					vst1_u16(dst, p_lo);
					vst1_u16(dst + 4, p_hi);
				}
				w -= nw;
			}
#endif
			for (; w--; dst++, src += 4) {
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
			u32 w = draw_w;
			if (py + 2 < draw_h)
				__builtin_prefetch((u16 *)(mVMemBase + (y + py + 2) * mBytesPerLine) + x, 1, 3);
#ifdef __ARM_NEON__
			{
				u32 nw = w & ~7;
				uint16x4_t bg_lo = vdup_n_u16(bg16);
				uint16x4_t bg_hi = vdup_n_u16(bg16);
				uint8x8_t zero = vdup_n_u8(0);
				for (u32 i = 0; i < nw; i += 8, dst += 8, src += 32) {
					uint8x8x4_t bgra = vld4_u8(src);
					uint8x8_t any = vorr_u8(
						vorr_u8(bgra.val[0], bgra.val[1]),
						vorr_u8(bgra.val[2], bgra.val[3]));
					uint8x8_t transp = vceq_u8(any, zero);
					uint16x8_t transp16 = vmovl_u8(transp);
					uint16x4_t mask_lo = vmul_n_u16(vget_low_u16(transp16), 0x0101);
					uint16x4_t mask_hi = vmul_n_u16(vget_high_u16(transp16), 0x0101);

					uint8x8_t r5 = vshr_n_u8(bgra.val[2], 3);
					uint8x8_t g6 = vshr_n_u8(bgra.val[1], 2);
					uint8x8_t b5 = vshr_n_u8(bgra.val[0], 3);

					uint16x8_t packed = vorrq_u16(
						vorrq_u16(vshlq_n_u16(vmovl_u8(r5), 11), vshlq_n_u16(vmovl_u8(g6), 5)),
						vmovl_u8(b5));

					uint16x4_t p_lo = vbsl_u16(mask_lo, bg_lo, vget_low_u16(packed));
					uint16x4_t p_hi = vbsl_u16(mask_hi, bg_hi, vget_high_u16(packed));
					vst1_u16(dst, p_lo);
					vst1_u16(dst + 4, p_hi);
				}
				w -= nw;
			}
#endif
			for (; w--; dst++, src += 4) {
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
			u32 w = draw_w;
			if (py + 2 < draw_h)
				__builtin_prefetch((u32 *)(mVMemBase + (y + py + 2) * mBytesPerLine) + x, 1, 3);
#ifdef __ARM_NEON__
			{
				u32 nw = w & ~7;
				uint32x4_t bg_v = vdupq_n_u32(bg_pixel);
				uint8x8_t zero = vdup_n_u8(0);
				for (u32 i = 0; i < nw; i += 8, dst += 8, src += 32) {
					uint8x8x4_t bgra = vld4_u8(src);
					uint8x8_t any = vorr_u8(
						vorr_u8(bgra.val[0], bgra.val[1]),
						vorr_u8(bgra.val[2], bgra.val[3]));
					uint8x8_t transp = vceq_u8(any, zero);

					uint16x8_t r16 = vmovl_u8(bgra.val[2]);
					uint16x8_t g16 = vmovl_u8(bgra.val[1]);
					uint16x8_t b16 = vmovl_u8(bgra.val[0]);

					uint32x4_t r_lo = vmovl_u16(vget_low_u16(r16));
					uint32x4_t g_lo = vmovl_u16(vget_low_u16(g16));
					uint32x4_t b_lo = vmovl_u16(vget_low_u16(b16));
					uint32x4_t rgb_lo = vorrq_u32(
						vorrq_u32(vshlq_n_u32(r_lo, 16), vshlq_n_u32(g_lo, 8)),
						b_lo);

					uint32x4_t r_hi = vmovl_u16(vget_high_u16(r16));
					uint32x4_t g_hi = vmovl_u16(vget_high_u16(g16));
					uint32x4_t b_hi = vmovl_u16(vget_high_u16(b16));
					uint32x4_t rgb_hi = vorrq_u32(
						vorrq_u32(vshlq_n_u32(r_hi, 16), vshlq_n_u32(g_hi, 8)),
						b_hi);

					uint16x8_t transp16 = vmovl_u8(transp);
					uint32x4_t mask_lo = vmulq_n_u32(
						vmovl_u16(vget_low_u16(transp16)), 0x01010101);
					uint32x4_t mask_hi = vmulq_n_u32(
						vmovl_u16(vget_high_u16(transp16)), 0x01010101);

					uint32x4_t r_lo_b = vbslq_u32(mask_lo, bg_v, rgb_lo);
					uint32x4_t r_hi_b = vbslq_u32(mask_hi, bg_v, rgb_hi);

					vst1q_u32(dst, r_lo_b);
					vst1q_u32(dst + 4, r_hi_b);
				}
				w -= nw;
			}
#endif
			for (; w--; dst++, src += 4) {
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
