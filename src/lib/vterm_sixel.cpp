/*
 *   Sixel support for FbTerm
 *   based on sixel implementation from yaft
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "vterm.h"

// #define SIXEL_DEBUG 1

#ifdef SIXEL_DEBUG
static FILE *dbg = 0;
#define DBG(fmt, ...) do { \
    if (!dbg) dbg = fopen("/tmp/sixel-debug.log", "a"); \
    if (dbg) { fprintf(dbg, fmt, ##__VA_ARGS__); fflush(dbg); } \
} while(0)
#else
#define DBG(fmt, ...) do {} while(0)
#endif

// VT340 default 16-color palette (RGB 0-255)
static const u8 vt340_palette[16][3] = {
	{  0,   0,   0}, // 0: black
	{  0,   0, 205}, // 1: blue
	{  0, 205,   0}, // 2: green
	{  0, 205, 205}, // 3: cyan
	{205,   0,   0}, // 4: red
	{205,   0, 205}, // 5: magenta
	{205, 205,   0}, // 6: yellow
	{205, 205, 205}, // 7: white (50%)
	{  0,   0, 238}, // 8: bright black
	{  0,   0, 255}, // 9: bright blue
	{  0, 238,   0}, // 10: bright green
	{  0, 255,   0}, // 11: ... (VT340 extension)
	{238,   0,   0}, // 12: bright red
	{238,   0, 255}, // 13: bright magenta
	{238, 238,   0}, // 14: bright yellow
	{238, 238, 255}, // 15: bright white
};

// xterm 256-color palette entries 16-255 (same as defaultPalette in fbshell.cpp)
static const u8 xterm_palette[240][3] = {
	{0x00, 0x00, 0x00}, {0x00, 0x00, 0x5f}, {0x00, 0x00, 0x87}, {0x00, 0x00, 0xaf},
	{0x00, 0x00, 0xd7}, {0x00, 0x00, 0xff}, {0x00, 0x5f, 0x00}, {0x00, 0x5f, 0x5f},
	{0x00, 0x5f, 0x87}, {0x00, 0x5f, 0xaf}, {0x00, 0x5f, 0xd7}, {0x00, 0x5f, 0xff},
	{0x00, 0x87, 0x00}, {0x00, 0x87, 0x5f}, {0x00, 0x87, 0x87}, {0x00, 0x87, 0xaf},
	{0x00, 0x87, 0xd7}, {0x00, 0x87, 0xff}, {0x00, 0xaf, 0x00}, {0x00, 0xaf, 0x5f},
	{0x00, 0xaf, 0x87}, {0x00, 0xaf, 0xaf}, {0x00, 0xaf, 0xd7}, {0x00, 0xaf, 0xff},
	{0x00, 0xd7, 0x00}, {0x00, 0xd7, 0x5f}, {0x00, 0xd7, 0x87}, {0x00, 0xd7, 0xaf},
	{0x00, 0xd7, 0xd7}, {0x00, 0xd7, 0xff}, {0x00, 0xff, 0x00}, {0x00, 0xff, 0x5f},
	{0x00, 0xff, 0x87}, {0x00, 0xff, 0xaf}, {0x00, 0xff, 0xd7}, {0x00, 0xff, 0xff},
	{0x5f, 0x00, 0x00}, {0x5f, 0x00, 0x5f}, {0x5f, 0x00, 0x87}, {0x5f, 0x00, 0xaf},
	{0x5f, 0x00, 0xd7}, {0x5f, 0x00, 0xff}, {0x5f, 0x5f, 0x00}, {0x5f, 0x5f, 0x5f},
	{0x5f, 0x5f, 0x87}, {0x5f, 0x5f, 0xaf}, {0x5f, 0x5f, 0xd7}, {0x5f, 0x5f, 0xff},
	{0x5f, 0x87, 0x00}, {0x5f, 0x87, 0x5f}, {0x5f, 0x87, 0x87}, {0x5f, 0x87, 0xaf},
	{0x5f, 0x87, 0xd7}, {0x5f, 0x87, 0xff}, {0x5f, 0xaf, 0x00}, {0x5f, 0xaf, 0x5f},
	{0x5f, 0xaf, 0x87}, {0x5f, 0xaf, 0xaf}, {0x5f, 0xaf, 0xd7}, {0x5f, 0xaf, 0xff},
	{0x5f, 0xd7, 0x00}, {0x5f, 0xd7, 0x5f}, {0x5f, 0xd7, 0x87}, {0x5f, 0xd7, 0xaf},
	{0x5f, 0xd7, 0xd7}, {0x5f, 0xd7, 0xff}, {0x5f, 0xff, 0x00}, {0x5f, 0xff, 0x5f},
	{0x5f, 0xff, 0x87}, {0x5f, 0xff, 0xaf}, {0x5f, 0xff, 0xd7}, {0x5f, 0xff, 0xff},
	{0x87, 0x00, 0x00}, {0x87, 0x00, 0x5f}, {0x87, 0x00, 0x87}, {0x87, 0x00, 0xaf},
	{0x87, 0x00, 0xd7}, {0x87, 0x00, 0xff}, {0x87, 0x5f, 0x00}, {0x87, 0x5f, 0x5f},
	{0x87, 0x5f, 0x87}, {0x87, 0x5f, 0xaf}, {0x87, 0x5f, 0xd7}, {0x87, 0x5f, 0xff},
	{0x87, 0x87, 0x00}, {0x87, 0x87, 0x5f}, {0x87, 0x87, 0x87}, {0x87, 0x87, 0xaf},
	{0x87, 0x87, 0xd7}, {0x87, 0x87, 0xff}, {0x87, 0xaf, 0x00}, {0x87, 0xaf, 0x5f},
	{0x87, 0xaf, 0x87}, {0x87, 0xaf, 0xaf}, {0x87, 0xaf, 0xd7}, {0x87, 0xaf, 0xff},
	{0x87, 0xd7, 0x00}, {0x87, 0xd7, 0x5f}, {0x87, 0xd7, 0x87}, {0x87, 0xd7, 0xaf},
	{0x87, 0xd7, 0xd7}, {0x87, 0xd7, 0xff}, {0x87, 0xff, 0x00}, {0x87, 0xff, 0x5f},
	{0x87, 0xff, 0x87}, {0x87, 0xff, 0xaf}, {0x87, 0xff, 0xd7}, {0x87, 0xff, 0xff},
	{0xaf, 0x00, 0x00}, {0xaf, 0x00, 0x5f}, {0xaf, 0x00, 0x87}, {0xaf, 0x00, 0xaf},
	{0xaf, 0x00, 0xd7}, {0xaf, 0x00, 0xff}, {0xaf, 0x5f, 0x00}, {0xaf, 0x5f, 0x5f},
	{0xaf, 0x5f, 0x87}, {0xaf, 0x5f, 0xaf}, {0xaf, 0x5f, 0xd7}, {0xaf, 0x5f, 0xff},
	{0xaf, 0x87, 0x00}, {0xaf, 0x87, 0x5f}, {0xaf, 0x87, 0x87}, {0xaf, 0x87, 0xaf},
	{0xaf, 0x87, 0xd7}, {0xaf, 0x87, 0xff}, {0xaf, 0xaf, 0x00}, {0xaf, 0xaf, 0x5f},
	{0xaf, 0xaf, 0x87}, {0xaf, 0xaf, 0xaf}, {0xaf, 0xaf, 0xd7}, {0xaf, 0xaf, 0xff},
	{0xaf, 0xd7, 0x00}, {0xaf, 0xd7, 0x5f}, {0xaf, 0xd7, 0x87}, {0xaf, 0xd7, 0xaf},
	{0xaf, 0xd7, 0xd7}, {0xaf, 0xd7, 0xff}, {0xaf, 0xff, 0x00}, {0xaf, 0xff, 0x5f},
	{0xaf, 0xff, 0x87}, {0xaf, 0xff, 0xaf}, {0xaf, 0xff, 0xd7}, {0xaf, 0xff, 0xff},
	{0xd7, 0x00, 0x00}, {0xd7, 0x00, 0x5f}, {0xd7, 0x00, 0x87}, {0xd7, 0x00, 0xaf},
	{0xd7, 0x00, 0xd7}, {0xd7, 0x00, 0xff}, {0xd7, 0x5f, 0x00}, {0xd7, 0x5f, 0x5f},
	{0xd7, 0x5f, 0x87}, {0xd7, 0x5f, 0xaf}, {0xd7, 0x5f, 0xd7}, {0xd7, 0x5f, 0xff},
	{0xd7, 0x87, 0x00}, {0xd7, 0x87, 0x5f}, {0xd7, 0x87, 0x87}, {0xd7, 0x87, 0xaf},
	{0xd7, 0x87, 0xd7}, {0xd7, 0x87, 0xff}, {0xd7, 0xaf, 0x00}, {0xd7, 0xaf, 0x5f},
	{0xd7, 0xaf, 0x87}, {0xd7, 0xaf, 0xaf}, {0xd7, 0xaf, 0xd7}, {0xd7, 0xaf, 0xff},
	{0xd7, 0xd7, 0x00}, {0xd7, 0xd7, 0x5f}, {0xd7, 0xd7, 0x87}, {0xd7, 0xd7, 0xaf},
	{0xd7, 0xd7, 0xd7}, {0xd7, 0xd7, 0xff}, {0xd7, 0xff, 0x00}, {0xd7, 0xff, 0x5f},
	{0xd7, 0xff, 0x87}, {0xd7, 0xff, 0xaf}, {0xd7, 0xff, 0xd7}, {0xd7, 0xff, 0xff},
	{0xff, 0x00, 0x00}, {0xff, 0x00, 0x5f}, {0xff, 0x00, 0x87}, {0xff, 0x00, 0xaf},
	{0xff, 0x00, 0xd7}, {0xff, 0x00, 0xff}, {0xff, 0x5f, 0x00}, {0xff, 0x5f, 0x5f},
	{0xff, 0x5f, 0x87}, {0xff, 0x5f, 0xaf}, {0xff, 0x5f, 0xd7}, {0xff, 0x5f, 0xff},
	{0xff, 0x87, 0x00}, {0xff, 0x87, 0x5f}, {0xff, 0x87, 0x87}, {0xff, 0x87, 0xaf},
	{0xff, 0x87, 0xd7}, {0xff, 0x87, 0xff}, {0xff, 0xaf, 0x00}, {0xff, 0xaf, 0x5f},
	{0xff, 0xaf, 0x87}, {0xff, 0xaf, 0xaf}, {0xff, 0xaf, 0xd7}, {0xff, 0xaf, 0xff},
	{0xff, 0xd7, 0x00}, {0xff, 0xd7, 0x5f}, {0xff, 0xd7, 0x87}, {0xff, 0xd7, 0xaf},
	{0xff, 0xd7, 0xd7}, {0xff, 0xd7, 0xff}, {0xff, 0xff, 0x00}, {0xff, 0xff, 0x5f},
	{0xff, 0xff, 0x87}, {0xff, 0xff, 0xaf}, {0xff, 0xff, 0xd7}, {0xff, 0xff, 0xff},
	{0x08, 0x08, 0x08}, {0x12, 0x12, 0x12}, {0x1c, 0x1c, 0x1c}, {0x26, 0x26, 0x26},
	{0x30, 0x30, 0x30}, {0x3a, 0x3a, 0x3a}, {0x44, 0x44, 0x44}, {0x4e, 0x4e, 0x4e},
	{0x58, 0x58, 0x58}, {0x62, 0x62, 0x62}, {0x6c, 0x6c, 0x6c}, {0x76, 0x76, 0x76},
	{0x80, 0x80, 0x80}, {0x8a, 0x8a, 0x8a}, {0x94, 0x94, 0x94}, {0x9e, 0x9e, 0x9e},
	{0xa8, 0xa8, 0xa8}, {0xb2, 0xb2, 0xb2}, {0xbc, 0xbc, 0xbc}, {0xc6, 0xc6, 0xc6},
	{0xd0, 0xd0, 0xd0}, {0xda, 0xda, 0xda}, {0xe4, 0xe4, 0xe4}, {0xee, 0xee, 0xee},
};

static u32 rgb_to_color(u8 r, u8 g, u8 b)
{
	return 0x00000000 | (r << 16) | (g << 8) | b;
}

// HLS to RGB conversion helpers
static int hue2rgb(int n1, int n2, int hue)
{
	if (hue < 0) hue += 360;
	if (hue >= 360) hue -= 360;

	if (hue < 60)  return n1 + (n2 - n1) * hue / 60;
	if (hue < 180) return n2;
	if (hue < 240) return n1 + (n2 - n1) * (240 - hue) / 60;
	return n1;
}

static void hls2rgb(int hue, int lum, int sat, u8 *r, u8 *g, u8 *b)
{
	if (!sat) {
		*r = *g = *b = lum * 255 / 100;
		return;
	}

	int n2;
	if (lum <= 50) n2 = lum * (100 + sat) / 100 * 255 / 100;
	else n2 = (lum + sat - (lum * sat / 100)) * 255 / 100;

	int n1 = 2 * lum * 255 / 100 - n2;

	*r = hue2rgb(n1, n2, hue + 120);
	*g = hue2rgb(n1, n2, hue);
	*b = hue2rgb(n1, n2, hue - 120);
}

void VTerm::reset_sixel()
{
	sixel_canvas_t *sc = mSixelCanvas;
	if (!sc) return;
	if (!sc->pixmap) return;

	memset(sc->pixmap, 0, sc->width * sc->height * 4);

	sc->color_index = 0;
	// Initialize color table 0-15 from VT340 palette
	for (int i = 0; i < 16; i++) {
		sc->color_table[i] = rgb_to_color(
			vt340_palette[i][0], vt340_palette[i][1], vt340_palette[i][2]);
	}
	// Initialize color table 16-255 from xterm palette
	for (int i = 0; i < 240; i++) {
		sc->color_table[i + 16] = rgb_to_color(
			xterm_palette[i][0], xterm_palette[i][1], xterm_palette[i][2]);
	}

	// Index 0 = current terminal foreground color
	if (cur_fcolor_direct && cur_fcolor < NR_DIRECT_COLORS) {
		const Color &fg = mDirectColors[cur_fcolor];
		sc->color_table[0] = rgb_to_color(fg.red, fg.green, fg.blue);
	} else {
		// Use default VT340 black for palette-based colors (can be redefined via #)
		sc->color_table[0] = rgb_to_color(0, 0, 0);
	}

	sc->point_x = 0;
	sc->point_y = 0;
	// Track actual image bounds (logical sixel image size, not canvas buffer)
	sc->img_width = 1;
	sc->img_height = 6;
}

void VTerm::sixel_bitmap(u8 bits)
{
	sixel_canvas_t *sc = mSixelCanvas;
	if (!sc) { DBG("sixel_bitmap: no canvas\n"); return; }

	if (sc->point_x < 0 || sc->point_x >= sc->width ||
	    sc->point_y < 0 || sc->point_y + 5 >= sc->height) {
		DBG("sixel_bitmap: OOB x=%d y=%d w=%d h=%d\n", sc->point_x, sc->point_y, sc->width, sc->height);
		return;
	}

	// Set alpha=0xFF to distinguish drawn pixels from transparent (0x00000000)
	u32 color = sc->color_table[sc->color_index] | 0xFF000000;
	int offset = sc->point_x * 4 + sc->point_y * sc->line_length;

	for (int i = 0; i < 6; i++) {
		if (bits & (1 << i)) {
			*(u32 *)(sc->pixmap + offset) = color;
		}
		offset += sc->line_length;
	}

	sc->point_x++;
	if (sc->point_x > sc->img_width) sc->img_width = sc->point_x;
}

void VTerm::sixel_cr()
{
	if (mSixelCanvas) {
		DBG("sixel_cr: x=%d->0\n", mSixelCanvas->point_x);
		mSixelCanvas->point_x = 0;
	}
}

void VTerm::sixel_nl()
{
	sixel_canvas_t *sc = mSixelCanvas;
	if (!sc) { DBG("sixel_nl: no canvas\n"); return; }

	DBG("sixel_nl: x=%d->0 y=%d->%d\n", sc->point_x, sc->point_y, sc->point_y + 6);
	sc->point_x = 0;
	sc->point_y += 6;

	// Cap img_height at the actual allocated pixmap height to prevent OOB access
	if (sc->point_y + 5 > sc->img_height) {
		int new_h = sc->point_y + 6;
		if (new_h > sc->height) new_h = sc->height;  // clamp to allocated
		if (new_h > 0) sc->img_height = new_h;
	}
}

void VTerm::sixel_repeat(const u8 **data, u32 *len)
{
	sixel_canvas_t *sc = mSixelCanvas;
	if (!sc) return;

	u32 count = 0;
	while (*len > 0 && **data >= '0' && **data <= '9') {
		u32 next = count * 10 + (**data - '0');
		if (next < count) { count = 0xFFFFFFFF; break; } // overflow, saturate
		count = next;
		(*data)++;
		(*len)--;
	}

	if (count == 0) count = 1;

	// Sanity limit: cap repeat count to prevent freeze from excessive digits
	if (count > 65535) count = 65535;

	if (*len > 0 && **data >= '?' && **data <= '~') {
		u8 bits = **data - '?';
		(*data)++;
		(*len)--;

		for (u32 i = 0; i < count; i++) {
			sixel_bitmap(bits);
		}
	}
}

void VTerm::sixel_attr(const u8 **data, u32 *len)
{
	sixel_canvas_t *sc = mSixelCanvas;
	if (!sc) return;

	// Parse " Pan; Pad; Ph; Pv
	// We only care about Ph and Pv (parameters 3 and 4)
	int params[4] = {0, 0, 0, 0};
	int param_idx = 0;
	bool in_param = false;

	while (*len > 0 && **data != ';' && **data >= '0' && **data <= '9') {
		int next = params[param_idx] * 10 + (**data - '0');
		if (next > 65535) next = 65535; // clamp to reasonable limit
		params[param_idx] = next;
		in_param = true;
		(*data)++;
		(*len)--;
	}

	while (*len > 0 && param_idx < 3) {
		if (**data == ';') {
			param_idx++;
			in_param = false;
			(*data)++;
			(*len)--;
		} else if (**data >= '0' && **data <= '9') {
			int next = params[param_idx] * 10 + (**data - '0');
			if (next > 65535) next = 65535;
			params[param_idx] = next;
			in_param = true;
			(*data)++;
			(*len)--;
		} else {
			break;
		}
	}

	// Only update width; height is the allocated canvas buffer size
	// and must not be reduced (img_height tracks logical image height)
	if (params[2] > 0) sc->width = params[2];
}

void VTerm::sixel_color(const u8 **data, u32 *len)
{
	sixel_canvas_t *sc = mSixelCanvas;
	if (!sc) return;

	// Parse #Pc or #Pc;Pu;Px;Py;Pz
	int params[5] = {0, 0, 0, 0, 0};
	int param_idx = 0;

	while (*len > 0 && param_idx < 5) {
		if (**data == ';') {
			param_idx++;
			(*data)++;
			(*len)--;
		} else if (**data >= '0' && **data <= '9') {
			int next = params[param_idx] * 10 + (**data - '0');
			if (next > 65535) next = 65535; // clamp to avoid overflow
			params[param_idx] = next;
			(*data)++;
			(*len)--;
		} else {
			break;
		}
	}

	if (param_idx == 0) {
		// Select color: #Pc
		if (params[0] < 256) {
			sc->color_index = params[0];
		}
	} else if (param_idx >= 4) {
		// Define color: #Pc;Pu;Px;Py;Pz
		int index = params[0];
		if (index >= 256) return;

		u8 r, g, b;
		if (params[1] == 1) {
			// HLS color space
			hls2rgb(params[2] % 360, params[3] % 100, params[4] % 100, &r, &g, &b);
		} else {
			// RGB color space (percentage 0-100)
			r = params[2] * 255 / 100;
			g = params[3] * 255 / 100;
			b = params[4] * 255 / 100;
		}
		sc->color_table[index] = rgb_to_color(r, g, b);
		// VT340 spec: defining a color also selects it
		sc->color_index = index;
	}
}

void VTerm::sixel_parse_data(const u8 *data, u32 len)
{
	DBG("sixel_parse_data: len=%u\n", len);
	while (len > 0) {
		u8 c = *data;

		switch (c) {
		case '!':
			data++;
			len--;
			sixel_repeat(&data, &len);
			break;
		case '"':
			data++;
			len--;
			sixel_attr(&data, &len);
			break;
		case '#':
			data++;
			len--;
			sixel_color(&data, &len);
			break;
		case '$':
			sixel_cr();
			data++;
			len--;
			break;
		case '-':
			sixel_nl();
			data++;
			len--;
			break;
		default:
			if (c >= '?' && c <= '~') {
				sixel_bitmap(c - '?');
			}
			data++;
			len--;
			break;
		}
	}
}

void VTerm::sixel_copy_to_cells()
{
	sixel_canvas_t *sc = mSixelCanvas;
	if (!sc || !pixmaps) { DBG("copy: no sc or pixmaps\n"); return; }
	if (!mCellW || !mCellH) { DBG("copy: mCellW=%u mCellH=%u\n", mCellW, mCellH); return; }

	// Use actual image bounds, not canvas buffer dimensions
	int img_w = sc->img_width;
	int img_h = sc->img_height;
	if (img_w <= 0 || img_h <= 0) { DBG("copy: empty image w=%d h=%d\n", img_w, img_h); return; }

	int cols = (img_w + mCellW - 1) / mCellW;
	int lines = (img_h + mCellH - 1) / mCellH;

	if (cols > (int)width) cols = width;

	// Auto-scroll to make room if image extends past scroll bottom
	{
		int room = (scroll_bot + 1) - cursor_y;
		if (lines > room) {
			int need = lines - room;
			int mx = scroll_bot - scroll_top + 1;
			if (need > mx) need = mx;
			scroll_region(scroll_top, scroll_bot, need);
			if (cursor_y >= scroll_top + need)
				cursor_y -= need;
			else
				cursor_y = scroll_top;
			update();
		}
	}

	if (cursor_y + lines > (int)height) lines = height - cursor_y;

	DBG("copy: canvas=%dx%d img=%dx%d cell=%ux%u cols=%d lines=%d cursor=(%u,%u) term=%ux%u\n",
		sc->width, sc->height, img_w, img_h, mCellW, mCellH, cols, lines, cursor_x, cursor_y, width, height);

	u16 start_x = cursor_x, start_y = cursor_y;

	for (int cy = 0; cy < lines; cy++) {
		int screen_y = start_y + cy;
		if (screen_y >= (int)height) break;

		u32 yp = linenumbers[screen_y] * max_width;

		for (int cx = 0; cx < cols; cx++) {
			int screen_x = start_x + cx;
			if (screen_x >= (int)width) break;

			u32 cell_idx = yp + screen_x;

			// Free existing pixmap if any
			free_pixmap(cell_idx);

			// Allocate new pixmap for this cell
			size_t cell_sz = (size_t)mCellW * mCellH * 4;
			if (cell_sz / 4 / mCellH != mCellW) continue; // overflow check
			u8 *cell_pixmap = (u8 *)malloc(cell_sz);
			if (!cell_pixmap) continue;

			pixmaps[cell_idx] = cell_pixmap;

			// Copy pixels from sixel canvas to cell pixmap
			int src_base_x = cx * mCellW;
			int src_base_y = cy * mCellH;

			for (u32 py = 0; py < mCellH; py++) {
				int src_y = src_base_y + py;
				u32 *dst_row = (u32 *)(cell_pixmap + py * mCellW * 4);

				for (u32 px = 0; px < mCellW; px++) {
					int src_x = src_base_x + px;
					if (src_x < sc->width && src_y < sc->height) {
						u32 *src = (u32 *)(sc->pixmap + src_x * 4 + src_y * sc->line_length);
						dst_row[px] = *src;
					} else {
						dst_row[px] = 0;
					}
				}
			}

			// Mark cell as having a pixmap
			attrs[cell_idx].has_pixmap = 1;

			// Mark line as dirty
			changed_line(screen_y, screen_x, screen_x);
		}
	}

	// Move cursor below image
	u16 new_y = start_y + lines;
	if (new_y >= height) new_y = height - 1;
	move_cursor(0, new_y);
}

void VTerm::sixel_parse_header()
{
	if (!mDcsLen) return;

	// Parse P1;P2;P3 from DCS header before 'q'
	// Format: DCS P1;P2;P3; q <sixel-data> ST
	// The sixel_parse_data function will parse the data after 'q'

	// Initialize sixel canvas if needed
	if (!mSixelCanvas) {
		// Get cell dimensions from Font if available
		// These will be set during the first shell creation
		mSixelCanvas = (sixel_canvas_t *)calloc(1, sizeof(sixel_canvas_t));
	}

	if (!mSixelCanvas) return;

	// Allocate canvas pixmap with terminal pixel dimensions
	// Use mCellW and mCellH to calculate full terminal pixel size
	u32 pix_w = mCellW ? mCellW * width : width * 8;
	u32 pix_h = mCellH ? mCellH * height : height * 16;

	DBG("sixel_header: mCellW=%u mCellH=%u width=%u height=%u pix=%ux%u\n",
		mCellW, mCellH, width, height, pix_w, pix_h);

	if (!mSixelCanvas->pixmap || (int)pix_w != mSixelCanvas->width || (int)pix_h != mSixelCanvas->height) {
		if (mSixelCanvas->pixmap) free(mSixelCanvas->pixmap);
		// Check for overflow: pix_w * pix_h must fit in size_t
		u64 alloc_sz = (u64)pix_w * pix_h;
		if (alloc_sz > 0x7FFFFFFF) { // Sanity limit: 2GB
			mSixelCanvas->pixmap = 0;
			mSixelCanvas->width = 0;
			mSixelCanvas->height = 0;
			DBG("sixel_header: allocation too large %llu\n", (unsigned long long)alloc_sz);
			return;
		}
		mSixelCanvas->pixmap = (u8 *)calloc((size_t)alloc_sz, 4);
		if (!mSixelCanvas->pixmap) {
			mSixelCanvas->width = 0;
			mSixelCanvas->height = 0;
			DBG("sixel_header: calloc failed\n");
			return;
		}
		mSixelCanvas->width = pix_w;
		mSixelCanvas->height = pix_h;
		mSixelCanvas->line_length = pix_w * 4;
		DBG("sixel_header: allocated canvas %p size=%dx%d\n",
			(void*)mSixelCanvas->pixmap, pix_w, pix_h);
	}

	reset_sixel();

	// Find the 'q' that marks end of header / start of data
	u32 data_start = 0;
	for (u32 i = 0; i < mDcsLen; i++) {
		if (mDcsBuf[i] == 'q') {
			data_start = i + 1;
			break;
		}
	}

	if (data_start == 0) { DBG("sixel_header: no 'q' found in buffer len=%u\n", mDcsLen); return; }

	DBG("sixel_header: data at offset %u, total buf len=%u\n", data_start, mDcsLen);

	// Parse header parameters (P1;P2;P3 before 'q')
	// P1: pixel aspect ratio (ignored)
	// P2: background select (ignored)
	// P3: horizontal grid size (ignored, we handle via sixel_attr)

	// Parse the sixel data
	sixel_parse_data(mDcsBuf + data_start, mDcsLen - data_start);

	// Distribute canvas pixels to terminal cells
	sixel_copy_to_cells();
}
