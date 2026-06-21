/*
 *   RGA Hardware Acceleration Adapter
 *   Wrapper for librga (Rockchip Raster Graphics Acceleration)
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 */

#ifndef RGA_ADAPTER_H
#define RGA_ADAPTER_H

#include "type.h"

#ifdef ENABLE_RGA

class RgaAdapter {
public:
	static RgaAdapter *instance();
	bool isAvailable() const { return mAvailable; }

	// Copy src buffer to dst buffer using RGA hardware
	bool copy(void *dst, void *src, u32 width, u32 height, u32 stride, u32 bpp);

	// Fill a rectangle with a solid color using RGA hardware
	bool fill(void *dst, u32 x, u32 y, u32 w, u32 h,
		  u32 fbWidth, u32 fbHeight, u32 stride, u32 bpp, u32 pixel);

	// Dirty rect tracking for batch-merge rendering
	void addDirtyRect(u32 x, u32 y, u32 w, u32 h);
	bool flushDirtyRects(void *backBuf, void *realBuf,
			     u32 width, u32 height, u32 stride, u32 bpp);
	void clearDirtyRects();
	u32 dirtyRectCount() const { return mDirtyCount; }

private:
	RgaAdapter();
	~RgaAdapter();
	bool probeHardware();

	bool mAvailable;

	// Dirty rect tracking
	struct DirtyRect { u32 x, y, w, h; };
	enum { MAX_DIRTY_RECTS = 128 };

	DirtyRect mDirtyRects[MAX_DIRTY_RECTS];
	u32 mDirtyCount;
	bool mDirtyOverflow;

	static int formatFromBpp(u32 bpp);
};

#endif // ENABLE_RGA

#endif // RGA_ADAPTER_H
