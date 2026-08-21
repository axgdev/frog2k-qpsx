/***************************************************************************
*   Copyright (C) 2010 PCSX4ALL Team                                      *
*   Copyright (C) 2010 Unai                                               *
*   Copyright (C) 2016 Senquack (dansilsby <AT> gmail <DOT> com)          *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   51 Franklin Street, Fifth Floor, Boston, MA 02111-1307 USA.           *
***************************************************************************/

///////////////////////////////////////////////////////////////////////////////
// Inner loop driver instantiation file

///////////////////////////////////////////////////////////////////////////////
//  Option Masks (CF template paramter)
#define  CF_LIGHT     ((CF>> 0)&1) // Lighting
#define  CF_BLEND     ((CF>> 1)&1) // Blending
#define  CF_MASKCHECK ((CF>> 2)&1) // Mask bit check
#define  CF_BLENDMODE ((CF>> 3)&3) // Blend mode   0..3
#define  CF_TEXTMODE  ((CF>> 5)&3) // Texture mode 1..3 (0: texturing disabled)
#define  CF_GOURAUD   ((CF>> 7)&1) // Gouraud shading
#define  CF_MASKSET   ((CF>> 8)&1) // Mask bit set
#define  CF_DITHER    ((CF>> 9)&1) // Dithering
#define  CF_BLITMASK  ((CF>>10)&1) // blit_mask check (skip rendering pixels
                                   //  that wouldn't end up displayed on
                                   //  low-res screen using simple downscaler)

/* Runtime renderer diagnostics are deliberately opt-in.  The production
 * core keeps these declarations and all associated updates out of the
 * generated inner loops; a metrics core uses them to weight driver variants
 * by the number of spans/pixels they actually process. */
#ifndef QPSX_GPU_RUNTIME_METRICS
#define QPSX_GPU_RUNTIME_METRICS 0
#endif
#ifndef QPSX_GPU_LINEAR_4BPP
#define QPSX_GPU_LINEAR_4BPP 0
#endif
#ifndef QPSX_GPU_PACKED_TILE_WRITES
#define QPSX_GPU_PACKED_TILE_WRITES 0
#endif
#ifndef QPSX_GPU_PACKED_SPRITE_4BPP
#define QPSX_GPU_PACKED_SPRITE_4BPP 0
#endif
#ifndef QPSX_GPU_PACKED_POLY_WRITES
#define QPSX_GPU_PACKED_POLY_WRITES 0
#endif
#ifndef QPSX_GPU_4BPP_GOURAUD_FLATV
#define QPSX_GPU_4BPP_GOURAUD_FLATV 0
#endif
#ifndef QPSX_GPU_4BPP_GOURAUD_FLATV_MIN_PIXELS
#define QPSX_GPU_4BPP_GOURAUD_FLATV_MIN_PIXELS 16
#endif
#ifndef QPSX_GPU_4BPP_GOURAUD_CACHE
#define QPSX_GPU_4BPP_GOURAUD_CACHE 0
#endif
#ifndef QPSX_GPU_4BPP_FLATV
#define QPSX_GPU_4BPP_FLATV 0
#endif
#ifndef QPSX_GPU_4BPP_FLATV_MIN_PIXELS
#define QPSX_GPU_4BPP_FLATV_MIN_PIXELS 16
#endif
#ifndef QPSX_GPU_4BPP_FLATV_ROW
#define QPSX_GPU_4BPP_FLATV_ROW 0
#endif
#ifndef QPSX_GPU_4BPP_FLATV_ROW_MIN_PIXELS
#define QPSX_GPU_4BPP_FLATV_ROW_MIN_PIXELS 16
#endif
#ifndef QPSX_GPU_4BPP_PALETTE_LUT
#define QPSX_GPU_4BPP_PALETTE_LUT 0
#endif
#ifndef QPSX_GPU_HOT_DRIVER_ORDER
#define QPSX_GPU_HOT_DRIVER_ORDER 0
#endif
#ifndef QPSX_GPU_4BPP_FULLMASK
#define QPSX_GPU_4BPP_FULLMASK 0
#endif
#ifndef QPSX_GPU_4BPP_FULLMASK_MIN_PIXELS
#define QPSX_GPU_4BPP_FULLMASK_MIN_PIXELS 16
#endif
#ifndef QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES
#define QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES 0
#endif
#ifndef QPSX_GPU_4BPP_FULLMASK_PACKED_UNROLL
#define QPSX_GPU_4BPP_FULLMASK_PACKED_UNROLL 0
#endif
#ifndef QPSX_GPU_GOURAUD_LINE_FLATFAST
#define QPSX_GPU_GOURAUD_LINE_FLATFAST 0
#endif
#ifndef QPSX_GPU_POLY_2043_FAST
#define QPSX_GPU_POLY_2043_FAST 0
#endif
#if QPSX_GPU_4BPP_PALETTE_LUT
#if defined(__GNUC__)
#define QPSX_GPU_PALETTE_LUT_NOINLINE __attribute__((noinline))
#else
#define QPSX_GPU_PALETTE_LUT_NOINLINE
#endif
static QPSX_GPU_PALETTE_LUT_NOINLINE void
qpsx_gpu_prepare_4bpp_palette_lut(const gpu_unai_t &gpu_unai)
{
	if (gpu_unai.CBA4PackedValid)
		return;
	const u16 *cba = gpu_unai.CBA;
	for (u32 packed = 0; packed < 256; ++packed) {
		gpu_unai.CBA4Packed[packed] =
			(u32)cba[packed & 0xfu] |
			((u32)cba[packed >> 4] << 16);
	}
	gpu_unai.CBA4PackedValid = true;
}
#undef QPSX_GPU_PALETTE_LUT_NOINLINE
#endif
#if QPSX_GPU_RUNTIME_METRICS
extern u32 qpsx_gpu_poly_span_hist[2048];
extern u32 qpsx_gpu_poly_pixel_hist[2048];
extern u32 qpsx_gpu_sprite_pixel_hist[256];
extern u32 qpsx_gpu_tile_pixel_hist[32];
extern u32 qpsx_gpu_poly_fullmask_spans;
extern u32 qpsx_gpu_poly_fullmask_pixels;
extern u32 qpsx_gpu_poly_unit_u_spans;
extern u32 qpsx_gpu_poly_unit_u_pixels;
extern u32 qpsx_gpu_poly_flat_v_spans;
extern u32 qpsx_gpu_poly_flat_v_pixels;
extern u32 qpsx_gpu_line_g_total;
extern u32 qpsx_gpu_line_g_flatfast;
#endif

#if QPSX_GPU_4BPP_FLATV
/*
 * The dominant Ridge Racer polygon driver is CF=32: opaque, unlit 4bpp.
 * For a normal horizontal span V is constant and the texture window is the
 * full 256x256 page.  The generic loop still masks U and V and recomputes the
 * byte-row address for every pixel.  This path proves that U will not wrap
 * during the span once, then keeps a row pointer and only advances U.  It is
 * deliberately restricted to CF=32 callers; all window, wrap, blend, mask,
 * lighting, and Gouraud cases retain the exact original loop.  The helper is
 * out-of-line and only considered for long spans: putting the proof and the
 * paired-byte loop into the CF=32 hot function enlarged its instruction
 * footprint enough to lose on a 16 KiB I-cache even when it saved pixels.
 */
#if defined(__GNUC__)
#define QPSX_GPU_FLATV_NOINLINE __attribute__((noinline))
#else
#define QPSX_GPU_FLATV_NOINLINE
#endif
static QPSX_GPU_FLATV_NOINLINE bool
qpsx_gpu_poly_span_4bpp_flatv(const gpu_unai_t &gpu_unai, u16 *pDst, u32 count)
{
	const u32 full_mask = ((255u << FIXED_BITS) | fixed_LOMASK);
	if (!count)
		return true;
	if (gpu_unai.u_msk != full_mask || gpu_unai.v_msk != full_mask ||
		gpu_unai.v_inc != 0)
		return false;

	const u32 l_u = gpu_unai.u & full_mask;
	const s32 u_inc = gpu_unai.u_inc;
	const bool unit_u = (u_inc == (1 << FIXED_BITS));
	if (unit_u) {
		const u32 tu = l_u >> FIXED_BITS;
		if (tu >= 256u || count > 256u - tu)
			return false;
	} else if (count > 1) {
		const u32 steps = count - 1;
		if (u_inc > 0) {
			const unsigned long long distance =
				(unsigned long long)steps * (u32)u_inc;
			if (distance > (unsigned long long)(full_mask - l_u))
				return false;
		} else if (u_inc < 0) {
			const u32 magnitude = (u32)(-(u_inc + 1)) + 1u;
			const unsigned long long distance =
				(unsigned long long)steps * magnitude;
			if (distance > (unsigned long long)l_u)
				return false;
		}
	}

	const u8 *row = ((const u8 *)gpu_unai.TBA) +
				(((gpu_unai.v & full_mask) >> FIXED_BITS) << 11);
	#if !QPSX_GPU_4BPP_PALETTE_LUT
	const u16 *cba = gpu_unai.CBA;
	#endif
	u32 tex_u = l_u;
#if QPSX_GPU_4BPP_PALETTE_LUT
	qpsx_gpu_prepare_4bpp_palette_lut(gpu_unai);
	const u32 *cba4 = gpu_unai.CBA4Packed;
#endif
	if (unit_u) {
		u32 tu = l_u >> FIXED_BITS;
		u8 *packed_row = (u8 *)row + (tu >> 1);
		if (tu & 1u) {
		#if QPSX_GPU_4BPP_PALETTE_LUT
			const u32 pair = cba4[*packed_row];
			const u16 src = (u16)(pair >> 16);
		#else
			const u16 src = cba[*packed_row >> 4];
		#endif
			if (src)
				*pDst = src;
			++pDst;
			++packed_row;
			--count;
		}
		while (count >= 2) {
			const u8 packed = *packed_row++;
		#if QPSX_GPU_4BPP_PALETTE_LUT
			const u32 pair = cba4[packed];
			const u16 src0 = (u16)pair;
			const u16 src1 = (u16)(pair >> 16);
		#else
			const u16 src0 = cba[packed & 0xf];
			const u16 src1 = cba[packed >> 4];
		#endif
			if (src0)
				pDst[0] = src0;
			if (src1)
				pDst[1] = src1;
			pDst += 2;
			count -= 2;
		}
		if (count) {
		#if QPSX_GPU_4BPP_PALETTE_LUT
			const u16 src = (u16)cba4[*packed_row & 0xfu];
		#else
			const u16 src = cba[*packed_row & 0xf];
		#endif
			if (src)
				*pDst = src;
		}
		return true;
	}

	do {
		const u32 tu = tex_u >> FIXED_BITS;
	#if QPSX_GPU_4BPP_PALETTE_LUT
		const u32 pair = cba4[row[tu >> 1]];
		const u16 src = (u16)(pair >> ((tu & 1u) << 4));
	#else
		const u8 packed = row[tu >> 1];
		const u16 src = cba[(packed >> ((tu & 1) << 2)) & 0xf];
	#endif
		if (src)
			*pDst = src;
		++pDst;
		tex_u += u_inc;
	} while (--count);
	return true;
}
#undef QPSX_GPU_FLATV_NOINLINE
#endif

