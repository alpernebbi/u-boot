/*
 * Convert UTF-8 bytes into a code page 437 character.
 * Based on the table in the Code_page_437 Wikipedia page.
 */

#include <linux/types.h>

uint8_t code_points_00a0[] = {
	255, 173, 155, 156,   7, 157,   7,  21,
	  7,   7, 166, 174, 170,   7,   7,   7,
	248, 241, 253,   7,   7, 230,  20, 250,
	  7,   7, 167, 175, 172, 171,   7, 168,
	  7,   7,   7,   7, 142, 143, 146, 128,
	  7, 144,   7,   7,   7,   7,   7,   7,
	  7, 165,   7,   7,   7,   7, 153,   7,
	  7,   7,   7,   7, 154,   7,   7, 225,
	133, 160, 131,   7, 132, 134, 145, 135,
	138, 130, 136, 137, 141, 161, 140, 139,
	  7, 164, 149, 162, 147,   7, 148, 246,
	  7, 151, 163, 150, 129,   7,   7, 152,
};

uint8_t code_points_2550[] = {
	205, 186, 213, 214, 201, 184, 183, 187,
	212, 211, 200, 190, 189, 188, 198, 199,
	204, 181, 182, 185, 209, 210, 203, 207,
	208, 202, 216, 215, 206
};

static uint8_t utf8_convert_11bit(uint16_t code)
{
	switch (code) {
	case 0x0192: return 159;
	case 0x0393: return 226;
	case 0x0398: return 233;
	case 0x03A3: return 228;
	case 0x03A6: return 232;
	case 0x03A9: return 234;
	case 0x03B1: return 224;
	case 0x03B4: return 235;
	case 0x03B5: return 238;
	case 0x03C0: return 227;
	case 0x03C3: return 229;
	case 0x03C4: return 231;
	case 0x03C6: return 237;
	}

	return 0;
};

static uint8_t utf8_convert_2xxx(uint16_t code)
{
	switch (code) {
	case 0x2022: return 7;
	case 0x203C: return 19;
	case 0x207F: return 252;
	case 0x20A7: return 158;
	case 0x2190: return 27;
	case 0x2191: return 24;
	case 0x2192: return 26;
	case 0x2193: return 25;
	case 0x2194: return 29;
	case 0x2195: return 18;
	case 0x21A8: return 23;
	case 0x2219: return 249;
	case 0x221A: return 251;
	case 0x221E: return 236;
	case 0x221F: return 28;
	case 0x2229: return 239;
	case 0x2248: return 247;
	case 0x2261: return 240;
	case 0x2264: return 243;
	case 0x2265: return 242;
	case 0x2310: return 169;
	case 0x2320: return 244;
	case 0x2321: return 245;
	case 0x2500: return 196;
	case 0x2502: return 179;
	case 0x250C: return 218;
	case 0x2510: return 191;
	case 0x2514: return 192;
	case 0x2518: return 217;
	case 0x251C: return 195;
	case 0x2524: return 180;
	case 0x252C: return 194;
	case 0x2534: return 193;
	case 0x253C: return 197;
	case 0x2580: return 223;
	case 0x2584: return 220;
	case 0x2588: return 219;
	case 0x258C: return 221;
	case 0x2590: return 222;
	case 0x2591: return 176;
	case 0x2592: return 177;
	case 0x2593: return 178;
	case 0x25A0: return 254;
	case 0x25AC: return 22;
	case 0x25B2: return 30;
	case 0x25BA: return 16;
	case 0x25BC: return 31;
	case 0x25C4: return 17;
	case 0x25CB: return 9;
	case 0x25D8: return 8;
	case 0x25D9: return 10;
	case 0x263A: return 1;
	case 0x263B: return 2;
	case 0x263C: return 15;
	case 0x2640: return 12;
	case 0x2642: return 11;
	case 0x2660: return 6;
	case 0x2663: return 5;
	case 0x2665: return 3;
	case 0x2666: return 4;
	case 0x266A: return 13;
	case 0x266B: return 14;
	}

	return 0;
}

uint8_t convert_uc16_to_cp437(uint16_t code)
{
	if (code < 0x7f)		// ASCII
		return code;
	if (code < 0xa0)		// high control characters
		return code;
	if (code < 0x100)		// international characters
		return code_points_00a0[code - 0xa0];
	if (code < 0x800)
		return utf8_convert_11bit(code);
	if (code >= 0x2550 && code < 0x256d)	// block graphics
		return code_points_2550[code - 0x2550];

	return utf8_convert_2xxx(code);
}

uint8_t convert_utf8_to_cp437(uint8_t c, uint32_t *esc)
{
	int shift;
	uint16_t ucs;

	if (c < 127)			// ASCII
		return c;
	if (c == 127)
		return 8;		// DEL (?)

	switch (c & 0xf0) {
	case 0xc0: case 0xd0:		// two bytes sequence
		*esc = (1U << 24) | ((c & 0x1f) << 6);
		return 0;
	case 0xe0:			// three bytes sequence
		*esc = (2U << 24) | ((c & 0x0f) << 12);
		return 0;
	case 0xf0:			// four bytes sequence
		*esc = (3U << 24) | ((c & 0x07) << 18);
		return 0;
	case 0x80: case 0x90: case 0xa0: case 0xb0:	// continuation
		shift = (*esc >> 24) - 1;
		ucs = *esc & 0xffffff;
		if (shift) {
			*esc = (shift << 24) | ucs | (c & 0x3f) << (shift * 6);
			return 0;
		}
		*esc = 0;
		return convert_uc16_to_cp437(ucs | (c & 0x3f));
	}

	return 0;
}
