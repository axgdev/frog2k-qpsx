/*
 * Compact CF=2043 polygon span loop.
 *
 * This header is deliberately independent of gpu_unai_t's definition and of
 * the renderer helper names.  The production renderer supplies three small
 * policies for lighting, blending, and quantization; the host differential
 * test supplies equivalent policies.  Consequently the loop exercised by
 * the test is the same template that is compiled into the target core.
 */
#ifndef QPSX_GPU_POLY2043_FAST_H
#define QPSX_GPU_POLY2043_FAST_H

#if defined(__GNUC__)
#define QPSX_GPU_POLY2043_INLINE static inline __attribute__((always_inline))
#else
#define QPSX_GPU_POLY2043_INLINE static inline
#endif

/*
 * The polygon rasterizers construct every span from PixelBase + xa and clip
 * xb to DrawingArea[2].  GP0(E4) stores that limit as a 10-bit coordinate plus
 * one, so each submitted span contains pixels [xa, xb) with xb <= 1024.  It
 * therefore cannot cross a 1024-pixel VRAM row.  The GPULIB and standalone
 * VRAM allocators both align the base to at least 16 bytes; fbpos&7 is thus
 * exactly the destination-address pixel modulo 8 used by the original blit
 * mask test.
 */
template<typename State, typename LightPolicy, typename BlendPolicy,
         typename QuantPolicy>
QPSX_GPU_POLY2043_INLINE void
qpsx_gpu_poly_span_2043_fast(const State &state, u16 *pDst, u32 count)
{
	u32 l_u = state.u & state.u_msk;
	u32 l_v = state.v & state.v_msk;
	const u32 l_u_msk = state.u_msk;
	const u32 l_v_msk = state.v_msk;
	const s32 l_u_inc = state.u_inc;
	const s32 l_v_inc = state.v_inc;
	const u16 *texture = state.TBA;
	u32 l_gCol = state.gCol;
	const u32 l_gInc = state.gInc;
	const u8 blit_mask = state.blit_mask;
	/* Match the original quantizer's intentional u16 truncation. */
	const u16 fbpos = (u16)(u32)(pDst - state.vram);
	const u32 dither_row = (u32)((fbpos & (7u << 10)) >> 7);
	u32 x = fbpos & 7u;
	if (!count)
		return;

	do {
		if ((blit_mask >> x) & 1u)
			goto skip;

		{
			const u16 texel = texture[(l_u >> FIXED_BITS) +
							  (l_v & (0xffu << FIXED_BITS))];
			u32 src24;

			if (!texel)
				goto skip;

			/* CF=2043 is always Gouraud + dither. */
			src24 = LightPolicy::apply(texel, l_gCol);
			if (texel & 0x8000u)
				src24 = BlendPolicy::apply(src24, *pDst);
			*pDst = QuantPolicy::apply(src24,
							  state.DitherMatrix[dither_row | x]) |
					  0x8000u;
		}

skip:
		++pDst;
		x = (x + 1u) & 7u;
		l_u = (l_u + l_u_inc) & l_u_msk;
		l_v = (l_v + l_v_inc) & l_v_msk;
		l_gCol += l_gInc;
	} while (--count);
}

#undef QPSX_GPU_POLY2043_INLINE

#endif