#if QPSX_GPU_4BPP_FLATV_ROW
/*
 * The previous CF=32 flat-V experiment required the full 256x256 texture
 * window and proved that U could not wrap.  That proof made the fast path
 * miss many of the measured flat-V pixels.  This variant keeps the same
 * exact texture-window semantics as the generic loop: it masks U once per
 * texel, but hoists the constant V row address out of the loop.  It is
 * intentionally out-of-line so the common renderer's I-cache footprint does
 * not grow on a 16 KiB MIPS cache.  No blend, mask, lighting, or Gouraud work
 * is present for CF=32, so a zero CLUT entry remains the only skipped pixel.
 */
#if defined(__GNUC__)
#define QPSX_GPU_FLATV_ROW_NOINLINE __attribute__((noinline))
#else
#define QPSX_GPU_FLATV_ROW_NOINLINE
#endif
static QPSX_GPU_FLATV_ROW_NOINLINE bool
qpsx_gpu_poly_span_4bpp_flatv_row(const gpu_unai_t &gpu_unai,
					  u16 *pDst, u32 count)
{
	if (!count || gpu_unai.v_inc != 0)
		return false;

	u32 l_u = gpu_unai.u & gpu_unai.u_msk;
	const u32 u_msk = gpu_unai.u_msk;
	const s32 u_inc = gpu_unai.u_inc;
	const u8 *row = ((const u8 *)gpu_unai.TBA) +
				(((gpu_unai.v & gpu_unai.v_msk) << 1) & (0xffu << 11));
	const u16 *cba = gpu_unai.CBA;

	/* The common unit-U case consumes two texels from one source byte.  Only
	 * use the paired loop when the texture-window mask cannot wrap during the
	 * span; the general loop below is exact for every other increment/window. */
	if (u_inc == (1 << FIXED_BITS)) {
		const u32 texel = l_u >> FIXED_BITS;
		const u32 max_texel = u_msk >> FIXED_BITS;
		if (texel <= max_texel && count <= max_texel - texel + 1u) {
			const u8 *packed_row = row + (texel >> 1);
			if (texel & 1u) {
				const u16 src = cba[*packed_row >> 4];
				if (src)
					*pDst = src;
				++pDst;
				++packed_row;
				--count;
			}
			while (count >= 2) {
				const u8 packed = *packed_row++;
				const u16 src0 = cba[packed & 0xf];
				const u16 src1 = cba[packed >> 4];
				if (src0)
					pDst[0] = src0;
				if (src1)
					pDst[1] = src1;
				pDst += 2;
				count -= 2;
			}
			if (count) {
				const u16 src = cba[*packed_row & 0xf];
				if (src)
					*pDst = src;
			}
			return true;
		}
	}

	do {
		const u32 tu = l_u >> FIXED_BITS;
		const u8 packed = row[tu >> 1];
		const u16 src = cba[(packed >> ((tu & 1u) << 2)) & 0xf];
		if (src)
			*pDst = src;
		++pDst;
		l_u = (l_u + u_inc) & u_msk;
	} while (--count);
	return true;
}
#undef QPSX_GPU_FLATV_ROW_NOINLINE
#endif

#if QPSX_GPU_4BPP_FULLMASK
/* Full texture windows are common in the measured scene.  When the fixed
 * point endpoints prove that neither coordinate wraps during a span, the
 * two per-pixel texture-window masks are redundant.  Keep this helper
 * out-of-line: the proof is cold, while the compact loop saves the masks and
 * the V-row address calculation from every CF=32 pixel. */
#if defined(__GNUC__)
#define QPSX_GPU_FULLMASK_NOINLINE __attribute__((noinline))
#else
#define QPSX_GPU_FULLMASK_NOINLINE
#endif
static QPSX_GPU_FULLMASK_NOINLINE bool
qpsx_gpu_poly_span_4bpp_fullmask(const gpu_unai_t &gpu_unai,
					 u16 *pDst, u32 count)
{
	const u32 full_mask = ((255u << FIXED_BITS) | fixed_LOMASK);
	const u32 u_inc = (u32)gpu_unai.u_inc;
	const u32 v_inc = (u32)gpu_unai.v_inc;
	u32 l_u;
	u32 l_v;

	if (!count || gpu_unai.u_msk != full_mask ||
		gpu_unai.v_msk != full_mask)
		return false;
	l_u = gpu_unai.u & full_mask;
	l_v = gpu_unai.v & full_mask;
	const bool flat_unit_u = (v_inc == 0 && u_inc == (1u << FIXED_BITS));
	if (flat_unit_u) {
		const u32 texel = l_u >> FIXED_BITS;
		if (texel >= 256u || count > 256u - texel)
			return false;
	} else if (count > 1) {
		const u32 steps = count - 1u;
		const s32 inc_u = (s32)u_inc;
		const s32 inc_v = (s32)v_inc;
		/* Compare the positive travel distance against the available
		 * endpoint room.  Unsigned products let MIPS use one 32x32
		 * multu per coordinate instead of the signed 64-bit endpoint
		 * arithmetic that this cold proof used originally. */
		if (inc_u > 0) {
			const uint64_t distance = (uint64_t)(u32)inc_u * steps;
			if (distance > (uint64_t)(full_mask - l_u))
				return false;
		} else if (inc_u < 0) {
			const uint64_t distance = (uint64_t)(0u - u_inc) * steps;
			if (distance > (uint64_t)l_u)
				return false;
		}
		if (inc_v > 0) {
			const uint64_t distance = (uint64_t)(u32)inc_v * steps;
			if (distance > (uint64_t)(full_mask - l_v))
				return false;
		} else if (inc_v < 0) {
			const uint64_t distance = (uint64_t)(0u - v_inc) * steps;
			if (distance > (uint64_t)l_v)
				return false;
		}
	}

	const u8 *texture = (const u8 *)gpu_unai.TBA;
	const u16 *cba = gpu_unai.CBA;
	/* Flat-V/unit-U spans are the best case: one texture row and two texels
	 * arrive in each source byte.  This is kept inside the out-of-line helper
	 * so the normal CF=32 driver does not grow on the target's tiny I-cache. */
	if (flat_unit_u) {
		const u32 tu = l_u >> FIXED_BITS;
		const u8 *packed_row = texture + ((l_v >> FIXED_BITS) << 11) + (tu >> 1);
		if (tu & 1u) {
			const u16 src = cba[*packed_row >> 4];
			if (src)
				*pDst = src;
			++pDst;
			++packed_row;
			--count;
		}
	#if QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES
		/* The CF=32 path has no blend, mask, or lighting side effects. Once
		 * the span is on a 32-bit boundary, two opaque RGB555 texels can be
		 * committed with one store. Keep transparent pairs on the exact
		 * per-pixel fallback because a zero CLUT entry must leave VRAM alone.
		 * The alignment peel is outside the pair loop so the MIPS target does
		 * not pay an alignment test for every two pixels. */
		if (((uintptr_t)pDst & 2u) && count) {
			const u16 src = cba[*packed_row & 0xfu];
			if (src)
				*pDst = src;
			++pDst;
			++packed_row;
			--count;
		}
	#endif
	#if QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES && QPSX_GPU_4BPP_FULLMASK_PACKED_UNROLL
		/* Once alignment and the odd-pixel tail are peeled, consume two
		 * source bytes at a time.  This keeps the same transparent-entry
		 * fallback as the pair kernel while cutting the loop branch/count
		 * update frequency in half.  The unrolled body lives in this cold
		 * helper, so it does not enlarge the CF=32 dispatch template. */
		while (count >= 4) {
			const u8 packed0 = *packed_row++;
			const u8 packed1 = *packed_row++;
			const u16 src00 = cba[packed0 & 0xfu];
			const u16 src01 = cba[packed0 >> 4];
			const u16 src10 = cba[packed1 & 0xfu];
			const u16 src11 = cba[packed1 >> 4];
			if (src00 && src01)
				*(u32 *)(void *)pDst = (u32)src00 | ((u32)src01 << 16);
			else {
				if (src00)
					pDst[0] = src00;
				if (src01)
					pDst[1] = src01;
			}
			if (src10 && src11)
				*(u32 *)(void *)(pDst + 2) = (u32)src10 | ((u32)src11 << 16);
			else {
				if (src10)
					pDst[2] = src10;
				if (src11)
					pDst[3] = src11;
			}
			pDst += 4;
			count -= 4;
		}
	#endif
		while (count >= 2) {
			const u8 packed = *packed_row++;
			const u16 src0 = cba[packed & 0xfu];
			const u16 src1 = cba[packed >> 4];
		#if QPSX_GPU_4BPP_FULLMASK_PACKED_WRITES
			if (src0 && src1)
				*(u32 *)(void *)pDst = (u32)src0 | ((u32)src1 << 16);
			else {
				if (src0)
					pDst[0] = src0;
				if (src1)
					pDst[1] = src1;
			}
		#else
			if (src0)
				pDst[0] = src0;
			if (src1)
				pDst[1] = src1;
		#endif
			pDst += 2;
			count -= 2;
		}
		if (count) {
			const u16 src = cba[*packed_row & 0xfu];
			if (src)
				*pDst = src;
		}
		return true;
	}
	if (v_inc == 0) {
		const u8 *row = texture + ((l_v >> FIXED_BITS) << 11);
		do {
			const u32 tu = l_u >> FIXED_BITS;
			const u8 packed = row[tu >> 1];
			const u16 src = cba[(packed >> ((tu & 1u) << 2)) & 0xfu];
			if (src)
				*pDst = src;
			++pDst;
			l_u += u_inc;
		} while (--count);
		return true;
	}
	do {
		const u32 tu = l_u >> FIXED_BITS;
		const u32 tv = l_v >> FIXED_BITS;
		const u8 packed = texture[(tv << 11) + (tu >> 1)];
		const u16 src = cba[(packed >> ((tu & 1u) << 2)) & 0xfu];
		if (src)
			*pDst = src;
		++pDst;
		l_u += u_inc;
		l_v += v_inc;
	} while (--count);
	return true;
}
#undef QPSX_GPU_FULLMASK_NOINLINE
#endif

#ifdef __arm__
#ifndef ENABLE_GPU_ARMV7
/* ARMv5 */
#include "gpu_inner_blend_arm5.h"
#else
/* ARMv7 optimized */
#include "gpu_inner_blend_arm7.h"
#endif
#else
#include "gpu_inner_blend.h"
#endif

#include "gpu_inner_quantization.h"
#include "gpu_inner_light.h"

// QPSX v091: MIPS32 Assembly optimizations
#include "gpu_inner_mips32.h"

