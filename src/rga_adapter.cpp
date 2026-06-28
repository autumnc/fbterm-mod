/*
 *   RGA Hardware Acceleration Adapter
 *   Wrapper for librga (Rockchip Raster Graphics Acceleration)
 *   Supports both im2d API (kernel 4.4+) and old RGA API (kernel 3.10)
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 */

#include "config.h"
#ifdef ENABLE_RGA

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "rga_adapter.h"

#if defined(RGA_USE_IM2D)
# include <rga/im2d.h>
#elif defined(RGA_USE_OLD_API)
# if defined(HAVE_RGA_ROCKCHIPRGA_H)
#  include <rga/RockchipRga.h>
# elif defined(HAVE_RGA_RGAAPI_H)
#  include <rga/RgaApi.h>
# else
#  include <RockchipRga.h>
# endif
#endif

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifdef RGA_USE_OLD_API
/* Old API requires manual init/deinit of a context */
static bool sOldApiInited = false;
#endif

static RgaAdapter *sInstance = 0;

RgaAdapter *RgaAdapter::instance()
{
	if (!sInstance) {
		RgaAdapter *a = new RgaAdapter();
		if (a->isAvailable()) {
			sInstance = a;
		} else {
			delete a;
		}
	}
	return sInstance;
}

RgaAdapter::RgaAdapter()
	: mAvailable(false),
	  mDirtyCount(0),
	  mDirtyOverflow(false)
{
	mAvailable = probeHardware();
}

RgaAdapter::~RgaAdapter()
{
#ifdef RGA_USE_OLD_API
	if (sOldApiInited) {
		c_RkRgaDeInit();
		sOldApiInited = false;
	}
#endif
}

bool RgaAdapter::probeHardware()
{
	int fd = open("/dev/rga", O_RDONLY);
	if (fd < 0) return false;
	close(fd);

#ifdef RGA_USE_IM2D
	// Probe with im2d: try a tiny wrapbuffer + imcopy
	u8 dummy[16] __attribute__((aligned(16)));
	memset(dummy, 0, sizeof(dummy));

	rga_buffer_t buf = wrapbuffer_virtualaddr_t(dummy, 4, 4, 4, 4, RK_FORMAT_RGBA_8888);
	IM_STATUS ret = imcopy_t(buf, buf, IM_SYNC);

	return (ret == IM_STATUS_SUCCESS);

#elif defined(RGA_USE_OLD_API)
	// Probe with old API: init, try a tiny blit
	if (c_RkRgaInit() != 0) return false;
	sOldApiInited = true;

	u8 dummy[16] __attribute__((aligned(16)));
	memset(dummy, 0, sizeof(dummy));

	rga_info_t info;
	memset(&info, 0, sizeof(info));
	info.fd = -1;
	info.virAddr = dummy;
	info.mmuFlag = 1;
	info.format = RK_FORMAT_RGBA_8888;
	info.rect.xoffset = 0;
	info.rect.yoffset = 0;
	info.rect.width = 4;
	info.rect.height = 4;
		info.rect.wstride = 4;
		info.rect.hstride = 4;
		info.rect.format = RK_FORMAT_RGBA_8888;

	int ret = c_RkRgaBlit(&info, &info, NULL);
	return (ret == 0);

#else
	return false;
#endif
}

int RgaAdapter::formatFromBpp(u32 bpp)
{
	switch (bpp) {
	case 32: return RK_FORMAT_BGRA_8888;
	case 16: return RK_FORMAT_RGB_565;
	case 15: return RK_FORMAT_RGBA_5551;
	case 8:  return RK_FORMAT_BPP8;
	default: return RK_FORMAT_BGRA_8888;
	}
}

bool RgaAdapter::copy(void *dst, void *src, u32 width, u32 height, u32 stride, u32 bpp)
{
	if (!mAvailable) return false;

	int fmt = formatFromBpp(bpp);
	int wstride = stride * 8 / bpp;

#ifdef RGA_USE_IM2D
	rga_buffer_t srcBuf = wrapbuffer_virtualaddr_t(src, width, height, wstride, height, fmt);
	rga_buffer_t dstBuf = wrapbuffer_virtualaddr_t(dst, width, height, wstride, height, fmt);

	IM_STATUS ret = imcopy_t(srcBuf, dstBuf, IM_SYNC);
	return (ret == IM_STATUS_SUCCESS);

#elif defined(RGA_USE_OLD_API)
	rga_info_t srcInfo, dstInfo;
	memset(&srcInfo, 0, sizeof(srcInfo));
	memset(&dstInfo, 0, sizeof(dstInfo));

	srcInfo.fd = -1;
	srcInfo.virAddr = src;
	srcInfo.mmuFlag = 1;
	srcInfo.format = fmt;
	srcInfo.rect.xoffset = 0;
	srcInfo.rect.yoffset = 0;
	srcInfo.rect.width = (int)width;
	srcInfo.rect.height = (int)height;
		srcInfo.rect.wstride = wstride;
		srcInfo.rect.hstride = (int)height;
		srcInfo.rect.format = fmt;

	dstInfo.fd = -1;
	dstInfo.virAddr = dst;
	dstInfo.mmuFlag = 1;
	dstInfo.format = fmt;
	dstInfo.rect.xoffset = 0;
	dstInfo.rect.yoffset = 0;
	dstInfo.rect.width = (int)width;
	dstInfo.rect.height = (int)height;
		dstInfo.rect.wstride = wstride;
		dstInfo.rect.hstride = (int)height;
		dstInfo.rect.format = fmt;

	int ret = c_RkRgaBlit(&srcInfo, &dstInfo, NULL);
	return (ret == 0);

#else
	return false;
#endif
}

