/* Differential proof for the CF=2043 compact span representation.
 *
 * This intentionally has no emulator linkage: the baseline is a small,
 * independent host model while span_fast is the exact production loop
 * template included from gpu_poly2043_fast.h.  This keeps the test cheap
 * without allowing a duplicated "fast" implementation to drift from the
 * code shipped in the core. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

#define FIXED_BITS 10
#include "../src/gpu/gpu_unai/gpu_poly2043_fast.h"

static const u32 dither_matrix[64] = {
	0x00000000, 0x00200808, 0x00080202, 0x00280a0a, 0x00000000, 0x00200808, 0x00080202, 0x00280a0a,
	0x00300c0c, 0x00100404, 0x00380e0e, 0x00180606, 0x00300c0c, 0x00100404, 0x00380e0e, 0x00180606,
	0x00080202, 0x00280a0a, 0x00000000, 0x00200808, 0x00080202, 0x00280a0a, 0x00000000, 0x00200808,
	0x00380e0e, 0x00180606, 0x00300c0c, 0x00100404, 0x00380e0e, 0x00180606, 0x00300c0c, 0x00100404,
	0x00000000, 0x00200808, 0x00080202, 0x00280a0a, 0x00000000, 0x00200808, 0x00080202, 0x00280a0a,
	0x00300c0c, 0x00100404, 0x00380e0e, 0x00180606, 0x00300c0c, 0x00100404, 0x00380e0e, 0x00180606,
	0x00080202, 0x00280a0a, 0x00000000, 0x00200808, 0x00080202, 0x00280a0a, 0x00000000, 0x00200808,
	0x00380e0e, 0x00180606, 0x00300c0c, 0x00100404, 0x00380e0e, 0x00180606, 0x00300c0c, 0x00100404
};

struct Span {
	u16 *vram;
	const u16 *TBA;
	u32 u, v, u_msk, v_msk, gCol, gInc;
	s32 u_inc, v_inc;
	u8 blit_mask;
	const u32 *DitherMatrix;
};

static u32 light24(u16 src, u32 g_col)
{
	u32 r = (u32)(src & 0x1f) * ((g_col >> 24) & 0xff);
	u32 g = (u32)(src & 0x3e0) * ((g_col >> 13) & 0xff);
	u32 b = (u32)(src & 0x7c00) * ((g_col >> 2) & 0xff);
	if (r & 0xfffff000u) r = ~0xfffff000u;
	if (g & 0xfffe0000u) g = ~0xfffe0000u;
	if (b & 0xffc00000u) b = ~0xffc00000u;
	return (r >> 3) | ((g >> 8) << 10) | ((b >> 13) << 20);
}

static u32 get_rgb24(u16 src)
{
	return ((src & 0x7c00u) << 14) | ((src & 0x03e0u) << 9) |
	       ((src & 0x001fu) << 4);
}

static u32 blend24_mode3(u32 src24, u16 dst)
{
	src24 = (src24 & 0x1fc7f1fcu) >> 2;
	u32 sum = src24 + get_rgb24(dst);
	u32 carries = sum & 0x20080200u;
	u32 modulo = sum - carries;
	u32 clamp = carries - (carries >> 9);
	return modulo | clamp;
}

static u16 quantize_with_coeff(u32 src24, u32 dither)
{
	src24 = (src24 & 0x1ff7fdffu) + dither;
	if (src24 & (1u << 9))  src24 |= 0x1ffu;
	if (src24 & (1u << 19)) src24 |= 0x1ffu << 10;
	if (src24 & (1u << 29)) src24 |= 0x1ffu << 20;
	return (u16)(((src24 >> 4) & 0x1f) |
	             ((src24 >> 9) & (0x1f << 5)) |
	             ((src24 >> 14) & (0x1f << 10)));
}

static u16 quantize(u32 src24, const u32 *dither, u16 fbpos)
{
	u16 offset = (u16)(((fbpos & (7u << 10)) >> 7) | (fbpos & 7));
	return quantize_with_coeff(src24, dither[offset]);
}

static void span_baseline(const Span &s, u16 *dst, u32 count)
{
	u32 u = s.u & s.u_msk;
	u32 v = s.v & s.v_msk;
	u32 g_col = s.gCol;
	for (u32 i = 0; i < count; ++i) {
		u16 *p = dst + i;
		if (!((s.blit_mask >> ((((uintptr_t)p) >> 1) & 7)) & 1u)) {
			u16 texel = s.TBA[(u >> 10) + (v & (0xffu << 10))];
			if (texel) {
				u16 msb = texel & 0x8000u;
				u32 src24 = light24(texel, g_col);
				if (msb)
					src24 = blend24_mode3(src24, *p);
				*p = quantize(src24, dither_matrix,
						     (u16)(u32)(p - s.vram)) | 0x8000u;
			}
		}
		u = (u32)((s32)(u + s.u_inc) & s.u_msk);
		v = (u32)((s32)(v + s.v_inc) & s.v_msk);
		g_col += s.gInc;
	}
}

struct host_poly_light_policy {
	static u32 apply(u16 texel, u32 g_col)
	{
		return light24(texel, g_col);
	}
};

struct host_poly_blend_policy {
	static u32 apply(u32 src24, u16 dst)
	{
		return blend24_mode3(src24, dst);
	}
};

struct host_poly_quant_policy {
	static u16 apply(u32 src24, u32 dither)
	{
		return quantize_with_coeff(src24, dither);
	}
};

static void span_fast(const Span &s, u16 *dst, u32 count)
{
	qpsx_gpu_poly_span_2043_fast<Span, host_poly_light_policy,
		host_poly_blend_policy, host_poly_quant_policy>(s, dst, count);
}

static u32 rng_state = 0x6d2b79f5u;
static u32 rnd(void)
{
	rng_state ^= rng_state << 13;
	rng_state ^= rng_state >> 17;
	rng_state ^= rng_state << 5;
	return rng_state;
}

static int run_case(u32 count, u32 dst_offset, u32 case_no)
{
	/* Keep the 384 KiB fixture out of the small test-process stack. */
	static u16 vram_a[65536] __attribute__((aligned(16)));
	static u16 vram_b[65536] __attribute__((aligned(16)));
	static u16 texture[262144] __attribute__((aligned(16)));
	Span s;
	for (u32 i = 0; i < 65536; ++i)
		vram_a[i] = (u16)rnd();
	for (u32 i = 0; i < 262144; ++i)
		/* Force transparent, opaque and MSB-set texels into every fixture. */
		texture[i] = ((i % 17u) == 0) ? 0 : (u16)rnd();
	memcpy(vram_b, vram_a, sizeof(vram_a));
	s.vram = vram_a;
	s.TBA = texture;
	s.u_msk = ((rnd() & 255u) << 10) | 0x3ffu;
	s.v_msk = ((rnd() & 255u) << 10) | 0x3ffu;
	s.u = rnd();
	s.v = rnd();
	s.u_inc = (s32)(rnd() & 0x1ffffu) - 0x10000;
	s.v_inc = (s32)(rnd() & 0x1ffffu) - 0x10000;
	s.gCol = rnd();
	s.gInc = rnd();
	s.blit_mask = (u8)rnd();
	s.DitherMatrix = dither_matrix;
	if (case_no == 1104) {
		/* Deterministic Mode-3 saturation fixture: an opaque white texel,
		 * maximum Gouraud light, and a full destination force both blend
		 * channels through their carry/clamp paths. */
		s.u = s.v = 0;
		s.u_msk = s.v_msk = 0x3ffu;
		s.u_inc = s.v_inc = 0;
		s.gCol = 0xffffffffu;
		s.gInc = 0;
		s.blit_mask = 0;
		texture[0] = 0xffffu;
		for (u32 i = 0; i < 8; ++i) {
			vram_a[1016 + i] = 0x7fffu;
			vram_b[1016 + i] = 0x7fffu;
		}
	}
	Span t = s;
	t.vram = vram_b;
	span_baseline(s, vram_a + dst_offset, count);
	span_fast(t, vram_b + dst_offset, count);
	if (memcmp(vram_a, vram_b, sizeof(vram_a)) != 0) {
		fprintf(stderr, "CF2043 differential failure case=%u count=%u dst=%u\n",
			case_no, count, dst_offset);
		return 1;
	}
	return 0;
}