#if QPSX_GPU_POLY_2043_FAST
/*
 * Compact specialization for the measured 16bpp Gouraud polygon driver.
 *
 * CF 2043 is: lighting + blending mode 3 + 16bpp texture + Gouraud + mask
 * set + dithering + display blit mask.  Keeping this as a separate function
 * is important on the HC15xx: the ordinary template remains small and this
 * large, but single-purpose loop does not replicate its runtime decisions.
 * Every operation below is copied from the CF=2043 arm of gpuPolySpanFn;
 * the only changed representation is carrying the already-known VRAM index
 * for dithering instead of subtracting two pointers on every pixel.
 */
#if defined(__GNUC__)
#define QPSX_GPU_POLY_2043_NOINLINE __attribute__((noinline))
#else
#define QPSX_GPU_POLY_2043_NOINLINE
#endif
static QPSX_GPU_POLY_2043_NOINLINE void
qpsx_gpu_poly_span_2043(const gpu_unai_t &gpu_unai, u16 *pDst, u32 count)
{
	u32 l_u = gpu_unai.u & gpu_unai.u_msk;
	u32 l_v = gpu_unai.v & gpu_unai.v_msk;
	const u32 l_u_msk = gpu_unai.u_msk;
	const u32 l_v_msk = gpu_unai.v_msk;
	const s32 l_u_inc = gpu_unai.u_inc;
	const s32 l_v_inc = gpu_unai.v_inc;
	const u16 *texture = gpu_unai.TBA;
	u32 l_gCol = gpu_unai.gCol;
	const u32 l_gInc = gpu_unai.gInc;
	const u8 blit_mask = gpu_unai.blit_mask;
	/* The original quantizer intentionally truncates the VRAM index to u16. */
	u16 fbpos = (u16)(u32)(pDst - gpu_unai.vram);
	uintptr_t dst_addr = (uintptr_t)pDst;
	if (!count)
		return;

	do {
		if ((blit_mask >> ((dst_addr >> 1) & 7)) & 1u)
			goto skip;

		{
			const u16 texel = texture[(l_u >> FIXED_BITS) +
							  (l_v & (0xffu << FIXED_BITS))];
			u16 src;
			u32 src24;

			if (!texel)
				goto skip;

			/* CF=2043 is always Gouraud + dither. */
			src = texel & 0x8000;
			src24 = gpuLightingTXT24Gouraud(texel, l_gCol);
			if (src) {
#if QPSX_GPU_FIXED_FAST_PATH
				src24 = gpuBlending24Fast_Mode3(src24, *pDst);
#else
				src24 = gpuBlending24<3>(src24, *pDst);
#endif
			}
			src = gpuColorQuantization24At<1>(src24, gpu_unai, fbpos);
			*pDst = src | 0x8000;
		}

skip:
		++pDst;
		dst_addr += sizeof(u16);
		fbpos = (u16)(fbpos + 1u);
		l_u = (l_u + l_u_inc) & l_u_msk;
		l_v = (l_v + l_v_inc) & l_v_msk;
		l_gCol += l_gInc;
	} while (--count);
}
#undef QPSX_GPU_POLY_2043_NOINLINE
#endif

#if QPSX_GPU_LINEAR_4BPP
/*
 * A large Ridge Racer workload uses the unlit 4bpp polygon driver (CF=32).
 * When a span walks one texel at a time along a constant texture row, two
 * texels share one source byte.  The normal loop recomputes the fixed-point
 * address and reloads that byte for every pixel.  This helper handles only
 * the provable linear/no-wrap case and returns false for every other texture
 * window, so correctness falls back to the normal renderer.
 */
static inline bool qpsx_gpu_poly_span_4bpp_linear(const gpu_unai_t &gpu_unai,
                                                  u16 *pDst, u32 count)
{
	const u32 full_mask = ((255u << FIXED_BITS) | fixed_LOMASK);
	const s32 unit_u = (1 << FIXED_BITS);
	const u32 l_u = gpu_unai.u & gpu_unai.u_msk;
	const u32 l_v = gpu_unai.v & gpu_unai.v_msk;
	if (gpu_unai.u_msk != full_mask || gpu_unai.v_msk != full_mask ||
		gpu_unai.u_inc != unit_u || gpu_unai.v_inc != 0 ||
		(l_u >> FIXED_BITS) + count > 256)
		return false;

	const u8 *row = ((const u8 *)gpu_unai.TBA) +
				((l_v << 1) & (0xffu << 11));
	const u16 *cba = gpu_unai.CBA;
	u32 tu = l_u >> FIXED_BITS;

	/* Consume an odd starting texel before the paired byte loop. */
	if (count && (tu & 1)) {
		u16 src = cba[*row >> 4];
		if (src) *pDst = src;
		++pDst;
		++tu;
		--count;
		++row;
	}

	while (count >= 2) {
		const u8 packed = *row++;
		u16 src = cba[packed & 0x0f];
		if (src) pDst[0] = src;
		src = cba[packed >> 4];
		if (src) pDst[1] = src;
		pDst += 2;
		tu += 2;
		count -= 2;
	}
	if (count) {
		u16 src = cba[*row & 0x0f];
		if (src) *pDst = src;
	}
	return true;
}
#endif

#if QPSX_GPU_4BPP_GOURAUD_FLATV
#if defined(__GNUC__)
#define QPSX_GPU_GOURAUD_FLATV_NOINLINE __attribute__((noinline))
#else
#define QPSX_GPU_GOURAUD_FLATV_NOINLINE
#endif
#if QPSX_GPU_4BPP_GOURAUD_CACHE
/* The fast Gouraud operation only consumes these three quantized 5-bit
 * light values from gCol. Cache the sixteen CLUT results while that key is
 * unchanged. The cache is deliberately a 32-byte span-local object rather
 * than a 1 KiB global table: this keeps the working set friendly to the
 * SF2000's tiny D-cache and avoids rebuilding a full table for short spans. */
static inline u32 qpsx_gpu_gouraud_light_key(u32 gCol)
{
	return (gCol >> 27) |
	       (((gCol >> 16) & 0x1fu) << 5) |
	       (((gCol >> 5) & 0x1fu) << 10);
}

static inline u16 qpsx_gpu_gouraud_cache_color(u16 src, u32 index,
						 u32 gCol, u16 *palette,
						 u32 *valid_mask)
{
	const u32 bit = 1u << index;
	if (!(*valid_mask & bit)) {
		palette[index] = src ?
			(gpuLightingTXTGouraud_Fast(src, gCol) | (src & 0x8000)) : 0;
		*valid_mask |= bit;
	}
	return palette[index];
}
#endif
/* CF=161 is the measured lit 4bpp driver.  Accept only a full 256x256
 * window, constant V, unit U, and a span proven not to wrap. */
static QPSX_GPU_GOURAUD_FLATV_NOINLINE bool
qpsx_gpu_poly_span_4bpp_gouraud_flatv(const gpu_unai_t &gpu_unai,
						      u16 *pDst, u32 count)
{
	const u32 full_mask = ((255u << FIXED_BITS) | fixed_LOMASK);
	if (!count || gpu_unai.u_msk != full_mask ||
	    gpu_unai.v_msk != full_mask || gpu_unai.v_inc != 0 ||
	    gpu_unai.u_inc != (1 << FIXED_BITS))
		return false;
	const u32 texel = (gpu_unai.u & full_mask) >> FIXED_BITS;
	if (texel >= 256u || count > 256u - texel)
		return false;

	const u8 *row = ((const u8 *)gpu_unai.TBA) +
				(((gpu_unai.v & full_mask) >> FIXED_BITS) << 11);
	const u16 *cba = gpu_unai.CBA;
	u32 l_gCol = gpu_unai.gCol;
	const u32 l_gInc = gpu_unai.gInc;
#if QPSX_GPU_4BPP_GOURAUD_CACHE
	/* This look-ahead is only a profitability gate. Every lookup below still
	 * checks the exact quantized key, so a crossing at any later pixel remains
	 * pixel-identical to the direct fast-lighting path. */
	const u32 first_key = qpsx_gpu_gouraud_light_key(l_gCol);
	const u32 probe_key = qpsx_gpu_gouraud_light_key(l_gCol + (l_gInc << 2));
	if (first_key == probe_key) {
		u16 palette[16];
		u32 valid_mask = 0;
		u32 cache_key = ~0u;
		if (texel & 1u) {
			const u32 key = qpsx_gpu_gouraud_light_key(l_gCol);
			if (key != cache_key) {
				cache_key = key;
				valid_mask = 0;
			}
			const u32 index = *row++ >> 4;
			const u16 src = cba[index];
			if (src)
				*pDst = qpsx_gpu_gouraud_cache_color(src, index, l_gCol,
								     palette, &valid_mask);
			++pDst;
			l_gCol += l_gInc;
			--count;
		}
		const bool pair_aligned = !((uintptr_t)pDst & 2u);
		if (pair_aligned) {
			while (count >= 2) {
				const u8 packed = *row++;
				const u32 key0 = qpsx_gpu_gouraud_light_key(l_gCol);
				if (key0 != cache_key) {
					cache_key = key0;
					valid_mask = 0;
				}
				const u32 index0 = packed & 0xfu;
				const u16 src0 = cba[index0];
				u16 out0 = 0;
				if (src0)
					out0 = qpsx_gpu_gouraud_cache_color(src0, index0, l_gCol,
									       palette, &valid_mask);
				l_gCol += l_gInc;
				const u32 key1 = qpsx_gpu_gouraud_light_key(l_gCol);
				if (key1 != cache_key) {
					cache_key = key1;
					valid_mask = 0;
				}
				const u32 index1 = packed >> 4;
				const u16 src1 = cba[index1];
				u16 out1 = 0;
				if (src1)
					out1 = qpsx_gpu_gouraud_cache_color(src1, index1, l_gCol,
									       palette, &valid_mask);
				l_gCol += l_gInc;
				if (src0 && src1)
					*(u32 *)pDst = (u32)out0 | ((u32)out1 << 16);
				else {
					if (src0) pDst[0] = out0;
					if (src1) pDst[1] = out1;
				}
				pDst += 2;
				count -= 2;
			}
		} else {
			while (count >= 2) {
				const u8 packed = *row++;
				const u32 key0 = qpsx_gpu_gouraud_light_key(l_gCol);
				if (key0 != cache_key) {
					cache_key = key0;
					valid_mask = 0;
				}
				const u32 index0 = packed & 0xfu;
				const u16 src0 = cba[index0];
				if (src0)
					pDst[0] = qpsx_gpu_gouraud_cache_color(src0, index0, l_gCol,
									      palette, &valid_mask);
				l_gCol += l_gInc;
				const u32 key1 = qpsx_gpu_gouraud_light_key(l_gCol);
				if (key1 != cache_key) {
					cache_key = key1;
					valid_mask = 0;
				}
				const u32 index1 = packed >> 4;
				const u16 src1 = cba[index1];
				if (src1)
					pDst[1] = qpsx_gpu_gouraud_cache_color(src1, index1, l_gCol,
									      palette, &valid_mask);
				l_gCol += l_gInc;
				pDst += 2;
				count -= 2;
			}
		}
		if (count) {
			const u32 key = qpsx_gpu_gouraud_light_key(l_gCol);
			if (key != cache_key) {
				cache_key = key;
				valid_mask = 0;
			}
			const u32 index = *row & 0xfu;
			const u16 src = cba[index];
			if (src)
				*pDst = qpsx_gpu_gouraud_cache_color(src, index, l_gCol,
								     palette, &valid_mask);
		}
		return true;
	}
#endif
	if (texel & 1u) {
		const u16 src = cba[*row++ >> 4];
		if (src)
			*pDst = gpuLightingTXTGouraud_Fast(src, l_gCol) |
				(src & 0x8000);
		++pDst;
		l_gCol += l_gInc;
		--count;
	}

	const bool pair_aligned = !((uintptr_t)pDst & 2u);
	if (pair_aligned) {
		while (count >= 2) {
			const u8 packed = *row++;
			const u16 src0 = cba[packed & 0xf];
			const u16 src1 = cba[packed >> 4];
			u16 out0 = 0, out1 = 0;
			if (src0)
				out0 = gpuLightingTXTGouraud_Fast(src0, l_gCol) |
					(src0 & 0x8000);
			l_gCol += l_gInc;
			if (src1)
				out1 = gpuLightingTXTGouraud_Fast(src1, l_gCol) |
					(src1 & 0x8000);
			l_gCol += l_gInc;
			if (src0 && src1)
				*(u32 *)pDst = (u32)out0 | ((u32)out1 << 16);
			else {
				if (src0) pDst[0] = out0;
				if (src1) pDst[1] = out1;
			}
			pDst += 2;
			count -= 2;
		}
	} else {
		while (count >= 2) {
			const u8 packed = *row++;
			const u16 src0 = cba[packed & 0xf];
			const u16 src1 = cba[packed >> 4];
			if (src0)
				pDst[0] = gpuLightingTXTGouraud_Fast(src0, l_gCol) |
					(src0 & 0x8000);
			l_gCol += l_gInc;
			if (src1)
				pDst[1] = gpuLightingTXTGouraud_Fast(src1, l_gCol) |
					(src1 & 0x8000);
			l_gCol += l_gInc;
			pDst += 2;
			count -= 2;
		}
	}
	if (count) {
		const u16 src = cba[*row & 0xf];
		if (src)
			*pDst = gpuLightingTXTGouraud_Fast(src, l_gCol) |
				(src & 0x8000);
	}
	return true;
}
#undef QPSX_GPU_GOURAUD_FLATV_NOINLINE
#endif

