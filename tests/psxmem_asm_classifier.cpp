/*
 * Exhaustive model of the hardware-region classifier used by psxmem_asm.S.
 *
 * The assembly receives only t = mem >> 16.  The PSX hardware aliases are
 * exactly 0x1f80, 0x9f80, and 0xbf80; a masked high-half comparison must not
 * accidentally classify other values as hardware.
 */
#include <stdint.h>
#include <stdio.h>

static bool asm_classifier(uint32_t t) {
	return ((t & 0x7fffU) == 0x1f80U) || t == 0xbf80U;
}

static bool canonical_classifier(uint32_t t) {
	return t == 0x1f80U || t == 0x9f80U || t == 0xbf80U;
}

int main() {
	unsigned mismatches = 0;
	for (uint32_t t = 0; t <= 0xffffU; ++t) {
		if (asm_classifier(t) != canonical_classifier(t)) {
			if (mismatches++ < 8)
				fprintf(stderr, "classifier mismatch t=0x%04x\n",
				        (unsigned)t);
		}
	}

	/* Keep the three legal aliases and known false positives explicit. */
	const uint32_t aliases[] = {0x1f80U, 0x9f80U, 0xbf80U};
	for (unsigned i = 0; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
		if (!asm_classifier(aliases[i])) {
			fprintf(stderr, "alias rejected t=0x%04x\n", (unsigned)aliases[i]);
			++mismatches;
		}
	}
	const uint32_t false_positives[] = {0x1f81U, 0x3f80U, 0x7f80U,
	                                    0x9f81U, 0xbf81U, 0xffffU};
	for (unsigned i = 0; i < sizeof(false_positives) / sizeof(false_positives[0]); ++i) {
		if (asm_classifier(false_positives[i])) {
			fprintf(stderr, "false hardware match t=0x%04x\n",
			        (unsigned)false_positives[i]);
			++mismatches;
		}
	}

	if (mismatches != 0) {
		fprintf(stderr, "psxmem asm classifier: %u mismatches\n", mismatches);
		return 1;
	}
	puts("psxmem asm classifier: 65536 high halves verified");
	return 0;
}
