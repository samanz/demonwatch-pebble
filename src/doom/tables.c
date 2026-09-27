/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *  Copyright 2023-2026 by
 *  Frenkel Smeijers
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *      Lookup tables.
 *      Do not try to look them up :-).
 *      In the order of appearance:
 *
 *      int finesine[10240]             - Sine lookup.
 *       Guess what, serves as cosine, too.
 *       Remarkable thing is, how to use BAMs with this?
 *
 *-----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include "w_wad.h"
#include "tables.h"
#include "globdata.h"


extern void *malloc(size_t size);
uint16_t *finesineTable_part_1 = NULL;

void R_InitTables(void)
{
    static boolean s_tables_initialized = false;
    if (s_tables_initialized) return;
    s_tables_initialized = true;

    if (!finesineTable_part_1) {
        finesineTable_part_1 = (uint16_t*)malloc(2048 * sizeof(uint16_t));
    }

    int64_t s = 411776;       // sin(0.5 * pi / 4096) * 2^30
    int64_t c = 1073741745;   // cos(0.5 * pi / 4096) * 2^30
    const int64_t cd = 1073741544; // cos(pi / 4096) * 2^30
    const int64_t sd = 823552;     // sin(pi / 4096) * 2^30

    for (int i = 0; i < 2048; i++) {
        uint32_t val = (uint32_t)(s >> 14);
        if (val > 65535) val = 65535;
        finesineTable_part_1[i] = (uint16_t)val;
        int64_t ns = (s * cd + c * sd) >> 30;
        int64_t nc = (c * cd - s * sd) >> 30;
        s = ns;
        c = nc;
    }
}


#if defined DEMO_COMPATIBLE
#define finesine_part_1(a) finesineTable_part_1[a]

static uint16_t finesine_part_2(int16_t x)
{
	x = 4095 - x;
	switch (x) {
		case  273: return 13646; // 13647
		case  771: return 36556; // 36555
		case  883: return 41088; // 41087
		case 1080: return 48304; // 48305
		case 1827: return 64600; // 64601
		default: return finesineTable_part_1[x];
	}
}

static fixed_t finesine_part_3(int16_t x)
{
	x -= 4096;
	switch (x) {
		case   51: return  -2588; //  -2587
		case  863: return -40299; // -40300
		case 1078: return -48236; // -48237
		case 1080: return -48304; // -48305
		default: return 0xffff0000 | -finesineTable_part_1[x];
	}
}

static fixed_t finesine_part_4(int16_t x)
{
	x = 8191 - x;
	switch (x) {
		case   51: return  -2588; //  -2587
		case  114: return  -5747; //  -5748
		case  244: return -12217; // -12218
		case  455: return -22432; // -22433
		case  771: return -36556; // -36555
		case  795: return -37550; // -37551
		case  863: return -40299; // -40300
		case 1021: return -46251; // -46252
		case 1051: return -47307; // -47308
		case 1469: return -59189; // -59190
		default: return 0xffff0000 | -finesineTable_part_1[x];
	}
}


#define finecosine_part_1(a) finesine_part_2(a + (FINEANGLES / 4))
#define finecosine_part_2(a) finesine_part_3(a + (FINEANGLES / 4))
#define finecosine_part_3(a) finesine_part_4(a + (FINEANGLES / 4))

static uint16_t finecosine_part_4(int16_t x)
{
	x -= 6144;
	switch (x) {
		case   70: return  3542; //  3541
		case  114: return  5747; //  5748
		case  455: return 22432; // 22433
		case  629: return 30427; // 30426
		case  631: return 30516; // 30515
		case  771: return 36556; // 36555
		case  863: return 40299; // 40300
		case 1067: return 47861; // 47860
		case 1133: return 50064; // 50065
		case 1259: return 53912; // 53911
		case 1273: return 54309; // 54308
		case 1827: return 64600; // 64601
		default: return finesineTable_part_1[x];
	}
}

fixed_t finesine(int16_t x)
{
	if (x < 2048) {			//    0 <= x < 2048
		return finesine_part_1(x);
	} else if (x < 4096) {	// 2048 <= x < 4096
		return finesine_part_2(x);
	} else if (x < 6144) {	// 4096 <= x < 6144
		return finesine_part_3(x);
	} else {				// 6144 <= x < 8192
		return finesine_part_4(x);
	}
}


fixed_t finecosine(int16_t x)
{
	if (x < 2048) {			//    0 <= x < 2048
		return finecosine_part_1(x);
	} else if (x < 4096) {	// 2048 <= x < 4096
		return finecosine_part_2(x);
	} else if (x < 6144) {	// 4096 <= x < 6144
		return finecosine_part_3(x);
	} else {				// 6144 <= x < 8192
		return finecosine_part_4(x);
	}
}
#endif


fixed_t finesineapprox(int16_t x)
{
	if (x < 2048) {			//    0 <= x < 2048
		return finesineTable_part_1[x];
	} else if (x < 4096) {	// 2048 <= x < 4096
		return finesineTable_part_1[4095 - x];
	} else if (x < 6144) {	// 4096 <= x < 6144
		return 0xffff0000 | -finesineTable_part_1[x - 4096];
	} else {				// 6144 <= x < 8192
		return 0xffff0000 | -finesineTable_part_1[8191 - x];
	}
}


fixed_t finecosineapprox(int16_t x)
{
	if (x < 2048) {			//    0 <= x < 2048
		return finesineTable_part_1[4095 - (x + (FINEANGLES / 4))];
	} else if (x < 4096) {	// 2048 <= x < 4096
		return 0xffff0000 | -finesineTable_part_1[(x + (FINEANGLES / 4)) - 4096];
	} else if (x < 6144) {	// 4096 <= x < 6144
		return 0xffff0000 | -finesineTable_part_1[8191 - (x + (FINEANGLES / 4))];
	} else {				// 6144 <= x < 8192
		return finesineTable_part_1[x - 6144];
	}
}


const angle16_t xtoviewangleTable[VIEWWINDOWWIDTH + 1] = {
5637,5560,5482,5404,5325,5245,5164,5083,5002,4919,4836,4752,4668,4583,4497,4411,4323,4236,4147,4058,3969,3879,3788,3696,3604,3512,3418,3325,3230,3135,3040,2944,2848,2751,2653,2555,2457,2358,2259,2159,2059,1958,1858,1756,1655,1553,1451,1348,1246,1143,1040,936,833,729,625,521,417,313,209,104,0,65432,65327,65223,65119,65015,64911,64807,64703,64600,64496,64393,64290,64188,64085,63983,63881,63780,63678,63578,63477,63377,63277,63178,63079,62981,62883,62785,62688,62592,62496,62401,62306,62211,62118,62024,61932,61840,61748,61657,61567,61478,61389,61300,61213,61125,61039,60953,60868,60784,60700,60617,60534,60453,60372,60291,60211,60132,60054,59976,59899
};