#if QPSX_GPU_PACKED_POLY_WRITES
#if defined(__GNUC__)
#define QPSX_GPU_POLY_PACK_NOINLINE __attribute__((noinline))
#else
#define QPSX_GPU_POLY_PACK_NOINLINE
#endif
static QPSX_GPU_POLY_PACK_NOINLINE void
qpsx_gpu_fill_flat_poly(u16 *pDst, u32 count, u16 data)
{
	if (count && ((uintptr_t)pDst & 2u)) {
		*pDst++ = data;
		--count;
	}
	const u32 packed = (u32)data | ((u32)data << 16);
	u32 *pDst32 = (u32 *)pDst;
	while (count >= 8) {
		pDst32[0] = packed;
		pDst32[1] = packed;
		pDst32[2] = packed;
		pDst32[3] = packed;
		pDst32 += 4;
		count -= 8;
	}
	while (count >= 2) {
		*pDst32++ = packed;
		count -= 2;
	}
	if (count)
		*(u16 *)pDst32 = data;
}
#undef QPSX_GPU_POLY_PACK_NOINLINE
#endif

// If defined, Gouraud colors are fixed-point 5.11, otherwise they are 8.16
// This is only for debugging/verification of low-precision colors in C.
// Low-precision Gouraud is intended for use by SIMD-optimized inner drivers
// which get/use Gouraud colors in SIMD registers.
//
// QPSX v089: Enable for SF2000 - fewer bits = faster math, minor visual difference
#if defined(SF2000) || defined(__mips__)
#define GPU_GOURAUD_LOW_PRECISION
#endif

/* The flat-line shortcut is exact only with the five-bit channel
 * quantization used by the SF2000/MIPS build. Keep an experimental request
 * disabled on desktop/high-precision builds rather than changing rendering
 * semantics there. */
#if QPSX_GPU_GOURAUD_LINE_FLATFAST && defined(GPU_GOURAUD_LOW_PRECISION)
#define QPSX_GPU_GOURAUD_LINE_FLATFAST_ACTIVE 1
#else
#define QPSX_GPU_GOURAUD_LINE_FLATFAST_ACTIVE 0
#endif

// How many bits of fixed-point precision GouraudColor uses
#ifdef GPU_GOURAUD_LOW_PRECISION
#define GPU_GOURAUD_FIXED_BITS 11
#else
#define GPU_GOURAUD_FIXED_BITS 16
#endif

// Used to pass Gouraud colors to gpuPixelSpanFn() (lines)
struct GouraudColor {
#ifdef GPU_GOURAUD_LOW_PRECISION
	u16 r, g, b;
	s16 r_incr, g_incr, b_incr;
#else
	u32 r, g, b;
	s32 r_incr, g_incr, b_incr;
#endif
};

static inline u16 gpuGouraudColor15bpp(u32 r, u32 g, u32 b)
{
	r >>= GPU_GOURAUD_FIXED_BITS;
	g >>= GPU_GOURAUD_FIXED_BITS;
	b >>= GPU_GOURAUD_FIXED_BITS;

#ifndef GPU_GOURAUD_LOW_PRECISION
	// High-precision Gouraud colors are 8-bit + fractional
	r >>= 3;  g >>= 3;  b >>= 3;
#endif

	return r | (g << 5) | (b << 10);
}

///////////////////////////////////////////////////////////////////////////////
//  GPU Pixel span operations generator gpuPixelSpanFn<>
//  Oct 2016: Created/adapted from old gpuPixelFn by senquack:
//  Original gpuPixelFn was used to draw lines one pixel at a time. I wrote
//  new line algorithms that draw lines using horizontal/vertical/diagonal
//  spans of pixels, necessitating new pixel-drawing function that could
//  not only render spans of pixels, but gouraud-shade them as well.
//  This speeds up line rendering and would allow tile-rendering (untextured
//  rectangles) to use the same set of functions. Since tiles are always
//  monochrome, they simply wouldn't use the extra set of 32 gouraud-shaded
//  gpuPixelSpanFn functions (TODO?).
//
// NOTE: While the PS1 framebuffer is 16 bit, we use 8-bit pointers here,
//       so that pDst can be incremented directly by 'incr' parameter
//       without having to shift it before use.
template<int CF>
static u8* gpuPixelSpanFn(u8* pDst, uintptr_t data, ptrdiff_t incr, size_t len)
{
	// Blend func can save an operation if it knows uSrc MSB is
	//  unset. For untextured prims, this is always true.
	const bool skip_uSrc_mask = true;

	u16 col;
	struct GouraudColor * gcPtr;
	u32 r, g, b;
	s32 r_incr, g_incr, b_incr;

	if (CF_GOURAUD) {
		gcPtr = (GouraudColor*)data;
		r = gcPtr->r;  r_incr = gcPtr->r_incr;
		g = gcPtr->g;  g_incr = gcPtr->g_incr;
		b = gcPtr->b;  b_incr = gcPtr->b_incr;
	} else {
		col = (u16)data;
	}

	do {
		if (!CF_GOURAUD)
		{   // NO GOURAUD
			if (!CF_MASKCHECK && !CF_BLEND) {
				if (CF_MASKSET) { *(u16*)pDst = col | 0x8000; }
				else            { *(u16*)pDst = col;          }
			} else if (CF_MASKCHECK && !CF_BLEND) {
				if (!(*(u16*)pDst & 0x8000)) {
					if (CF_MASKSET) { *(u16*)pDst = col | 0x8000; }
					else            { *(u16*)pDst = col;          }
				}
			} else {
				u16 uDst = *(u16*)pDst;
				if (CF_MASKCHECK) { if (uDst & 0x8000) goto endpixel; }

				u16 uSrc = col;

				if (CF_BLEND) {
					// QPSX_090: Use fast blending if enabled
					if (FastBlendingEnabled())
						uSrc = gpuBlending_Fast<CF_BLENDMODE>(uSrc, uDst);
					else
						uSrc = gpuBlending<CF_BLENDMODE, skip_uSrc_mask>(uSrc, uDst);
				}

				if (CF_MASKSET) { *(u16*)pDst = uSrc | 0x8000; }
				else            { *(u16*)pDst = uSrc;          }
			}

		} else
		{   // GOURAUD

			if (!CF_MASKCHECK && !CF_BLEND) {
				col = gpuGouraudColor15bpp(r, g, b);
				if (CF_MASKSET) { *(u16*)pDst = col | 0x8000; }
				else            { *(u16*)pDst = col;          }
			} else if (CF_MASKCHECK && !CF_BLEND) {
				col = gpuGouraudColor15bpp(r, g, b);
				if (!(*(u16*)pDst & 0x8000)) {
					if (CF_MASKSET) { *(u16*)pDst = col | 0x8000; }
					else            { *(u16*)pDst = col;          }
				}
			} else {
				u16 uDst = *(u16*)pDst;
				if (CF_MASKCHECK) { if (uDst & 0x8000) goto endpixel; }
				col = gpuGouraudColor15bpp(r, g, b);

				u16 uSrc = col;

				// Blend func can save an operation if it knows uSrc MSB is
				//  unset. For untextured prims, this is always true.
				const bool skip_uSrc_mask = true;

				if (CF_BLEND) {
					// QPSX_090: Use fast blending if enabled
					if (FastBlendingEnabled())
						uSrc = gpuBlending_Fast<CF_BLENDMODE>(uSrc, uDst);
					else
						uSrc = gpuBlending<CF_BLENDMODE, skip_uSrc_mask>(uSrc, uDst);
				}

				if (CF_MASKSET) { *(u16*)pDst = uSrc | 0x8000; }
				else            { *(u16*)pDst = uSrc;          }
			}
		}

endpixel:
		if (CF_GOURAUD) {
			r += r_incr;
			g += g_incr;
			b += b_incr;
		}
		pDst += incr;
	} while (len-- > 1);

	// Note from senquack: Normally, I'd prefer to write a 'do {} while (--len)'
	//  loop, or even a for() loop, however, on MIPS platforms anything but the
	//  'do {} while (len-- > 1)' tends to generate very unoptimal asm, with
	//  many unneeded MULs/ADDs/branches at the ends of these functions.
	//  If you change the loop structure above, be sure to compare the quality
	//  of the generated code!!

	if (CF_GOURAUD) {
		gcPtr->r = r;
		gcPtr->g = g;
		gcPtr->b = b;
	}
	return pDst;
}