bool RgaAdapter::fill(void *dst, u32 x, u32 y, u32 w, u32 h,
		       u32 fbWidth, u32 fbHeight, u32 stride, u32 bpp, u32 pixel)
{
	if (!mAvailable) return false;

	int fmt = formatFromBpp(bpp);
	int wstride = stride * 8 / bpp;

#ifdef RGA_USE_IM2D
	rga_buffer_t dstBuf = wrapbuffer_virtualaddr_t(dst, fbWidth, fbHeight, wstride, fbHeight, fmt);

	im_rect rect;
	rect.x = (int)x;
	rect.y = (int)y;
	rect.width  = (int)w;
	rect.height = (int)h;

	IM_STATUS ret = imfill_t(dstBuf, rect, (int)pixel, IM_SYNC);
	return (ret == IM_STATUS_SUCCESS);

#elif defined(RGA_USE_OLD_API)
	rga_info_t dstInfo;
	memset(&dstInfo, 0, sizeof(dstInfo));

	dstInfo.fd = -1;
	dstInfo.virAddr = dst;
	dstInfo.mmuFlag = 1;
	dstInfo.format = fmt;
	dstInfo.rect.xoffset = (int)x;
	dstInfo.rect.yoffset = (int)y;
	dstInfo.rect.width = (int)w;
	dstInfo.rect.height = (int)h;
		dstInfo.rect.wstride = wstride;
		dstInfo.rect.hstride = (int)fbHeight;
		dstInfo.rect.format = fmt;
	dstInfo.color = (int)pixel;

	int ret = c_RkRgaColorFill(&dstInfo);
	return (ret == 0);

#else
	return false;
#endif
}

void RgaAdapter::addDirtyRect(u32 x, u32 y, u32 w, u32 h)
{
	if (!w || !h) return;
	if (mDirtyOverflow) return;

	if (mDirtyCount >= MAX_DIRTY_RECTS) {
		mDirtyOverflow = true;
		return;
	}

	mDirtyRects[mDirtyCount].x = x;
	mDirtyRects[mDirtyCount].y = y;
	mDirtyRects[mDirtyCount].w = w;
	mDirtyRects[mDirtyCount].h = h;
	mDirtyCount++;
}

void RgaAdapter::clearDirtyRects()
{
	mDirtyCount = 0;
	mDirtyOverflow = false;
}

bool RgaAdapter::flushDirtyRects(void *backBuf, void *realBuf,
				  u32 width, u32 height, u32 stride, u32 bpp)
{
	if (!mAvailable || !mDirtyCount || mDirtyOverflow) return false;

	u32 totalArea = 0;
	for (u32 i = 0; i < mDirtyCount; i++) {
		totalArea += mDirtyRects[i].w * mDirtyRects[i].h;
	}
	u32 screenArea = width * height;
	if (totalArea > screenArea * 60 / 100) return false;

	int fmt = formatFromBpp(bpp);
	int wstride = stride * 8 / bpp;
	int bppByte = bpp >> 3;
	if (bppByte == 0) bppByte = 1;

	bool ok = true;

#ifdef RGA_USE_IM2D
	for (u32 i = 0; i < mDirtyCount; i++) {
		DirtyRect &r = mDirtyRects[i];

		u32 offset = r.y * stride + r.x * bppByte;
		u8 *srcPtr = (u8 *)backBuf + offset;
		u8 *dstPtr = (u8 *)realBuf + offset;

		rga_buffer_t srcBuf = wrapbuffer_virtualaddr_t(srcPtr, r.w, r.h, wstride, r.h, fmt);
		rga_buffer_t dstBuf = wrapbuffer_virtualaddr_t(dstPtr, r.w, r.h, wstride, r.h, fmt);

		IM_STATUS ret = imcopy_t(srcBuf, dstBuf, IM_SYNC);
		if (ret != IM_STATUS_SUCCESS) {
			ok = false;
			break;
		}
	}

#elif defined(RGA_USE_OLD_API)
	for (u32 i = 0; i < mDirtyCount; i++) {
		DirtyRect &r = mDirtyRects[i];

		u32 offset = r.y * stride + r.x * bppByte;
		u8 *srcPtr = (u8 *)backBuf + offset;
		u8 *dstPtr = (u8 *)realBuf + offset;

		rga_info_t srcInfo, dstInfo;
		memset(&srcInfo, 0, sizeof(srcInfo));
		memset(&dstInfo, 0, sizeof(dstInfo));

		srcInfo.fd = -1;
		srcInfo.virAddr = srcPtr;
		srcInfo.mmuFlag = 1;
		srcInfo.format = fmt;
		srcInfo.rect.xoffset = 0;
		srcInfo.rect.yoffset = 0;
		srcInfo.rect.width = (int)r.w;
		srcInfo.rect.height = (int)r.h;
			srcInfo.rect.wstride = wstride;
			srcInfo.rect.hstride = (int)r.h;
			srcInfo.rect.format = fmt;

		dstInfo.fd = -1;
		dstInfo.virAddr = dstPtr;
		dstInfo.mmuFlag = 1;
		dstInfo.format = fmt;
		dstInfo.rect.xoffset = 0;
		dstInfo.rect.yoffset = 0;
		dstInfo.rect.width = (int)r.w;
		dstInfo.rect.height = (int)r.h;
			dstInfo.rect.wstride = wstride;
			dstInfo.rect.hstride = (int)r.h;
			dstInfo.rect.format = fmt;

		int ret = c_RkRgaBlit(&srcInfo, &dstInfo, NULL);
		if (ret != 0) {
			ok = false;
			break;
		}
	}
#endif

	return ok;
}

#endif // ENABLE_RGA