int main(void)
{
	/* Covers short/long spans, odd/even VRAM positions, every blit pattern,
	 * edge UV masks/wrap, transparent and MSB-set texels, masked destination
	 * data, dithering positions, and blend saturation through the random
	 * texture/framebuffer population. */
	static const u32 lengths[] = { 1, 2, 3, 7, 8, 15, 16, 31, 64, 127, 255, 511 };
	for (u32 i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
		for (u32 j = 0; j < 8; ++j) {
			u32 row = rnd() % 64;
			u32 x = rnd() % (1024u - lengths[i] + 1);
			u32 dst = row * 1024u + x;
			if (run_case(lengths[i], dst, i * 8 + j))
				return 1;
		}
	}
	for (u32 i = 0; i < 1000; ++i) {
		u32 count = 1 + (rnd() % 768);
		u32 row = rnd() % 64;
		u32 x = rnd() % (1024u - count + 1);
		u32 dst = row * 1024u + x;
		if (run_case(count, dst, 100 + i))
			return 1;
	}
	/* Explicit row-end cases exercise the proven no-crossing invariant. */
	if (run_case(1, 1023, 1100) ||
		run_case(8, 1016, 1101) ||
		run_case(1024, 0, 1102) ||
		run_case(1, 7 * 1024 + 1023, 1103) ||
		run_case(8, 1016, 1104))
		return 1;
	puts("gpu_poly2043_diff: PASS (1101 deterministic spans)");
	return 0;
}