static u8* PixelSpanNULL(u8* pDst, uintptr_t data, ptrdiff_t incr, size_t len)
{
	#ifdef ENABLE_GPU_LOG_SUPPORT
		fprintf(stdout,"PixelSpanNULL()\n");
	#endif
	return pDst;
}

///////////////////////////////////////////////////////////////////////////////
//  PixelSpan (lines) innerloops driver
typedef u8* (*PSD)(u8* dst, uintptr_t data, ptrdiff_t incr, size_t len);

const PSD gpuPixelSpanDrivers[64] =
{ 
	// Array index | 'CF' template field | Field value
	// ------------+---------------------+----------------
	// Bit 0       | CF_BLEND            | off (0), on (1)
	// Bit 1       | CF_MASKCHECK        | off (0), on (1)
	// Bit 3:2     | CF_BLENDMODE        | 0..3
	// Bit 4       | CF_MASKSET          | off (0), on (1)
	// Bit 5       | CF_GOURAUD          | off (0), on (1)
	//
	// NULL entries are ones for which blending is disabled and blend-mode
	//  field is non-zero, which is obviously invalid.

	// Flat-shaded
	gpuPixelSpanFn<0x00<<1>,         gpuPixelSpanFn<0x01<<1>,         gpuPixelSpanFn<0x02<<1>,         gpuPixelSpanFn<0x03<<1>,
	PixelSpanNULL,                   gpuPixelSpanFn<0x05<<1>,         PixelSpanNULL,                   gpuPixelSpanFn<0x07<<1>,
	PixelSpanNULL,                   gpuPixelSpanFn<0x09<<1>,         PixelSpanNULL,                   gpuPixelSpanFn<0x0B<<1>,
	PixelSpanNULL,                   gpuPixelSpanFn<0x0D<<1>,         PixelSpanNULL,                   gpuPixelSpanFn<0x0F<<1>,

	// Flat-shaded + PixelMSB (CF_MASKSET)
	gpuPixelSpanFn<(0x00<<1)|0x100>, gpuPixelSpanFn<(0x01<<1)|0x100>, gpuPixelSpanFn<(0x02<<1)|0x100>, gpuPixelSpanFn<(0x03<<1)|0x100>,
	PixelSpanNULL,                   gpuPixelSpanFn<(0x05<<1)|0x100>, PixelSpanNULL,                   gpuPixelSpanFn<(0x07<<1)|0x100>,
	PixelSpanNULL,                   gpuPixelSpanFn<(0x09<<1)|0x100>, PixelSpanNULL,                   gpuPixelSpanFn<(0x0B<<1)|0x100>,
	PixelSpanNULL,                   gpuPixelSpanFn<(0x0D<<1)|0x100>, PixelSpanNULL,                   gpuPixelSpanFn<(0x0F<<1)|0x100>,

	// Gouraud-shaded (CF_GOURAUD)
	gpuPixelSpanFn<(0x00<<1)|0x80>,  gpuPixelSpanFn<(0x01<<1)|0x80>,  gpuPixelSpanFn<(0x02<<1)|0x80>,  gpuPixelSpanFn<(0x03<<1)|0x80>,
	PixelSpanNULL,                   gpuPixelSpanFn<(0x05<<1)|0x80>,  PixelSpanNULL,                   gpuPixelSpanFn<(0x07<<1)|0x80>,
	PixelSpanNULL,                   gpuPixelSpanFn<(0x09<<1)|0x80>,  PixelSpanNULL,                   gpuPixelSpanFn<(0x0B<<1)|0x80>,
	PixelSpanNULL,                   gpuPixelSpanFn<(0x0D<<1)|0x80>,  PixelSpanNULL,                   gpuPixelSpanFn<(0x0F<<1)|0x80>,

	// Gouraud-shaded (CF_GOURAUD) + PixelMSB (CF_MASKSET)
	gpuPixelSpanFn<(0x00<<1)|0x180>, gpuPixelSpanFn<(0x01<<1)|0x180>, gpuPixelSpanFn<(0x02<<1)|0x180>, gpuPixelSpanFn<(0x03<<1)|0x180>,
	PixelSpanNULL,                   gpuPixelSpanFn<(0x05<<1)|0x180>, PixelSpanNULL,                   gpuPixelSpanFn<(0x07<<1)|0x180>,
	PixelSpanNULL,                   gpuPixelSpanFn<(0x09<<1)|0x180>, PixelSpanNULL,                   gpuPixelSpanFn<(0x0B<<1)|0x180>,
	PixelSpanNULL,                   gpuPixelSpanFn<(0x0D<<1)|0x180>, PixelSpanNULL,                   gpuPixelSpanFn<(0x0F<<1)|0x180>
};

///////////////////////////////////////////////////////////////////////////////
//  GPU Tiles innerloops generator

template<int CF>
static void gpuTileSpanFn(u16 *pDst, u32 count, u16 data)
{
#if QPSX_GPU_RUNTIME_METRICS
	qpsx_gpu_tile_pixel_hist[CF] += count;
#endif
	if (!CF_MASKCHECK && !CF_BLEND) {
		if (CF_MASKSET) { data = data | 0x8000; }
		/* CF=0 is the overwhelmingly common opaque tile fill.  The normal
		 * eight-halfword unroll still spends one store instruction per pixel.
		 * On the little-endian SF2000 framebuffer, align once and write two
		 * identical pixels with one 32-bit store.  The odd-pixel prefix/suffix
		 * keeps this exact for every x alignment and the option is disabled for
		 * other ports by default. */
#if QPSX_GPU_PACKED_TILE_WRITES
		if (CF == 0 && !CF_MASKSET) {
			if (count && ((uintptr_t)pDst & 2u)) {
				*pDst++ = data;
				--count;
			}
			const u32 packed = (u32)data | ((u32)data << 16);
			u32 *pDst32 = (u32 *)pDst;
			/* Keep the branch rate of the old eight-pixel unroll while
			 * retaining the two-pixels-per-store reduction. */
			while (count >= 8) {
				pDst32[0] = packed;
				pDst32[1] = packed;
				pDst32[2] = packed;
				pDst32[3] = packed;
				pDst32 += 4;
				count -= 8;
			}
			while (count >= 2) {
				*pDst32++ = packed;
				count -= 2;
			}
			pDst = (u16 *)pDst32;
			if (count) *pDst = data;
			return;
		}
#endif
		// QPSX v089: 8x loop unroll for tile fills - significant speedup
		while (count >= 8) {
			pDst[0] = data; pDst[1] = data;
			pDst[2] = data; pDst[3] = data;
			pDst[4] = data; pDst[5] = data;
			pDst[6] = data; pDst[7] = data;
			pDst += 8; count -= 8;
		}
		while (count--) { *pDst++ = data; }
	} else if (CF_MASKCHECK && !CF_BLEND) {
		if (CF_MASKSET) { data = data | 0x8000; }
		do { if (!(*pDst&0x8000)) { *pDst = data; } pDst++; } while (--count);
	} else
	{
		// Blend func can save an operation if it knows uSrc MSB is
		//  unset. For untextured prims, this is always true.
		const bool skip_uSrc_mask = true;

		u16 uSrc, uDst;
		do
		{
			if (CF_MASKCHECK || CF_BLEND) { uDst = *pDst; }
			if (CF_MASKCHECK) { if (uDst&0x8000) goto endtile; }

			uSrc = data;

			if (CF_BLEND) {
				// QPSX_090: Use fast blending if enabled
				if (FastBlendingEnabled())
					uSrc = gpuBlending_Fast<CF_BLENDMODE>(uSrc, uDst);
				else
					uSrc = gpuBlending<CF_BLENDMODE, skip_uSrc_mask>(uSrc, uDst);
			}

			if (CF_MASKSET) { *pDst = uSrc | 0x8000; }
			else            { *pDst = uSrc;          }

			//senquack - Did not apply "Silent Hill" mask-bit fix to here.
			// It is hard to tell from scarce documentation available and
			//  lack of comments in code, but I believe the tile-span
			//  functions here should not bother to preserve any source MSB,
			//  as they are not drawing from a texture.
endtile:
			pDst++;
		}
		while (--count);
	}
}

static void TileNULL(u16 *pDst, u32 count, u16 data)
{
	#ifdef ENABLE_GPU_LOG_SUPPORT
		fprintf(stdout,"TileNULL()\n");
	#endif
}

///////////////////////////////////////////////////////////////////////////////
//  Tiles innerloops driver
typedef void (*PT)(u16 *pDst, u32 count, u16 data);

// Template instantiation helper macros
#define TI(cf) gpuTileSpanFn<(cf)>
#define TN     TileNULL
#define TIBLOCK(ub) \
	TI((ub)|0x00), TI((ub)|0x02), TI((ub)|0x04), TI((ub)|0x06), \
	TN,            TI((ub)|0x0a), TN,            TI((ub)|0x0e), \
	TN,            TI((ub)|0x12), TN,            TI((ub)|0x16), \
	TN,            TI((ub)|0x1a), TN,            TI((ub)|0x1e)

const PT gpuTileSpanDrivers[32] = {
	TIBLOCK(0<<8), TIBLOCK(1<<8)
};

#undef TI
#undef TN
#undef TIBLOCK


///////////////////////////////////////////////////////////////////////////////
//  GPU Sprites innerloops generator

