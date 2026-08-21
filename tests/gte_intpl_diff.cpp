/*
 * Differential test for the MIPS recompiler's constant INTPL entry points.
 *
 * This deliberately links the production src/gte.cpp rather than copying the
 * algorithm into a test helper.  The generic opcode-taking implementation is
 * the reference and each specialized implementation must produce an
 * identical complete emulator state for the corresponding SF/LM pair.
 */

#include "gte.h"

#include <stdio.h>
#include <string.h>

psxRegisters psxRegs;

static unsigned failures;

static u32 next_random(u32 *state)
{
	/* Deterministic xorshift32: no libc/random implementation differences. */
	u32 value = *state;
	value ^= value << 13;
	value ^= value >> 17;
	value ^= value << 5;
	*state = value;
	return value;
}

static void report_mismatch(const char *label, unsigned sf, unsigned lm,
		unsigned vector, const psxRegisters *expected,
		const psxRegisters *actual)
{
	const unsigned char *want = (const unsigned char *)expected;
	const unsigned char *got = (const unsigned char *)actual;
	size_t offset;

	for (offset = 0; offset < sizeof(*expected); ++offset) {
		if (want[offset] != got[offset])
			break;
	}

	if (failures < 8) {
		unsigned want_byte = offset < sizeof(*expected) ? want[offset] : 0;
		unsigned got_byte = offset < sizeof(*actual) ? got[offset] : 0;
		fprintf(stderr,
			"INTPL mismatch %s sf=%u lm=%u vector=%u byte=%lu "
			"want=%02x got=%02x flag=%08x/%08x\n",
			label, sf, lm, vector, (unsigned long)offset,
			want_byte, got_byte,
			expected->CP2C.r[31], actual->CP2C.r[31]);
	}
	++failures;
}

typedef void (*intpl_function)(void);

static intpl_function specialized(unsigned sf, unsigned lm)
{
	if (!sf && !lm)
		return gteINTPL_s0_l0;
	if (!sf && lm)
		return gteINTPL_s0_l1;
	if (sf && !lm)
		return gteINTPL_s1_l0;
	return gteINTPL_s1_l1;
}

static void compare_one(const char *label, const psxRegisters *seed,
		unsigned sf, unsigned lm, unsigned vector)
{
	psxRegisters expected;
	intpl_function function = specialized(sf, lm);

	psxRegs = *seed;
	gteINTPL((sf << 9) | lm);
	expected = psxRegs;

	psxRegs = *seed;
	function();
	if (memcmp(&expected, &psxRegs, sizeof(expected)) != 0)
		report_mismatch(label, sf, lm, vector, &expected, &psxRegs);
}

static void fill_random_state(psxRegisters *state, u32 *random_state)
{
	size_t i;
	*state = psxRegisters();

	for (i = 0; i < sizeof(state->GPR.r) / sizeof(state->GPR.r[0]); ++i)
		state->GPR.r[i] = next_random(random_state);
	for (i = 0; i < sizeof(state->CP0.r) / sizeof(state->CP0.r[0]); ++i)
		state->CP0.r[i] = next_random(random_state);
	for (i = 0; i < sizeof(state->CP2D.r) / sizeof(state->CP2D.r[0]); ++i)
		state->CP2D.r[i] = next_random(random_state);
	for (i = 0; i < sizeof(state->CP2C.r) / sizeof(state->CP2C.r[0]); ++i)
		state->CP2C.r[i] = next_random(random_state);
	state->pc = next_random(random_state);
	state->code = next_random(random_state);
	state->cycle = next_random(random_state);
	state->interrupt = next_random(random_state);
	state->io_cycle_counter = next_random(random_state);
	state->writeok = (int)(next_random(random_state) & 1);
	state->chain_budget = next_random(random_state);
	state->cycles_until_event = (s32)next_random(random_state);
	for (i = 0; i < sizeof(state->intCycle) / sizeof(state->intCycle[0]); ++i) {
		state->intCycle[i].sCycle = next_random(random_state);
		state->intCycle[i].cycle = next_random(random_state);
	}
}

static void set_boundary_state(psxRegisters *state, s32 ir0, s32 ir1,
		s32 ir2, s32 ir3, s32 rfc, s32 gfc, s32 bfc, u32 flag)
{
	*state = psxRegisters();
	state->GPR.r[1] = 0xa5a5a5a5u;
	state->CP0.r[1] = 0x5a5a5a5au;
	state->CP2D.r[0] = 0xa5a5a5a5u;
	state->CP2C.r[0] = 0x5a5a5a5au;
	state->CP2D.p[8].sw.l = (s16)ir0;
	state->CP2D.p[9].sw.l = (s16)ir1;
	state->CP2D.p[10].sw.l = (s16)ir2;
	state->CP2D.p[11].sw.l = (s16)ir3;
	state->CP2C.r[21] = (u32)rfc;
	state->CP2C.r[22] = (u32)gfc;
	state->CP2C.r[23] = (u32)bfc;
	state->CP2C.r[31] = flag;
}

static void run_boundary_vectors(void)
{
	static const s32 values[][7] = {
		{ 0,       0,       0,       0,       0,       0,       0       },
		{ 1,       1,       1,       1,       1,       1,       1       },
		{ -1,      -1,      -1,      -1,      -1,      -1,      -1      },
		{ 32767,   32767,   32767,   32767,   32767,   32767,   32767  },
		{ -32768,  -32768,  -32768,  -32768,  -32768,  -32768,  -32768 },
		{ 32767,   -32768,  32767,   -32768,  0x7fffffff, 0, -1      },
		{ -32768,  32767,  -32768,  32767,  -2147483647 - 1, -1, 0x7fffffff },
		{ 32767,   0,       0,       0,       -32768, 32767, 0       },
		{ -32768,  0,       0,       0,       32767, -32768, 0       },
		{ 16384,   32767,   -32768,  1,       0x7fffffff, -2147483647 - 1, 32767 },
		{ -16384,  -32768,  32767,  -1,      -2147483647 - 1, 0x7fffffff, -32768 },
	};
	unsigned vector;
	unsigned sf;
	unsigned lm;

	for (vector = 0; vector < sizeof(values) / sizeof(values[0]); ++vector) {
		psxRegisters state;
		set_boundary_state(&state,
			values[vector][0], values[vector][1], values[vector][2],
			values[vector][3], values[vector][4], values[vector][5],
			values[vector][6], 0xdeadbeefu ^ vector);
		for (sf = 0; sf < 2; ++sf)
			for (lm = 0; lm < 2; ++lm)
				compare_one("boundary", &state, sf, lm, vector);
	}
}

static void run_random_vectors(void)
{
	psxRegisters state;
	u32 random_state = 0x4d595df4u;
	unsigned vector;
	unsigned sf;
	unsigned lm;

	for (vector = 0; vector < 100000; ++vector) {
		fill_random_state(&state, &random_state);
		for (sf = 0; sf < 2; ++sf)
			for (lm = 0; lm < 2; ++lm)
				compare_one("random", &state, sf, lm, vector);
	}
}

int main(void)
{
	run_boundary_vectors();
	run_random_vectors();
	if (failures != 0) {
		fprintf(stderr, "GTE INTPL differential test failed: %u mismatches\n",
			failures);
		return 1;
	}
	printf("GTE INTPL differential test passed: 400044 cases\n");
	return 0;
}
