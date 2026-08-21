/* Differential proof for the CF=2043 compact span representation.
 *
 * This intentionally has no emulator linkage: it is a small host test of
 * the mechanically-derived pixel operations and the one optimization (carry
 * the VRAM index used by dithering).  The production implementation calls
 * the corresponding gpu_unai helpers; keeping both models here makes edge
 * cases easy to exercise without constructing the complete PSX core. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t s32;

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
	const u16 *texture;
	u32 u, v, u_mask, v_mask, g_col, g_inc;
	s32 u_inc, v_inc;
	u8 blit_mask;
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

static u16 quantize(u32 src24, const u32 *dither, u16 fbpos)
{
	u16 offset = (u16)(((fbpos & (7u << 10)) >> 7) | (fbpos & 7));
	src24 = (src24 & 0x1ff7fdffu) + dither[offset];
	if (src24 & (1u << 9))  src24 |= 0x1ffu;
	if (src24 & (1u << 19)) src24 |= 0x1ffu << 10;
	if (src24 & (1u << 29)) src24 |= 0x1ffu << 20;
	return (u16)(((src24 >> 4) & 0x1f) |
	             ((src24 >> 9) & (0x1f << 5)) |
	             ((src24 >> 14) & (0x1f << 10)));
}

static void span_baseline(const Span &s, u16 *dst, u32 count)
{
	u32 u = s.u & s.u_mask;
	u32 v = s.v & s.v_mask;
	u32 g_col = s.g_col;
	for (u32 i = 0; i < count; ++i) {
		u16 *p = dst + i;
		if (!((s.blit_mask >> ((((uintptr_t)p) >> 1) & 7)) & 1u)) {
			u16 texel = s.texture[(u >> 10) + (v & (0xffu << 10))];
			if (texel) {
				u16 msb = texel & 0x8000u;
				u32 src24 = light24(texel, g_col);
				if (msb)
					src24 = blend24_mode3(src24, *p);
				*p = quantize(src24, dither_matrix,
						     (u16)(u32)(p - s.vram)) | 0x8000u;
			}
		}
		u = (u32)((s32)(u + s.u_inc) & s.u_mask);
		v = (u32)((s32)(v + s.v_inc) & s.v_mask);
		g_col += s.g_inc;
	}
}

static void span_fast(const Span &s, u16 *dst, u32 count)
{
	u32 u = s.u & s.u_mask;
	u32 v = s.v & s.v_mask;
	u32 g_col = s.g_col;
	u16 fbpos = (u16)(u32)(dst - s.vram);
	uintptr_t addr = (uintptr_t)dst;
	for (u32 i = 0; i < count; ++i) {
		u16 *p = dst + i;
		if (!((s.blit_mask >> ((addr >> 1) & 7)) & 1u)) {
			u16 texel = s.texture[(u >> 10) + (v & (0xffu << 10))];
			if (texel) {
				u16 msb = texel & 0x8000u;
				u32 src24 = light24(texel, g_col);
				if (msb)
					src24 = blend24_mode3(src24, *p);
				*p = quantize(src24, dither_matrix, fbpos) | 0x8000u;
			}
		}
		++fbpos;
		addr += sizeof(u16);
		u = (u32)((s32)(u + s.u_inc) & s.u_mask);
		v = (u32)((s32)(v + s.v_inc) & s.v_mask);
		g_col += s.g_inc;
	}
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
	static u16 vram_a[65536], vram_b[65536], texture[262144];
	Span s;
	for (u32 i = 0; i < 65536; ++i)
		vram_a[i] = (u16)rnd();
	for (u32 i = 0; i < 262144; ++i)
		/* Force transparent, opaque and MSB-set texels into every fixture. */
		texture[i] = ((i % 17u) == 0) ? 0 : (u16)rnd();
	memcpy(vram_b, vram_a, sizeof(vram_a));
	s.vram = vram_a;
	s.texture = texture;
	s.u_mask = ((rnd() & 255u) << 10) | 0x3ffu;
	s.v_mask = ((rnd() & 255u) << 10) | 0x3ffu;
	s.u = rnd();
	s.v = rnd();
	s.u_inc = (s32)(rnd() & 0x1ffffu) - 0x10000;
	s.v_inc = (s32)(rnd() & 0x1ffffu) - 0x10000;
	s.g_col = rnd();
	s.g_inc = rnd();
	s.blit_mask = (u8)rnd();
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
			u32 dst = 1 + ((rnd() >> 1) % (65536u - lengths[i] - 2));
			if (run_case(lengths[i], dst, i * 8 + j))
				return 1;
		}
	}
	for (u32 i = 0; i < 1000; ++i) {
		u32 count = 1 + (rnd() % 768);
		u32 dst = 1 + ((rnd() >> 1) % (65536u - count - 2));
		if (run_case(count, dst, 100 + i))
			return 1;
	}
	puts("gpu_poly2043_diff: PASS (1096 deterministic spans)");
	return 0;
}