template<int CF>
static void gpuSpriteSpanFn(u16 *pDst, u32 count, u8* pTxt, u32 u0)
{
#if QPSX_GPU_RUNTIME_METRICS
	qpsx_gpu_sprite_pixel_hist[CF] += count;
#endif
	// Blend func can save an operation if it knows uSrc MSB is unset.
	//  Untextured prims can always skip (source color always comes with MSB=0).
	//  For textured prims, lighting funcs always return it unset. (bonus!)
	const bool skip_uSrc_mask = (!CF_TEXTMODE) || CF_LIGHT;

	u16 uSrc, uDst, srcMSB;
	u32 u0_mask = gpu_unai.TextureWindow[2];

	u8 r5, g5, b5;
	if (CF_LIGHT) {
		r5 = gpu_unai.r5;
		g5 = gpu_unai.g5;
		b5 = gpu_unai.b5;
	}

	if (CF_TEXTMODE==3) {
		// Texture is accessed byte-wise, so adjust mask if 16bpp
		u0_mask <<= 1;
	}

	const u16 *CBA_; if (CF_TEXTMODE!=3) CBA_ = gpu_unai.CBA;

#if QPSX_GPU_PACKED_SPRITE_4BPP
	/* Most sprites in the Ridge Racer scene are opaque, unlit 4bpp (CF=32).
	 * With the default texture window, two adjacent texels share one byte.
	 * Handle only a complete, non-wrapping run so transparent texels and all
	 * texture-window corner cases retain the generic renderer's behavior. */
	if (CF == 0x20 && u0_mask == 255u && u0 <= 255u &&
		count <= 256u - u0) {
	#if QPSX_GPU_4BPP_PALETTE_LUT
		qpsx_gpu_prepare_4bpp_palette_lut(gpu_unai);
		const u32 *CBA4_ = gpu_unai.CBA4Packed;
	#endif
		u32 tu = u0;
		if (tu & 1u) {
			const u8 packed = pTxt[tu >> 1];
		#if QPSX_GPU_4BPP_PALETTE_LUT
			const u16 src = (u16)(CBA4_[packed] >> 16);
		#else
			const u16 src = CBA_[packed >> 4];
		#endif
			if (src) *pDst = src;
			++pDst;
			++tu;
			--count;
		}
		while (count >= 2) {
			const u8 packed = pTxt[tu >> 1];
		#if QPSX_GPU_4BPP_PALETTE_LUT
			const u32 pair = CBA4_[packed];
			const u16 src0 = (u16)pair;
			const u16 src1 = (u16)(pair >> 16);
		#else
			const u16 src0 = CBA_[packed & 0x0f];
			const u16 src1 = CBA_[packed >> 4];
		#endif
			if (src0) pDst[0] = src0;
			if (src1) pDst[1] = src1;
			pDst += 2;
			tu += 2;
			count -= 2;
		}
		if (count) {
			const u8 packed = pTxt[tu >> 1];
		#if QPSX_GPU_4BPP_PALETTE_LUT
			const u16 src = (u16)CBA4_[packed];
		#else
			const u16 src = CBA_[packed & 0x0f];
		#endif
			if (src) *pDst = src;
		}
		return;
	}
#endif

	do
	{
		if (CF_MASKCHECK || CF_BLEND) { uDst = *pDst; }
		if (CF_MASKCHECK) if (uDst&0x8000) { goto endsprite; }

		if (CF_TEXTMODE==1) {  //  4bpp (CLUT)
			u8 rgb = pTxt[(u0 & u0_mask)>>1];
			uSrc = CBA_[(rgb>>((u0&1)<<2))&0xf];
		}
		if (CF_TEXTMODE==2) {  //  8bpp (CLUT)
			uSrc = CBA_[pTxt[u0 & u0_mask]];
		}
		if (CF_TEXTMODE==3) {  // 16bpp
			uSrc = *(u16*)(&pTxt[u0 & u0_mask]);
		}

		if (!uSrc) goto endsprite;

		//senquack - save source MSB, as blending or lighting macros will not
		//           (Silent Hill gray rectangles mask bit bug)
		if (CF_BLEND || CF_LIGHT) srcMSB = uSrc & 0x8000;
		
		if (CF_LIGHT) {
			// QPSX_091: Priority: ASM > Fast > LUT
			if (AsmLightingEnabled())
				uSrc = gpuLightingTXT_ASM(uSrc, r5, g5, b5);
			else if (FastLightingEnabled())
				uSrc = gpuLightingTXT_Fast(uSrc, r5, g5, b5);
			else
				uSrc = gpuLightingTXT(uSrc, r5, g5, b5);
		}

		if (CF_BLEND && srcMSB) {
			// QPSX_091: Priority: ASM > Fast > Original
			if (AsmBlendingEnabled())
				uSrc = gpuBlending_ASM<CF_BLENDMODE>(uSrc, uDst);
			else if (FastBlendingEnabled())
				uSrc = gpuBlending_Fast<CF_BLENDMODE>(uSrc, uDst);
			else
				uSrc = gpuBlending<CF_BLENDMODE, skip_uSrc_mask>(uSrc, uDst);
		}

		if (CF_MASKSET)                { *pDst = uSrc | 0x8000; }
		else if (CF_BLEND || CF_LIGHT) { *pDst = uSrc | srcMSB; }
		else                           { *pDst = uSrc;          }

endsprite:
		u0 += (CF_TEXTMODE==3) ? 2 : 1;
		pDst++;
	}
	while (--count);
}

static void SpriteNULL(u16 *pDst, u32 count, u8* pTxt, u32 u0)
{
	#ifdef ENABLE_GPU_LOG_SUPPORT
		fprintf(stdout,"SpriteNULL()\n");
	#endif
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Sprite innerloops driver
typedef void (*PS)(u16 *pDst, u32 count, u8* pTxt, u32 u0);

// Template instantiation helper macros
#define TI(cf) gpuSpriteSpanFn<(cf)>
#define TN     SpriteNULL
#define TIBLOCK(ub) \
	TN,            TN,            TN,            TN,            TN,            TN,            TN,            TN,            \
	TN,            TN,            TN,            TN,            TN,            TN,            TN,            TN,            \
	TN,            TN,            TN,            TN,            TN,            TN,            TN,            TN,            \
	TN,            TN,            TN,            TN,            TN,            TN,            TN,            TN,            \
	TI((ub)|0x20), TI((ub)|0x21), TI((ub)|0x22), TI((ub)|0x23), TI((ub)|0x24), TI((ub)|0x25), TI((ub)|0x26), TI((ub)|0x27), \
	TN,            TN,            TI((ub)|0x2a), TI((ub)|0x2b), TN,            TN,            TI((ub)|0x2e), TI((ub)|0x2f), \
	TN,            TN,            TI((ub)|0x32), TI((ub)|0x33), TN,            TN,            TI((ub)|0x36), TI((ub)|0x37), \
	TN,            TN,            TI((ub)|0x3a), TI((ub)|0x3b), TN,            TN,            TI((ub)|0x3e), TI((ub)|0x3f), \
	TI((ub)|0x40), TI((ub)|0x41), TI((ub)|0x42), TI((ub)|0x43), TI((ub)|0x44), TI((ub)|0x45), TI((ub)|0x46), TI((ub)|0x47), \
	TN,            TN,            TI((ub)|0x4a), TI((ub)|0x4b), TN,            TN,            TI((ub)|0x4e), TI((ub)|0x4f), \
	TN,            TN,            TI((ub)|0x52), TI((ub)|0x53), TN,            TN,            TI((ub)|0x56), TI((ub)|0x57), \
	TN,            TN,            TI((ub)|0x5a), TI((ub)|0x5b), TN,            TN,            TI((ub)|0x5e), TI((ub)|0x5f), \
	TI((ub)|0x60), TI((ub)|0x61), TI((ub)|0x62), TI((ub)|0x63), TI((ub)|0x64), TI((ub)|0x65), TI((ub)|0x66), TI((ub)|0x67), \
	TN,            TN,            TI((ub)|0x6a), TI((ub)|0x6b), TN,            TN,            TI((ub)|0x6e), TI((ub)|0x6f), \
	TN,            TN,            TI((ub)|0x72), TI((ub)|0x73), TN,            TN,            TI((ub)|0x76), TI((ub)|0x77), \
	TN,            TN,            TI((ub)|0x7a), TI((ub)|0x7b), TN,            TN,            TI((ub)|0x7e), TI((ub)|0x7f)

const PS gpuSpriteSpanDrivers[256] = {
	TIBLOCK(0<<8), TIBLOCK(1<<8)
};

#undef TI
#undef TN
#undef TIBLOCK

///////////////////////////////////////////////////////////////////////////////
//  GPU Polygon innerloops generator

//senquack - Newer version with following changes:
//           * Adapted to work with new poly routings in gpu_raster_polygon.h
//             adapted from DrHell GPU. They are less glitchy and use 22.10
//             fixed-point instead of original UNAI's 16.16.
//           * Texture coordinates are no longer packed together into one
//             unsigned int. This seems to lose too much accuracy (they each
//             end up being only 8.7 fixed-point that way) and pixel-droupouts
//             were noticeable both with original code and current DrHell
//             adaptations. An example would be the sky in NFS3. Now, they are
//             stored in separate ints, using separate masks.
//           * Function is no longer INLINE, as it was always called
//             through a function pointer.
//           * Function now ensures the mask bit of source texture is preserved
//             across calls to blending functions (Silent Hill rectangles fix)
//           * November 2016: Large refactoring of blending/lighting when
//             JohnnyF added dithering. See gpu_inner_quantization.h and
//             relevant blend/light headers.
// (see README_senquack.txt)
template<int CF>
static void gpuPolySpanFn(const gpu_unai_t &gpu_unai, u16 *pDst, u32 count)
{
#if QPSX_GPU_RUNTIME_METRICS
	qpsx_gpu_poly_span_hist[CF]++;
	qpsx_gpu_poly_pixel_hist[CF] += count;
	if (CF_TEXTMODE &&
		gpu_unai.u_msk == ((255u << FIXED_BITS) | fixed_LOMASK) &&
		gpu_unai.v_msk == ((255u << FIXED_BITS) | fixed_LOMASK)) {
		++qpsx_gpu_poly_fullmask_spans;
		qpsx_gpu_poly_fullmask_pixels += count;
	}
	if (CF_TEXTMODE && gpu_unai.u_inc == (1 << FIXED_BITS)) {
		++qpsx_gpu_poly_unit_u_spans;
		qpsx_gpu_poly_unit_u_pixels += count;
	}
	if (CF_TEXTMODE && gpu_unai.v_inc == 0) {
		++qpsx_gpu_poly_flat_v_spans;
		qpsx_gpu_poly_flat_v_pixels += count;
	}
#endif
#if QPSX_GPU_4BPP_FLATV
	if (CF == 32 && count >= QPSX_GPU_4BPP_FLATV_MIN_PIXELS &&
	    qpsx_gpu_poly_span_4bpp_flatv(gpu_unai, pDst, count))
		return;
#endif
#if QPSX_GPU_POLY_2043_FAST
	if (CF == 2043) {
		qpsx_gpu_poly_span_2043(gpu_unai, pDst, count);
		return;
	}
#endif
#if QPSX_GPU_4BPP_FLATV_ROW
	if (CF == 32 && count >= QPSX_GPU_4BPP_FLATV_ROW_MIN_PIXELS &&
	    qpsx_gpu_poly_span_4bpp_flatv_row(gpu_unai, pDst, count))
		return;
#endif
#if QPSX_GPU_LINEAR_4BPP
	if (CF == 32 && qpsx_gpu_poly_span_4bpp_linear(gpu_unai, pDst, count))
		return;
#endif
#if QPSX_GPU_4BPP_FULLMASK
	if (CF == 32 && count >= QPSX_GPU_4BPP_FULLMASK_MIN_PIXELS &&
		gpu_unai.u_msk == ((255u << FIXED_BITS) | fixed_LOMASK) &&
		gpu_unai.v_msk == ((255u << FIXED_BITS) | fixed_LOMASK) &&
		qpsx_gpu_poly_span_4bpp_fullmask(gpu_unai, pDst, count))
		return;
#endif
#if QPSX_GPU_4BPP_GOURAUD_FLATV
	if (CF == 161 && count >= QPSX_GPU_4BPP_GOURAUD_FLATV_MIN_PIXELS &&
	    qpsx_gpu_poly_span_4bpp_gouraud_flatv(gpu_unai, pDst, count))
		return;
#endif
	// Blend func can save an operation if it knows uSrc MSB is unset.
	//  Untextured prims can always skip this (src color MSB is always 0).
	//  For textured prims, lighting funcs always return it unset. (bonus!)
	const bool skip_uSrc_mask = (!CF_TEXTMODE) || CF_LIGHT;

	u32 bMsk; if (CF_BLITMASK) bMsk = gpu_unai.blit_mask;

#if QPSX_GPU_PACKED_POLY_WRITES
	/* The opaque, untextured, non-Gouraud polygon is a pure fill.  It is
	 * common in the Ridge Racer scene (flat road/sky polygons), yet the
	 * generic loop performs one 16-bit store per pixel.  Pairing identical
	 * RGB555 pixels into aligned 32-bit stores halves store traffic and avoids
	 * the load/branch machinery used by every other polygon variant.  Keep the
	 * helper out of every template body: on a 16 KiB I-cache the cold alignment
	 * and tail code is more expensive than the saving for short spans. */
	if (!CF_TEXTMODE && !CF_GOURAUD && !CF_BLEND && !CF_MASKCHECK &&
	    !CF_MASKSET && !CF_BLITMASK && count >= 8) {
		qpsx_gpu_fill_flat_poly(pDst, count, gpu_unai.PixelData);
		return;
	}
#endif

	if (!CF_TEXTMODE)
	{
		if (!CF_GOURAUD)
		{
			// UNTEXTURED, NO GOURAUD
			const u16 pix15 = gpu_unai.PixelData;
			do {
				u16 uSrc, uDst;

				// NOTE: Don't enable CF_BLITMASK  pixel skipping (speed hack)
				//  on untextured polys. It seems to do more harm than good: see
				//  gravestone text at end of Medieval intro sequence. -senquack
				//if (CF_BLITMASK) { if ((bMsk>>((((uintptr_t)pDst)>>1)&7))&1) { goto endpolynotextnogou; } }

				if (CF_BLEND || CF_MASKCHECK) uDst = *pDst;
				if (CF_MASKCHECK) { if (uDst&0x8000) { goto endpolynotextnogou; } }

				uSrc = pix15;

				if (CF_BLEND) {
					// QPSX_090: Use fast blending if enabled
					if (FastBlendingEnabled())
						uSrc = gpuBlending_Fast<CF_BLENDMODE>(uSrc, uDst);
					else
						uSrc = gpuBlending<CF_BLENDMODE, skip_uSrc_mask>(uSrc, uDst);
				}

				if (CF_MASKSET) { *pDst = uSrc | 0x8000; }
				else            { *pDst = uSrc;          }

endpolynotextnogou:
				pDst++;
			} while(--count);
		}
		else
		{
			// UNTEXTURED, GOURAUD
			u32 l_gCol = gpu_unai.gCol;
			u32 l_gInc = gpu_unai.gInc;

			do {
				u16 uDst, uSrc;

				// See note in above loop regarding CF_BLITMASK
				//if (CF_BLITMASK) { if ((bMsk>>((((uintptr_t)pDst)>>1)&7))&1) goto endpolynotextgou; }

				if (CF_BLEND || CF_MASKCHECK) uDst = *pDst;
				if (CF_MASKCHECK) { if (uDst&0x8000) goto endpolynotextgou; }

				if (CF_DITHER) {
					// GOURAUD, DITHER

					u32 uSrc24 = gpuLightingRGB24(l_gCol);
					if (CF_BLEND) {
						// QPSX_090: Use fast blending if enabled
						if (FastBlendingEnabled())
							uSrc24 = gpuBlending24_Fast<CF_BLENDMODE>(uSrc24, uDst);
						else
							uSrc24 = gpuBlending24<CF_BLENDMODE>(uSrc24, uDst);
					}
					uSrc = gpuColorQuantization24<CF_DITHER>(uSrc24, pDst);
				} else {
					// GOURAUD, NO DITHER

					uSrc = gpuLightingRGB(l_gCol);

					if (CF_BLEND) {
						// QPSX_090: Use fast blending if enabled
						if (FastBlendingEnabled())
							uSrc = gpuBlending_Fast<CF_BLENDMODE>(uSrc, uDst);
						else
							uSrc = gpuBlending<CF_BLENDMODE, skip_uSrc_mask>(uSrc, uDst);
					}
				}

				if (CF_MASKSET) { *pDst = uSrc | 0x8000; }
				else            { *pDst = uSrc;          }

endpolynotextgou:
				pDst++;
				l_gCol += l_gInc;
			}
			while (--count);
		}
	}
	else
	{
		// TEXTURED

		u16 uDst, uSrc, srcMSB;

		//senquack - note: original UNAI code had gpu_unai.{u4/v4} packed into
		// one 32-bit unsigned int, but this proved to lose too much accuracy
		// (pixel drouputs noticeable in NFS3 sky), so now are separate vars.
		u32 l_u_msk = gpu_unai.u_msk;     u32 l_v_msk = gpu_unai.v_msk;
		u32 l_u = gpu_unai.u & l_u_msk;   u32 l_v = gpu_unai.v & l_v_msk;
		s32 l_u_inc = gpu_unai.u_inc;     s32 l_v_inc = gpu_unai.v_inc;

		const u16* TBA_ = gpu_unai.TBA;
		const u16* CBA_; if (CF_TEXTMODE!=3) CBA_ = gpu_unai.CBA;

		u8 r5, g5, b5;
		u8 r8, g8, b8;

		u32 l_gInc, l_gCol;

		if (CF_LIGHT) {
			if (CF_GOURAUD) {
				l_gInc = gpu_unai.gInc;
				l_gCol = gpu_unai.gCol;
			} else {
				if (CF_DITHER) {
					r8 = gpu_unai.r8;
					g8 = gpu_unai.g8;
					b8 = gpu_unai.b8;
				} else {
					r5 = gpu_unai.r5;
					g5 = gpu_unai.g5;
					b5 = gpu_unai.b5;
				}
			}
		}

		do
		{
			if (CF_BLITMASK) { if ((bMsk>>((((uintptr_t)pDst)>>1)&7))&1) goto endpolytext; }
			if (CF_MASKCHECK || CF_BLEND) { uDst = *pDst; }
			if (CF_MASKCHECK) if (uDst&0x8000) { goto endpolytext; }

			//senquack - adapted to work with new 22.10 fixed point routines:
			//           (UNAI originally used 16.16)
			if (CF_TEXTMODE==1) {  //  4bpp (CLUT)
				u32 tu=(l_u>>10);
				u32 tv=(l_v<<1)&(0xff<<11);
				u8 rgb=((u8*)TBA_)[tv+(tu>>1)];
				uSrc=CBA_[(rgb>>((tu&1)<<2))&0xf];
				if (!uSrc) goto endpolytext;
			}
			if (CF_TEXTMODE==2) {  //  8bpp (CLUT)
				uSrc = CBA_[(((u8*)TBA_)[(l_u>>10)+((l_v<<1)&(0xff<<11))])];
				if (!uSrc) goto endpolytext;
			}
			if (CF_TEXTMODE==3) {  // 16bpp
				uSrc = TBA_[(l_u>>10)+((l_v)&(0xff<<10))];
				if (!uSrc) goto endpolytext;
			}

			// Save source MSB, as blending or lighting will not (Silent Hill)
			if (CF_BLEND || CF_LIGHT) srcMSB = uSrc & 0x8000;

			// When textured, only dither when LIGHT (texture blend) is enabled
			// LIGHT &&  BLEND => dither
			// LIGHT && !BLEND => dither
			//!LIGHT &&  BLEND => no dither
			//!LIGHT && !BLEND => no dither

			if (CF_DITHER && CF_LIGHT) {
				u32 uSrc24;
				if ( CF_GOURAUD)
					uSrc24 = gpuLightingTXT24Gouraud(uSrc, l_gCol);
				if (!CF_GOURAUD)
					uSrc24 = gpuLightingTXT24(uSrc, r8, g8, b8);

				if (CF_BLEND && srcMSB) {
					// QPSX_090: Use fast blending if enabled
					if (FastBlendingEnabled())
						uSrc24 = gpuBlending24_Fast<CF_BLENDMODE>(uSrc24, uDst);
					else
						uSrc24 = gpuBlending24<CF_BLENDMODE>(uSrc24, uDst);
				}

				uSrc = gpuColorQuantization24<CF_DITHER>(uSrc24, pDst);
			} else
			{
				if (CF_LIGHT) {
					// QPSX_091: Priority: ASM > Fast > LUT
					if (AsmLightingEnabled()) {
						if ( CF_GOURAUD)
							uSrc = gpuLightingTXTGouraud_ASM(uSrc, l_gCol);
						if (!CF_GOURAUD)
							uSrc = gpuLightingTXT_ASM(uSrc, r5, g5, b5);
					} else if (FastLightingEnabled()) {
						if ( CF_GOURAUD)
							uSrc = gpuLightingTXTGouraud_Fast(uSrc, l_gCol);
						if (!CF_GOURAUD)
							uSrc = gpuLightingTXT_Fast(uSrc, r5, g5, b5);
					} else {
						if ( CF_GOURAUD)
							uSrc = gpuLightingTXTGouraud(uSrc, l_gCol);
						if (!CF_GOURAUD)
							uSrc = gpuLightingTXT(uSrc, r5, g5, b5);
					}
				}

				if (CF_BLEND && srcMSB) {
					// QPSX_091: Priority: ASM > Fast > Original
					if (AsmBlendingEnabled())
						uSrc = gpuBlending_ASM<CF_BLENDMODE>(uSrc, uDst);
					else if (FastBlendingEnabled())
						uSrc = gpuBlending_Fast<CF_BLENDMODE>(uSrc, uDst);
					else
						uSrc = gpuBlending<CF_BLENDMODE, skip_uSrc_mask>(uSrc, uDst);
				}
			}

			if (CF_MASKSET)                { *pDst = uSrc | 0x8000; }
			else if (CF_BLEND || CF_LIGHT) { *pDst = uSrc | srcMSB; }
			else                           { *pDst = uSrc;          }
endpolytext:
			pDst++;
			l_u = (l_u + l_u_inc) & l_u_msk;
			l_v = (l_v + l_v_inc) & l_v_msk;
			if (CF_LIGHT && CF_GOURAUD) l_gCol += l_gInc;
		}
		while (--count);
	}
}

static void PolyNULL(const gpu_unai_t &gpu_unai, u16 *pDst, u32 count)
{
	#ifdef ENABLE_GPU_LOG_SUPPORT
		fprintf(stdout,"PolyNULL()\n");
	#endif
}

/* The physical scene used for SF2000 tuning exercises only a few of the
 * 2048 table entries.  Explicitly instantiate those entries before the
 * complete dispatch table so function-section linking places their hot
 * bodies together at the front of the renderer text.  The table remains
 * complete and all other variants retain their exact semantics. */
#if QPSX_GPU_HOT_DRIVER_ORDER == 2
/* Keep measured driver families separate: 274/278 are pixel-span CF
 * values, 46 is a sprite-span CF value, and 2043 is a polygon-span CF
 * value.  Explicit instantiation only changes section order; dispatch
 * indices and table types stay untouched. */
template u8* gpuPixelSpanFn<274>(u8 *, uintptr_t, ptrdiff_t, size_t);
template u8* gpuPixelSpanFn<278>(u8 *, uintptr_t, ptrdiff_t, size_t);
template void gpuSpriteSpanFn<46>(u16 *, u32, u8 *, u32);
template void gpuPolySpanFn<2043>(const gpu_unai_t &, u16 *, u32);
#endif
#if QPSX_GPU_HOT_DRIVER_ORDER == 1 || QPSX_GPU_HOT_DRIVER_ORDER == 2
/* Legacy measured order retained as mode 1 and as the tail of mode 2. */
template void gpuPolySpanFn<32>(const gpu_unai_t &, u16 *, u32);
template void gpuPolySpanFn<161>(const gpu_unai_t &, u16 *, u32);
template void gpuPolySpanFn<163>(const gpu_unai_t &, u16 *, u32);
template void gpuPolySpanFn<2>(const gpu_unai_t &, u16 *, u32);
#endif

///////////////////////////////////////////////////////////////////////////////
//  Polygon innerloops driver
typedef void (*PP)(const gpu_unai_t &gpu_unai, u16 *pDst, u32 count);

// Template instantiation helper macros
/*
 * Several command bits have no effect in particular span families. Map those
 * table entries onto one canonical instantiation so taking every driver's
 * address does not force the compiler to emit identical copies. This changes
 * neither the table layout nor the inner-loop decisions that can be reached:
 *
 * - blend mode is unused when blending is disabled;
 * - lighting is unused for untextured polygons;
 * - Gouraud is unused for an unlit texture;
 * - dithering needs Gouraud on untextured spans or lighting on textures;
 * - display downsampling's blit mask is unused for untextured polygons.
 */
#define POLY_CANONICAL_FLAGS(cf) ( \
	((cf) & (0x02 | 0x04 | 0x60 | 0x100)) | \
	(((cf) & 0x02) ? ((cf) & 0x18) : 0) | \
	(((cf) & 0x60) ? ((cf) & 0x01) : 0) | \
	((!((cf) & 0x60) || ((cf) & 0x01)) ? ((cf) & 0x80) : 0) | \
	(((!((cf) & 0x60) && ((cf) & 0x80)) || \
	  (((cf) & 0x60) && ((cf) & 0x01))) ? ((cf) & 0x200) : 0) | \
	(((cf) & 0x60) ? ((cf) & 0x400) : 0))
#define TI(cf) gpuPolySpanFn<POLY_CANONICAL_FLAGS(cf)>
#define TN     PolyNULL
#define TIBLOCK(ub) \
	TI((ub)|0x00), TI((ub)|0x01), TI((ub)|0x02), TI((ub)|0x03), TI((ub)|0x04), TI((ub)|0x05), TI((ub)|0x06), TI((ub)|0x07), \
	TN,            TN,            TI((ub)|0x0a), TI((ub)|0x0b), TN,            TN,            TI((ub)|0x0e), TI((ub)|0x0f), \
	TN,            TN,            TI((ub)|0x12), TI((ub)|0x13), TN,            TN,            TI((ub)|0x16), TI((ub)|0x17), \
	TN,            TN,            TI((ub)|0x1a), TI((ub)|0x1b), TN,            TN,            TI((ub)|0x1e), TI((ub)|0x1f), \
	TI((ub)|0x20), TI((ub)|0x21), TI((ub)|0x22), TI((ub)|0x23), TI((ub)|0x24), TI((ub)|0x25), TI((ub)|0x26), TI((ub)|0x27), \
	TN,            TN,            TI((ub)|0x2a), TI((ub)|0x2b), TN,            TN,            TI((ub)|0x2e), TI((ub)|0x2f), \
	TN,            TN,            TI((ub)|0x32), TI((ub)|0x33), TN,            TN,            TI((ub)|0x36), TI((ub)|0x37), \
	TN,            TN,            TI((ub)|0x3a), TI((ub)|0x3b), TN,            TN,            TI((ub)|0x3e), TI((ub)|0x3f), \
	TI((ub)|0x40), TI((ub)|0x41), TI((ub)|0x42), TI((ub)|0x43), TI((ub)|0x44), TI((ub)|0x45), TI((ub)|0x46), TI((ub)|0x47), \
	TN,            TN,            TI((ub)|0x4a), TI((ub)|0x4b), TN,            TN,            TI((ub)|0x4e), TI((ub)|0x4f), \
	TN,            TN,            TI((ub)|0x52), TI((ub)|0x53), TN,            TN,            TI((ub)|0x56), TI((ub)|0x57), \
	TN,            TN,            TI((ub)|0x5a), TI((ub)|0x5b), TN,            TN,            TI((ub)|0x5e), TI((ub)|0x5f), \
	TI((ub)|0x60), TI((ub)|0x61), TI((ub)|0x62), TI((ub)|0x63), TI((ub)|0x64), TI((ub)|0x65), TI((ub)|0x66), TI((ub)|0x67), \
	TN,            TN,            TI((ub)|0x6a), TI((ub)|0x6b), TN,            TN,            TI((ub)|0x6e), TI((ub)|0x6f), \
	TN,            TN,            TI((ub)|0x72), TI((ub)|0x73), TN,            TN,            TI((ub)|0x76), TI((ub)|0x77), \
	TN,            TN,            TI((ub)|0x7a), TI((ub)|0x7b), TN,            TN,            TI((ub)|0x7e), TI((ub)|0x7f), \
	TN,            TI((ub)|0x81), TN,            TI((ub)|0x83), TN,            TI((ub)|0x85), TN,            TI((ub)|0x87), \
	TN,            TN,            TN,            TI((ub)|0x8b), TN,            TN,            TN,            TI((ub)|0x8f), \
	TN,            TN,            TN,            TI((ub)|0x93), TN,            TN,            TN,            TI((ub)|0x97), \
	TN,            TN,            TN,            TI((ub)|0x9b), TN,            TN,            TN,            TI((ub)|0x9f), \
	TN,            TI((ub)|0xa1), TN,            TI((ub)|0xa3), TN,            TI((ub)|0xa5), TN,            TI((ub)|0xa7), \
	TN,            TN,            TN,            TI((ub)|0xab), TN,            TN,            TN,            TI((ub)|0xaf), \
	TN,            TN,            TN,            TI((ub)|0xb3), TN,            TN,            TN,            TI((ub)|0xb7), \
	TN,            TN,            TN,            TI((ub)|0xbb), TN,            TN,            TN,            TI((ub)|0xbf), \
	TN,            TI((ub)|0xc1), TN,            TI((ub)|0xc3), TN,            TI((ub)|0xc5), TN,            TI((ub)|0xc7), \
	TN,            TN,            TN,            TI((ub)|0xcb), TN,            TN,            TN,            TI((ub)|0xcf), \
	TN,            TN,            TN,            TI((ub)|0xd3), TN,            TN,            TN,            TI((ub)|0xd7), \
	TN,            TN,            TN,            TI((ub)|0xdb), TN,            TN,            TN,            TI((ub)|0xdf), \
	TN,            TI((ub)|0xe1), TN,            TI((ub)|0xe3), TN,            TI((ub)|0xe5), TN,            TI((ub)|0xe7), \
	TN,            TN,            TN,            TI((ub)|0xeb), TN,            TN,            TN,            TI((ub)|0xef), \
	TN,            TN,            TN,            TI((ub)|0xf3), TN,            TN,            TN,            TI((ub)|0xf7), \
	TN,            TN,            TN,            TI((ub)|0xfb), TN,            TN,            TN,            TI((ub)|0xff)

const PP gpuPolySpanDrivers[2048] = {
	TIBLOCK(0<<8), TIBLOCK(1<<8), TIBLOCK(2<<8), TIBLOCK(3<<8),
	TIBLOCK(4<<8), TIBLOCK(5<<8), TIBLOCK(6<<8), TIBLOCK(7<<8)
};

#undef TI
#undef TN
#undef TIBLOCK
#undef POLY_CANONICAL_FLAGS

///////////////////////////////////////////////////////////////////////////////
// Optional renderer-driver histogram.  The production build leaves these
// wrappers as the original direct table accesses, so there is no counter or
// branch in the hot path.  A diagnostic build can enable the histogram to
// identify the small set of inner-loop variants that account for most of the
// work on a particular game/scene.  Keeping the counters here makes them
// local to the translation unit that owns the renderer and avoids a data
// relocation on the NOMMU target.
#if QPSX_GPU_RUNTIME_METRICS
static u32 qpsx_gpu_poly_hist[2048];
static u32 qpsx_gpu_sprite_hist[256];
static u32 qpsx_gpu_pixel_hist[64];
static u32 qpsx_gpu_tile_hist[32];
u32 qpsx_gpu_poly_span_hist[2048];
u32 qpsx_gpu_poly_pixel_hist[2048];
u32 qpsx_gpu_sprite_pixel_hist[256];
u32 qpsx_gpu_tile_pixel_hist[32];
u32 qpsx_gpu_poly_fullmask_spans;
u32 qpsx_gpu_poly_fullmask_pixels;
u32 qpsx_gpu_poly_unit_u_spans;
u32 qpsx_gpu_poly_unit_u_pixels;
u32 qpsx_gpu_poly_flat_v_spans;
u32 qpsx_gpu_poly_flat_v_pixels;
u32 qpsx_gpu_line_g_total;
u32 qpsx_gpu_line_g_flatfast;

static inline PP qpsx_gpu_poly_driver(u32 index)
{
	++qpsx_gpu_poly_hist[index];
	return gpuPolySpanDrivers[index];
}

static inline PS qpsx_gpu_sprite_driver(u32 index)
{
	++qpsx_gpu_sprite_hist[index];
	return gpuSpriteSpanDrivers[index];
}

static inline PSD qpsx_gpu_pixel_driver(u32 index)
{
	++qpsx_gpu_pixel_hist[index];
	return gpuPixelSpanDrivers[index];
}

static inline PT qpsx_gpu_tile_driver(u32 index)
{
	++qpsx_gpu_tile_hist[index];
	return gpuTileSpanDrivers[index];
}

#define QPSX_GPU_POLY_DRIVER(index) qpsx_gpu_poly_driver((u32)(index))
#define QPSX_GPU_SPRITE_DRIVER(index) qpsx_gpu_sprite_driver((u32)(index))
#define QPSX_GPU_PIXEL_DRIVER(index) qpsx_gpu_pixel_driver((u32)(index))
#define QPSX_GPU_TILE_DRIVER(index) qpsx_gpu_tile_driver((u32)(index))
#else
#define QPSX_GPU_POLY_DRIVER(index) gpuPolySpanDrivers[(index)]
#define QPSX_GPU_SPRITE_DRIVER(index) gpuSpriteSpanDrivers[(index)]
#define QPSX_GPU_PIXEL_DRIVER(index) gpuPixelSpanDrivers[(index)]
#define QPSX_GPU_TILE_DRIVER(index) gpuTileSpanDrivers[(index)]
#endif
