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
 *      Rendering main loop and setup functions,
 *       utility functions (BSP, geometry, trigonometry).
 *      See tables.c, too.
 *
 *-----------------------------------------------------------------------------*/

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "compiler.h"
#include "d_player.h"
#include "w_wad.h"
#include "r_main.h"
#include "r_things.h"
#include "m_fixed.h"
#include "st_stuff.h"
#include "i_system.h"
#include "g_game.h"
#include "m_random.h"

#include "globdata.h"


// Silhouette, needed for clipping Segs (mainly)
// and sprites representing things.
#define SIL_NONE    0
#define SIL_BOTTOM  1
#define SIL_TOP     2
#define SIL_BOTH    3

typedef struct drawseg_s
{
  const seg_t __far* curline;
  int16_t x1, x2;
  fixed_t scale1, scale2, scalestep;
  int16_t silhouette;                       // 0=none, 1=bottom, 2=top, 3=both
  fixed_t bsilheight;                   // do not clip sprites above this
  fixed_t tsilheight;                   // do not clip sprites below this

  // Pointers to lists for sprite clipping,
  // all three adjusted so [x1] is first value.

  int16_t *sprtopclip, *sprbottomclip;
  int16_t *maskedtexturecol; // dropoff overflow
} drawseg_t;


#if VIEWWINDOWWIDTH * VIEWWINDOWHEIGHT <= 38 * 28
#define MAXDRAWSEGS    64
#else
#define MAXDRAWSEGS   64
#endif

static drawseg_t* _s_drawsegs = NULL;


#define MAXOPENINGS (VIEWWINDOWWIDTH*8)

static int16_t *openings;
static int16_t* lastopening;


#if VIEWWINDOWWIDTH == 240
#define VIEWANGLETOXMAX 1029
#elif VIEWWINDOWWIDTH == 120
#define VIEWANGLETOXMAX 1034
#elif VIEWWINDOWWIDTH == 80
#define VIEWANGLETOXMAX 1040
#elif VIEWWINDOWWIDTH == 60
#define VIEWANGLETOXMAX 1046
#elif VIEWWINDOWWIDTH == 40
#define VIEWANGLETOXMAX 1057
#elif VIEWWINDOWWIDTH == 38
#define VIEWANGLETOXMAX 1059
#elif VIEWWINDOWWIDTH == 30
#define VIEWANGLETOXMAX 1068
#else
#error unsupported VIEWWINDOWWIDTH value
#endif
static uint8_t* viewangletoxTable = NULL;


static uint8_t viewangletox(int16_t va) {
    int16_t angle = (va - 2048) * 8;
    int lo=0, hi=VIEWWINDOWWIDTH;
    while (lo < hi) {
        int mid=(lo+hi)/2;
        if ((int16_t)xtoviewangleTable[mid] > angle) lo=mid+1;
        else hi=mid;
    }
    return lo;
}


static angle_t* tantoangleTable = NULL;

static angle16_t* tantoangle16Table = NULL;

#define tantoangle(t) tantoangleTable[t]
#define tantoangle16(t) tantoangle16Table[(t)*2]


static uint16_t* finetangentTable_part_3 = NULL;
#if VIEWWINDOWWIDTH == 240
static const fixed_t  __far finetangentTable_part_4[1024];
#else
static fixed_t*  finetangentTable_part_4 = NULL;
#endif

static int16_t floorclip[VIEWWINDOWWIDTH];
static int16_t ceilingclip[VIEWWINDOWWIDTH];


static int16_t screenheightarray[VIEWWINDOWWIDTH] =
{
#if VIEWWINDOWWIDTH == 240
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT
#elif VIEWWINDOWWIDTH == 120
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT
#elif VIEWWINDOWWIDTH == 80
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT
#elif VIEWWINDOWWIDTH == 60
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT
#elif VIEWWINDOWWIDTH == 40
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT
#elif VIEWWINDOWWIDTH == 38
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,

	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT
#elif VIEWWINDOWWIDTH == 30
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT,
	VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT, VIEWWINDOWHEIGHT
#else
#error unsupported VIEWWINDOWWIDTH value
#endif
};

static int16_t negonearray[VIEWWINDOWWIDTH] =
{
#if VIEWWINDOWWIDTH == 240
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1
#elif VIEWWINDOWWIDTH == 120
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1
#elif VIEWWINDOWWIDTH == 80
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,

	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,

	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,

	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1
#elif VIEWWINDOWWIDTH == 60
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,

	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1
#elif VIEWWINDOWWIDTH == 40
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,

	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1
#elif VIEWWINDOWWIDTH == 38
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,

	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1, -1, -1,
	-1, -1
#elif VIEWWINDOWWIDTH == 30
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1
#else
#error unsupported VIEWWINDOWWIDTH value
#endif
};


//*****************************************
//Globals.
//*****************************************

int16_t numnodes;
const mapnode_t __far* nodes;

#if defined FLAT_SPAN
static fixed_t  viewx, viewy, viewz;
static fixed_t  viewcos, viewsin;
#else
fixed_t  viewx, viewy, viewz;
fixed_t  viewcos, viewsin;
#endif

angle_t  viewangle;
static angle16_t viewangle16;

static byte solidcol[VIEWWINDOWWIDTH];

static const seg_t     __far* curline;
static linedata_t    __far* linedef;
static const line_t *maplinedef;
static sector_t  __far* frontsector;
static sector_t  __far* backsector;
static drawseg_t *ds_p;

#if defined FLAT_SPAN
static int16_t floorplane_color;
static int16_t ceilingplane_color;
#else
static visplane_t __far* floorplane;
static visplane_t __far* ceilingplane;
#endif

static angle16_t             rw_angle1;

static angle16_t         rw_normalangle; // angle to line origin
static int16_t         rw_distance;

static int16_t      rw_stopx;

static fixed_t  rw_scale;
static fixed_t  rw_scalestep;

static int32_t      worldtop;
static int32_t      worldbottom;

static boolean didsolidcol; /* True if at least one column was marked solid */

static boolean  maskedtexture;
static int16_t      toptexture;
static int16_t      bottomtexture;
static int16_t      midtexture;
static const texture_t __far* textoptexture;
static const texture_t __far* texbottomtexture;
static const texture_t __far* texmidtexture;

static fixed_t  rw_midtexturemid;
static fixed_t  rw_toptexturemid;
static fixed_t  rw_bottomtexturemid;

const uint8_t *fullcolormap;
const uint8_t* fixedcolormap;

static int16_t extralight;                           // bumped light from gun blasts


static int16_t   *mfloorclip;   // dropoff overflow
static int16_t   *mceilingclip; // dropoff overflow
static fixed_t spryscale;
static fixed_t sprtopscreen;

static angle16_t  rw_centerangle;
static int16_t  rw_offset;
static int16_t      rw_lightlevel;

static int16_t      *maskedtexturecol; // dropoff overflow

const int16_t   __far* textureheight; //needed for texture pegging (and TFE fix - killough)


static fixed_t  topfrac;
static fixed_t  topstep;
static fixed_t  bottomfrac;
static fixed_t  bottomstep;

static fixed_t  pixhigh;
static fixed_t  pixlow;

static fixed_t  pixhighstep;
static fixed_t  pixlowstep;

static int32_t      worldhigh;
static int32_t      worldlow;


uint16_t validcount = 1;         // increment every time a check is made

//*****************************************
// Constants
//*****************************************

#define COLEXTRABITS (8 - 1)

static const int16_t CENTERX = VIEWWINDOWWIDTH  / 2;
       const int16_t CENTERY = VIEWWINDOWHEIGHT / 2;

static const fixed_t PROJECTION = 100L << FRACBITS;

static const uint16_t PSPRITESCALE  = FRACUNIT * VIEWWINDOWWIDTH / SCREENWIDTH_VGA;
static const fixed_t  PSPRITEISCALE = FRACUNIT * SCREENWIDTH_VGA / VIEWWINDOWWIDTH; // = FixedReciprocal(PSPRITESCALE)

static const uint16_t PSPRITEYSCALE = FRACUNIT * (VIEWWINDOWHEIGHT * 5 / 4) / SCREENHEIGHT_VGA;
static const uint16_t PSPRITEYFRACSTEP = (FRACUNIT * SCREENHEIGHT_VGA / (VIEWWINDOWHEIGHT * 5 / 4)) >> COLEXTRABITS; // = FixedReciprocal(PSPRITEYSCALE) >> COLEXTRABITS

static const angle16_t clipangle = 5637; // = xtoviewangleTable[0]


// Emits a mulu.w instruction. It's quite difficult to get gcc to do that :-)
static uint32_t mulu(uint16_t a, uint16_t b) {
#if C_ONLY
	return (uint32_t)a * b;
#else
	uint32_t result = a;
	__asm__ (
		"mulu.w %1, %0"
		: "+d" (result)
		: "d" (b)
	);
	return result;
#endif
}


#if defined __WATCOMC__
//
#else
inline
#endif
fixed_t CONSTFUNC FixedMul(fixed_t a, fixed_t b)
{
	// Is the result a negative number?
	uint32_t neg = (a ^ b) < 0 ? 0xffff : 0;

	uint16_t alw, ahw, blw, bhw;
	int32_t result;

	// Only work with unsigned numbers.
	a = D_abs(a);
	b = D_abs(b);
	alw = a;
	ahw = a >> FRACBITS;
	blw = b;
	bhw = b >> FRACBITS;

	if (bhw == 0) {
		uint32_t hl = mulu(ahw, blw);

		// Make sure we round towards -inf
		uint32_t ll = (mulu(alw, blw) + neg) >> FRACBITS;

		result = hl + ll;
	} else {
		uint32_t hh = mulu(ahw, bhw) << FRACBITS;
		uint32_t hl = mulu(ahw, blw);
		uint32_t lh = mulu(alw, bhw);

		// Make sure we round towards -inf
		uint32_t ll = (mulu(alw, blw) + neg) >> FRACBITS;

		result = hh + hl + lh + ll;
	}

	if (neg) result = -result;
	return result;
}


inline static fixed_t CONSTFUNC FixedMul3232(fixed_t a, fixed_t b)
{
	// Is the result a negative number?
	uint32_t neg = (a ^ b) < 0 ? 0xffff : 0;

	uint16_t alw, ahw, blw, bhw;
	uint32_t hh, hl, lh, ll;
	int32_t result;

	// Only work with unsigned numbers.
	a = D_abs(a);
	b = D_abs(b);
	alw = a;
	ahw = a >> FRACBITS;
	blw = b;
	bhw = b >> FRACBITS;

	hh = mulu(ahw, bhw) << FRACBITS;
	hl = mulu(ahw, blw);
	lh = mulu(alw, bhw);

	// Make sure we round towards -inf
	ll = (mulu(alw, blw) + neg) >> FRACBITS;

	result = hh + hl + lh + ll;
	if (neg) result = -result;
	return result;
}


//
// FixedMulAngle
// b should be coming from finesine() or finecosine(), so its high word is either 0x0000 or 0xffff
//
#if defined __WATCOMC__
//
#else
inline
#endif
fixed_t CONSTFUNC FixedMulAngle(fixed_t a, fixed_t b)
{
	// Is the result a negative number?
	uint32_t neg = (a ^ b) < 0 ? 0xffff : 0;

	uint16_t alw, ahw, blw;
	uint32_t hl, ll;
	int32_t result;

	// Only work with unsigned numbers.
	a = D_abs(a);
	b = D_abs(b);
	alw = a;
	ahw = a >> FRACBITS;
	blw = b;

	hl = mulu(ahw, blw);

	// Make sure we round towards -inf
	ll = (mulu(alw, blw) + neg) >> FRACBITS;

	result = hl + ll;

	if (neg) result = -result;
	return result;
}


#if defined __WATCOMC__
//
#else
inline
#endif
fixed_t CONSTFUNC FixedMul3216(fixed_t a, uint16_t blw)
{
	boolean neg = a < 0;

	uint16_t alw, ahw;
	uint32_t ll, hl;
	fixed_t r;

	a = D_abs(a);

	alw = a;
	ahw = a >> FRACBITS;

	ll = mulu(alw, blw);
	hl = mulu(ahw, blw);
	r = (ll >> FRACBITS) + hl;
	if (neg) r = -r;
	return r;
}


//Approx fixed point divide of a/b using reciprocal. -> a * (1/b).
#if defined __WATCOMC__
//
#else
inline
#endif
fixed_t CONSTFUNC FixedApproxDiv(fixed_t a, fixed_t b)
{
	if (b <= 0xffffu)
		return FixedMul3232(a, FixedReciprocalSmall(b));
	else
		return FixedMul3216(a, FixedReciprocalBig(b));
}


//
// R_PointOnSide
// Traverse BSP (sub) tree,
//  check point against partition plane.
// Returns side 0 (front) or 1 (back).
//

static PUREFUNC int16_t R_PointOnSide(fixed_t x, fixed_t y, const mapnode_t __far* node)
{
	int16_t ix = x >> FRACBITS;

	if (!node->dx)
		return ix <= node->x ? node->dy > 0 : node->dy < 0;

	int16_t iy = y >> FRACBITS;

	if (!node->dy)
		return iy <= node->y ? node->dx < 0 : node->dx > 0;

	x -= (fixed_t)node->x << FRACBITS;
	y -= (fixed_t)node->y << FRACBITS;

	ix = x >> FRACBITS;
	iy = y >> FRACBITS;

	// Try to quickly decide by looking at sign bits.
	if ((node->dy ^ node->dx ^ ix ^ iy) < 0)
		return (node->dy ^ ix) < 0;  // (left is negative)

	//return FixedMul(y, node->dx) >= FixedMul(x, node->dy);
	return (y >> 8) * node->dx >= (x >> 8) * node->dy;
}

//
// R_PointInSubsector
//

subsector_t __far* R_PointInSubsector(fixed_t x, fixed_t y)
{
	fixed_t prevx;
	fixed_t prevy;
	static subsector_t __far* prevr;

	int16_t nodenum = numnodes-1;

	// special case for trivial maps (single subsector, no nodes)
	if (numnodes == 0)
	{
		prevr = _g_subsectors;
		return prevr;
	}

	while (!(nodenum & NF_SUBSECTOR))
		nodenum = nodes[nodenum].children[R_PointOnSide(x, y, nodes+nodenum)];

	prevr = &_g_subsectors[(int16_t)(nodenum & ~NF_SUBSECTOR)];
	return prevr;
}


#define SLOPERANGE 2048

static CONSTFUNC int16_t SlopeDiv(uint32_t num, uint32_t den)
{
    den = den >> 8;

    if (den == 0)
        return SLOPERANGE;

    const uint16_t ans = (num << 3) / den;//FixedApproxDiv(num << 3, den) >> FRACBITS;

    return (ans <= SLOPERANGE) ? ans : SLOPERANGE;
}


static CONSTFUNC int16_t SlopeDiv16(uint16_t n, uint16_t d)
{
	if (d == 0)
		return SLOPERANGE;

	const uint16_t ans = ((uint32_t)n * SLOPERANGE) / d;

	return (ans <= SLOPERANGE) ? ans : SLOPERANGE;
}


//
// R_PointToAngle
// To get a global angle from cartesian coordinates,
//  the coordinates are flipped until they are in
//  the first octant of the coordinate system, then
//  the y (<=x) is scaled and divided by x to get a
//  tangent (slope) value which is looked up in the
//  tantoangleTable[] table.
//


CONSTFUNC angle_t R_PointToAngle3(fixed_t x, fixed_t y)
{
    if ( (!x) && (!y) )
        return 0;

    if (x>= 0)
    {
        // x >=0
        if (y>= 0)
        {
            // y>= 0

            if (x>y)
            {
                // octant 0
                return tantoangle(SlopeDiv(y,x));
            }
            else
            {
                // octant 1
                return ANG90-1-tantoangle(SlopeDiv(x,y));
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x>y)
            {
                // octant 8
                return -tantoangle(SlopeDiv(y,x));
            }
            else
            {
                // octant 7
                return ANG270+tantoangle(SlopeDiv(x,y));
            }
        }
    }
    else
    {
        // x<0
        x = -x;

        if (y>= 0)
        {
            // y>= 0
            if (x>y)
            {
                // octant 3
                return ANG180-1-tantoangle(SlopeDiv(y,x));
            }
            else
            {
                // octant 2
                return ANG90+ tantoangle(SlopeDiv(x,y));
            }
        }
        else
        {
            // y<0
            y = -y;

            if (x>y)
            {
                // octant 4
                return ANG180+tantoangle(SlopeDiv(y,x));
            }
            else
            {
                // octant 5
                return ANG270-1-tantoangle(SlopeDiv(x,y));
            }
        }
    }
}


#define R_PointToAngle(x,y) R_PointToAngle16((x)>>FRACBITS,(y)>>FRACBITS)


static angle16_t R_PointToAngle16(int16_t x, int16_t y)
{
    x = x - (viewx >> FRACBITS);
    y = y - (viewy >> FRACBITS);

    if (!x && !y)
        return 0;

    if (x >= 0)
    {
        // x >= 0
        if (y >= 0)
        {
            // y >= 0

            if (x > y)
            {
                // octant 0
                return tantoangle16(SlopeDiv16(y, x));
            }
            else
            {
                // octant 1
                return ANG90_16 - 1 - tantoangle16(SlopeDiv16(x, y));
            }
        }
        else
        {
            // y < 0
            y = -y;

            if (x > y)
            {
                // octant 8
                return -tantoangle16(SlopeDiv16(y, x));
            }
            else
            {
                // octant 7
                return ANG270_16 + tantoangle16(SlopeDiv16(x, y));
            }
        }
    }
    else
    {
        // x < 0
        x = -x;

        if (y >= 0)
        {
            // y >= 0
            if (x > y)
            {
                // octant 3
                return ANG180_16 - 1 - tantoangle16(SlopeDiv16(y, x));
            }
            else
            {
                // octant 2
                return ANG90_16 + tantoangle16(SlopeDiv16(x, y));
            }
        }
        else
        {
            // y < 0
            y = -y;

            if (x > y)
            {
                // octant 4
                return ANG180_16 + tantoangle16(SlopeDiv16(y, x));
            }
            else
            {
                // octant 5
                return ANG270_16 - 1 - tantoangle16(SlopeDiv16(x, y));
            }
        }
    }
}


#define SLOPEBITS    11
#define DBITS      (FRACBITS-SLOPEBITS)

static CONSTFUNC int16_t R_PointToDist(int16_t x, int16_t y)
{
    if (viewx == (fixed_t)x << FRACBITS && viewy == (fixed_t)y << FRACBITS)
        return 0;

    fixed_t dx = D_abs(((fixed_t)x << FRACBITS) - viewx);
    fixed_t dy = D_abs(((fixed_t)y << FRACBITS) - viewy);

    if (dy > dx)
    {
        fixed_t t = dx;
        dx = dy;
        dy = t;
    }

    return dx / finecosineapprox((FixedApproxDiv(dy,dx) >> DBITS) / 2);
}


// Lighting constants.

#define LIGHTSEGSHIFT      4


// Number of diminishing brightness levels.
// There a 0-31, i.e. 32 LUT in the COLORMAP lump.

#define NUMCOLORMAPS 32


const uint8_t* R_LoadColorMap(int16_t lightlevel)
{
    return fullcolormap; // Bright watch palette; one 256-byte map.

}


//
// R_DrawMaskedColumn
// Used for sprites and masked mid textures.
// Masked means: partly transparent, i.e. stored
//  in posts/runs of opaque pixels.
//

typedef void (*R_DrawColumn_f)(const draw_column_vars_t *dcvars);

static void R_DrawMaskedColumn(R_DrawColumn_f colfunc, draw_column_vars_t *dcvars, const column_t __far* column)
{
    const fixed_t basetexturemid = dcvars->texturemid;

    const int16_t fclip_x = mfloorclip[dcvars->x];
    const int16_t cclip_x = mceilingclip[dcvars->x];

    while (column->topdelta != 0xff)
    {
        // calculate unclipped screen coordinates for post
        const int32_t topscreen = sprtopscreen + spryscale*column->topdelta;
        const int32_t bottomscreen = topscreen + spryscale*column->length;

        int16_t yh = (bottomscreen-1)>>FRACBITS;
        int16_t yl = (topscreen+FRACUNIT-1)>>FRACBITS;

        if (yh >= fclip_x)
            yh = fclip_x - 1;

        if (yl <= cclip_x)
            yl = cclip_x + 1;

        // killough 3/2/98, 3/27/98: Failsafe against overflow/crash:
        if (yl <= yh && yh < VIEWWINDOWHEIGHT)
        {
            dcvars->source =  (const byte __far*)column + 3;

            dcvars->texturemid = basetexturemid - (((int32_t)column->topdelta)<<FRACBITS);

            dcvars->yh = yh;
            dcvars->yl = yl;

            // Drawn by either R_DrawColumn or (SHADOW) R_DrawFuzzColumn.
            colfunc (dcvars);
        }

        column = (const column_t __far*)((const byte __far*)column + column->length + 4);
    }

    dcvars->texturemid = basetexturemid;
}


//
// R_InitColormaps
//
void R_InitColormaps(void)
{
	fullcolormap = W_GetLumpByName("COLORMAP");
}


//
// A vissprite_t is a thing that will be drawn during a refresh.
// i.e. a sprite object that is partly visible.
//

typedef struct vissprite_s
{
  int16_t x1, x2;
  fixed_t gx, gy;              // for line side calculation
  fixed_t gz;                   // global bottom for silhouette clipping
  fixed_t startfrac;           // horizontal position of x1
  fixed_t scale;
  fixed_t xiscale;             // negative if flipped
  fixed_t texturemid;
  uint16_t fracstep;

  int16_t lump_num;
  int16_t patch_topoffset;

  // for color translation and shadow draw, maxbright frames as well
  const uint8_t* colormap;

} vissprite_t;


void R_DrawFuzzColumn (const draw_column_vars_t *dcvars);


//
// R_DrawVisSprite
//  mfloorclip and mceilingclip should also be set.
//
// CPhipps - new wad lump handling, *'s to const*'s
static void R_DrawVisSprite(const vissprite_t *vis)
{
    fixed_t  frac;

    R_DrawColumn_f colfunc = R_DrawColumnSprite;
    draw_column_vars_t dcvars;
    dcvars.colormap = vis->colormap;

    // killough 4/11/98: rearrange and handle translucent sprites
    // mixed with translucent/non-translucenct 2s normals

    if (!dcvars.colormap)   // NULL colormap = shadow draw
        colfunc = R_DrawFuzzColumn;    // killough 3/14/98

    // proff 11/06/98: Changed for high-res
    dcvars.fracstep = vis->fracstep;
    dcvars.texturemid = vis->texturemid;
    frac = vis->startfrac;

    spryscale = vis->scale;
    sprtopscreen = CENTERY * FRACUNIT - FixedMul(dcvars.texturemid, spryscale);


    const patch_t __far* patch = W_GetLumpByNum(vis->lump_num);

    dcvars.x = vis->x1;

    while (dcvars.x < VIEWWINDOWWIDTH)
    {
        const column_t __far* column = (const column_t __far*) ((const byte __far*)patch + patch->columnofs[frac >> FRACBITS]);
        R_DrawMaskedColumn(colfunc, &dcvars, column);

        frac += vis->xiscale;

        if(((frac >> FRACBITS) >= patch->width) || frac < 0)
            break;

        dcvars.x++;
    }
}


static void R_GetColumn(const texture_t __far* texture, int16_t texcolumn, int16_t* patch_num, int16_t* x_c)
{
	const uint8_t patchcount = texture->patchcount;

	const int16_t xc = texcolumn & texture->widthmask;

	if (patchcount == 1)
	{
		//simple texture.
		*patch_num = texture->patches[0].patch_num;
		*x_c = xc;
	}
	else
	{
		uint8_t i = 0;

		do
		{
			const texpatch_t __far* patch = &texture->patches[i];

			int16_t x = xc - patch->originx;
			if (0 <= x && x < patch->patch_width)
			{
				*patch_num = patch->patch_num;
				*x_c = x;
				break;
			}
		} while (++i < patchcount);
	}
}


//
// R_RenderMaskedSegRange
//

static void R_RenderMaskedSegRange(const drawseg_t *ds, int16_t x1, int16_t x2)
{
	draw_column_vars_t dcvars;

	// Calculate light table.
	// Use different light tables
	//   for horizontal / vertical / diagonal. Diagonal?

	curline = ds->curline;  // OPTIMIZE: get rid of LIGHTSEGSHIFT globally

	frontsector = &_g_sectors[curline->frontsectornum];
	backsector  = &_g_sectors[curline->backsectornum];

	int16_t texnum = R_GetTextureTranslation(_g_sides[curline->sidenum].midtexture);

	// killough 4/13/98: get correct lightlevel for 2s normal textures
	rw_lightlevel = frontsector->lightlevel;

	maskedtexturecol = ds->maskedtexturecol;
	rw_scalestep     = ds->scalestep;
	spryscale        = ds->scale1 + (x1 - ds->x1) * rw_scalestep;
	mfloorclip       = ds->sprbottomclip;
	mceilingclip     = ds->sprtopclip;

	// find positioning
	if (_g_maplines[curline->linenum].flags & ML_DONTPEGBOTTOM)
	{
		dcvars.texturemid = frontsector->floorheight > backsector->floorheight ? frontsector->floorheight : backsector->floorheight;
		dcvars.texturemid = dcvars.texturemid + ((int32_t)textureheight[texnum] << FRACBITS) - viewz;
	}
	else
	{
		dcvars.texturemid =frontsector->ceilingheight<backsector->ceilingheight ? frontsector->ceilingheight : backsector->ceilingheight;
		dcvars.texturemid = dcvars.texturemid - viewz;
	}

	dcvars.texturemid += (((int32_t)_g_mapsides[curline->sidenum].rowoffset) << FRACBITS);

	dcvars.colormap = R_LoadColorMap(rw_lightlevel);

	const texture_t __far* texture = R_GetTexture(texnum);

	const uint16_t widthmask = texture->widthmask;

	// draw the columns
	// simple texture == 1 patch
	const patch_t __far* patch = W_GetLumpByNum(texture->patches[0].patch_num);

	for (dcvars.x = x1 ; dcvars.x <= x2 ; dcvars.x++, spryscale += rw_scalestep)
	{
		int16_t xc = maskedtexturecol[dcvars.x];

		if (xc != SHRT_MAX) // dropoff overflow
		{
			xc &= widthmask;

			sprtopscreen = CENTERY * FRACUNIT - FixedMul(dcvars.texturemid, spryscale);

			dcvars.fracstep = FixedReciprocal((uint32_t)spryscale) >> COLEXTRABITS;

			// draw the texture
			const column_t __far* column = (const column_t __far*) ((const byte __far*)patch + patch->columnofs[xc]);

			R_DrawMaskedColumn(R_DrawColumnWall, &dcvars, column);
			maskedtexturecol[dcvars.x] = SHRT_MAX; // dropoff overflow
		}
	}

	curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_LoadColorMap doesn't try using it for other things */
}


static PUREFUNC boolean R_PointOnSegSide(fixed_t x, fixed_t y, const seg_t __far* line)
{
    const int16_t lx = line->v1.x;
    const int16_t ly = line->v1.y;
    const int16_t ldx = line->v2.x - lx;
    const int16_t ldy = line->v2.y - ly;

    if (!ldx)
        return x <= (fixed_t)lx << FRACBITS ? ldy > 0 : ldy < 0;

    if (!ldy)
        return y <= (fixed_t)ly << FRACBITS ? ldx < 0 : ldx > 0;

    x -= (fixed_t)lx << FRACBITS;
    y -= (fixed_t)ly << FRACBITS;

    // Try to quickly decide by looking at sign bits.
    if ((ldy ^ ldx ^ (x >> FRACBITS) ^ (y >> FRACBITS)) < 0)
        return (ldy ^ (x >> FRACBITS)) < 0;          // (left is negative)

    return FixedMul3216(y, ldx) >= FixedMul3216(x, ldy);
}


//
// R_DrawSprite
//

static void R_DrawSprite (const vissprite_t* spr)
{
    int16_t* clipbot = floorclip;
    int16_t* cliptop = ceilingclip;

    fixed_t scale;
    fixed_t lowscale;

    for (int16_t x = spr->x1; x <= spr->x2; x++)
    {
        clipbot[x] = VIEWWINDOWHEIGHT;
        cliptop[x] = -1;
    }


    // Scan drawsegs from end to start for obscuring segs.
    // The first drawseg that has a greater scale is the clip seg.

    // Modified by Lee Killough:
    // (pointer check was originally nonportable
    // and buggy, by going past LEFT end of array):

    const drawseg_t* drawsegs  =_s_drawsegs;

    for (const drawseg_t* ds = ds_p; ds-- > drawsegs; )  // new -- killough
    {
        // determine if the drawseg obscures the sprite
        if (ds->x1 > spr->x2 || ds->x2 < spr->x1 || (!ds->silhouette && !ds->maskedtexturecol))
            continue;      // does not cover sprite

        const int16_t r1 = ds->x1 < spr->x1 ? spr->x1 : ds->x1;
        const int16_t r2 = ds->x2 > spr->x2 ? spr->x2 : ds->x2;

        if (ds->scale1 > ds->scale2)
        {
            lowscale = ds->scale2;
            scale    = ds->scale1;
        }
        else
        {
            lowscale = ds->scale1;
            scale    = ds->scale2;
        }

        if (scale < spr->scale || (lowscale < spr->scale && !R_PointOnSegSide (spr->gx, spr->gy, ds->curline)))
        {
            if (ds->maskedtexturecol)       // masked mid texture?
                R_RenderMaskedSegRange(ds, r1, r2);

            continue;               // seg is behind sprite
        }

        // clip this piece of the sprite
        // killough 3/27/98: optimized and made much shorter

        fixed_t gzt = spr->gz + (((int32_t)spr->patch_topoffset) << FRACBITS);

        if ((ds->silhouette & SIL_BOTTOM && spr->gz < ds->bsilheight)  // bottom sil
         && (ds->silhouette & SIL_TOP    && gzt     > ds->tsilheight)) // top sil
        {
            for (int16_t x = r1; x <= r2; x++)
            {
                if (clipbot[x] == VIEWWINDOWHEIGHT)
                    clipbot[x] = ds->sprbottomclip[x];

                if (cliptop[x] == -1)
                    cliptop[x] = ds->sprtopclip[x];
            }
        }
        else if (ds->silhouette & SIL_BOTTOM && spr->gz < ds->bsilheight) // bottom sil
        {
            for (int16_t x = r1; x <= r2; x++)
            {
                if (clipbot[x] == VIEWWINDOWHEIGHT)
                    clipbot[x] = ds->sprbottomclip[x];
            }
        }
        else if (ds->silhouette & SIL_TOP && gzt > ds->tsilheight) // top sil
        {
            for (int16_t x = r1; x <= r2; x++)
            {
                if (cliptop[x] == -1)
                    cliptop[x] = ds->sprtopclip[x];
            }
        }
    }

    // all clipping has been performed, so draw the sprite
    mfloorclip   = clipbot;
    mceilingclip = cliptop;
    R_DrawVisSprite (spr);
}


/*
 * Frame flags:
 * handles maximum brightness (torches, muzzle flare, light sources)
 */

#define FF_FULLBRIGHT   0x8000  /* flag in thing->frame */
#define FF_FRAMEMASK    0x7fff


//
// R_DrawPSprite
//

#define BASEXCENTER (SCREENWIDTH_VGA  / 2)
#define BASEYCENTER (SCREENHEIGHT_VGA / 2L)

static void R_DrawPSprite (pspdef_t *psp, int16_t lightlevel)
{
    int16_t           x1, x2;
    int32_t hl;
    spritedef_t   __far* sprdef;
    spriteframe_t __far* sprframe;
    vissprite_t   *vis;
    vissprite_t   avis;
    fixed_t       topoffset;

    // decide which patch to use
    sprdef = &sprites[psp->state->sprite];

    sprframe = &sprdef->spriteframes[psp->state->frame & FF_FRAMEMASK];

    const patch_t __far* patch = W_GetLumpByNum(sprframe->lump[0]);
    // calculate edges of the shape
    int16_t tx = psp->sx;

    tx -= patch->leftoffset;
    hl = (int32_t) tx * PSPRITESCALE;
    x1 = CENTERX + (hl >> FRACBITS);

    tx += patch->width;
    hl = (int32_t) tx * PSPRITESCALE;
    x2 = CENTERX + (hl >> FRACBITS) - 1;

    // off the side
    if (x2 < 0 || x1 > VIEWWINDOWWIDTH)
    {
        return;
    }

    topoffset = ((int32_t)patch->topoffset) << FRACBITS;

    // store information in a vissprite
    vis = &avis;
    // killough 12/98: fix psprite positioning problem
    vis->texturemid = (BASEYCENTER<<FRACBITS) /* +  FRACUNIT/2 */ -
            (psp->sy-topoffset);
    vis->x1 = x1 < 0 ? 0 : x1;
    vis->x2 = x2 >= VIEWWINDOWWIDTH ? VIEWWINDOWWIDTH - 1 : x2;
    // proff 11/06/98: Added for high-res
    vis->scale = PSPRITEYSCALE;
    vis->fracstep = PSPRITEYFRACSTEP;

    vis->xiscale = PSPRITEISCALE;
    vis->startfrac = 0;

    if (vis->x1 > x1)
        vis->startfrac = vis->xiscale*(vis->x1-x1);

    vis->lump_num = sprframe->lump[0];

    if (_g_player.powers[pw_invisibility] > 4*32 || _g_player.powers[pw_invisibility] & 8)
        vis->colormap = NULL;                    // shadow draw
    else if (fixedcolormap)
        vis->colormap = fixedcolormap;           // fixed color
    else if (psp->state->frame & FF_FULLBRIGHT)
        vis->colormap = fullcolormap;            // full bright // killough 3/20/98
    else
        vis->colormap = R_LoadColorMap(lightlevel);  // local light

    R_DrawVisSprite(vis);
}



//
// R_DrawPlayerSprites
//

static void R_DrawPlayerSprites(void)
{

  int16_t i, lightlevel = _g_player.mo->subsector->sector->lightlevel;
  pspdef_t *psp;

  // clip to screen bounds
  mfloorclip   = screenheightarray;
  mceilingclip = negonearray;

  // add all active psprites
  for (i=0, psp=_g_player.psprites; i<NUMPSPRITES; i++,psp++)
    if (psp->state)
      R_DrawPSprite (psp, lightlevel);
}


//
// R_SortVisSprites
//

// insertion sort
static void isort(vissprite_t **s, int16_t n)
{
	for (int16_t i = 1; i < n; i++)
	{
		vissprite_t *temp = s[i];
		if (s[i - 1]->scale < temp->scale)
		{
			int16_t j = i;
			while ((s[j] = s[j - 1])->scale < temp->scale && --j)
				;
			s[j] = temp;
		}
	}
}

#define MAXVISSPRITES 8
static int16_t num_vissprite;
static vissprite_t vissprites[MAXVISSPRITES];
static vissprite_t* vissprite_ptrs[MAXVISSPRITES];

static void R_SortVisSprites (void)
{
    int16_t i = num_vissprite;

    if (i)
    {
        while (--i >= 0)
            vissprite_ptrs[i] = vissprites + i;

        isort(vissprite_ptrs, num_vissprite);
    }
}

//
// R_DrawMasked
//

static void R_DrawMasked(void)
{
    drawseg_t *ds;
    drawseg_t* drawsegs = _s_drawsegs;


    R_SortVisSprites();

    // draw all vissprites back to front
    for (int16_t i = num_vissprite; --i >= 0; )
        R_DrawSprite(vissprite_ptrs[i]);

    // render any remaining masked mid textures

    for (ds=ds_p ; ds-- > drawsegs ; )
        if (ds->maskedtexturecol)
            R_RenderMaskedSegRange(ds, ds->x1, ds->x2);

    R_DrawPlayerSprites ();
}


//
// R_NewVisSprite
//
static vissprite_t *R_NewVisSprite(void)
{
    if (num_vissprite >= MAXVISSPRITES)
    {
#ifdef RANGECHECK
        I_Error("Vissprite overflow.");
#endif
        return NULL;
    }

    return vissprites + num_vissprite++;
}


//
// R_ClearSprites
// Called at frame start.
//

static void R_ClearSprites(void)
{
    num_vissprite = 0;
}


//*******************************************

//
// R_ScaleFromGlobalAngle
// Returns the texture mapping scale
//  for the current line (horizontal span)
//  at the given angle.
// rw_distance must be calculated first.
//

static fixed_t R_ScaleFromGlobalAngle(int16_t x)
{
  int16_t anglea = ANG90_16 + xtoviewangleTable[x];
  int16_t angleb = anglea + viewangle16 - rw_normalangle;

  fixed_t den = rw_distance * finesineapprox(anglea >> ANGLETOFINESHIFT_16);

// proff 11/06/98: Changed for high-res
  fixed_t num = 100 * finesineapprox(angleb >> ANGLETOFINESHIFT_16);

  return den > num>>16 ? (num = FixedApproxDiv(num, den)) > 64*FRACUNIT ?
    64*FRACUNIT : num < 256 ? 256 : num : 64*FRACUNIT;
}


//
// R_ProjectSprite
// Generates a vissprite for a thing if it might be visible.
//

#define MINZ        (FRACUNIT*4)
#define MAXZ        (FRACUNIT*1280)

#define SPR_FLIPPED(s, r) (s->flipmask & (1 << r))

static void R_ProjectSprite (mobj_t __far* thing, int16_t lightlevel)
{
    const fixed_t fx = thing->x;
    const fixed_t fy = thing->y;
    const fixed_t fz = thing->z;

    const fixed_t tr_x = fx - viewx;
    const fixed_t tr_y = fy - viewy;

    fixed_t xc = FixedMulAngle(tr_x, viewcos);
    fixed_t ys = FixedMulAngle(tr_y, viewsin);
    const fixed_t tz = xc - (-ys);

    // thing is behind view plane?
    if (tz < MINZ)
        return;

    //Too far away.
    if(tz > MAXZ)
        return;

    fixed_t yc = FixedMulAngle(tr_y, viewcos);
    fixed_t xs = FixedMulAngle(tr_x, viewsin);
    fixed_t tx = -(yc + (-xs));

    // too far off the side?
    if (D_abs(tx)>(tz<<2))
        return;

    // decide which patch to use for sprite relative to player
    const spritedef_t __far*   sprdef   = &sprites[thing->sprite];
    const spriteframe_t __far* sprframe = &sprdef->spriteframes[thing->frame & FF_FRAMEMASK];

    uint16_t rot = 0;

    if (sprframe->rotate)
    {
        // choose a different rotation based on player view
        angle16_t ang = R_PointToAngle(fx, fy);
        rot = (angle16_t)(ang - (angle16_t)(thing->angle >> FRACBITS) + (angle16_t)(ANG45_16 / 2) * 9) >> 13;
    }

    const boolean flip = (boolean)SPR_FLIPPED(sprframe, rot);
    const patch_t __far* patch = W_GetLumpByNum(sprframe->lump[rot]);

    /* calculate edges of the shape
     * cph 2003/08/1 - fraggle points out that this offset must be flipped
     * if the sprite is flipped; e.g. FreeDoom imp is messed up by this. */
    if (flip)
        tx -= ((int32_t)(patch->width - patch->leftoffset)) << FRACBITS;
    else
        tx -= ((int32_t)patch->leftoffset) << FRACBITS;

    //const fixed_t xscale = FixedDiv(PROJECTION, tz);
    const fixed_t xscale = PROJECTION / (tz >> FRACBITS);

    fixed_t xl = CENTERX * FRACUNIT + FixedMul(tx,xscale);
    const int16_t x1 = (xl >> FRACBITS);

    // off the side?
    if (x1 > VIEWWINDOWWIDTH)
    {
        return;
    }

    fixed_t xr = CENTERX * FRACUNIT - FRACUNIT + FixedMul(tx + (((int32_t)patch->width) << FRACBITS), xscale);
    const int16_t x2 = (xr >> FRACBITS);

    // off the side?
    if (xr < 0)
    {
        return;
    }

    //Too small.
    if (xr <= (xl + (FRACUNIT >> 2)))
    {
        return;
    }


    // store information in a vissprite
    vissprite_t* vis = R_NewVisSprite ();

    //No more vissprites.
    if(!vis)
    {
        return;
    }

    //vis->scale           = FixedDiv(PROJECTIONY, tz);
    vis->scale           = (100 * FRACUNIT) / (tz >> FRACBITS);
    vis->fracstep        = tz / (100 << COLEXTRABITS);
    vis->lump_num        = sprframe->lump[rot];
    vis->patch_topoffset = patch->topoffset;
    vis->gx              = fx;
    vis->gy              = fy;
    vis->gz              = fz;
    vis->texturemid      = (fz + (((int32_t)patch->topoffset) << FRACBITS)) - viewz;
    vis->x1              = x1 < 0 ? 0 : x1;
    vis->x2              = x2 >= VIEWWINDOWWIDTH ? VIEWWINDOWWIDTH - 1 : x2;


    const fixed_t iscale = FixedReciprocal(xscale);

    if (flip)
    {
        vis->startfrac = (((int32_t)patch->width)<<FRACBITS)-1;
        vis->xiscale = -iscale;
    }
    else
    {
        vis->startfrac = 0;
        vis->xiscale = iscale;
    }

    if (vis->x1 > x1)
        vis->startfrac += vis->xiscale*(vis->x1-x1);

    // get light level
    if (thing->flags & MF_SHADOW)
        vis->colormap = NULL;             // shadow draw
    else if (fixedcolormap)
        vis->colormap = fixedcolormap;      // fixed map
    else if (thing->frame & FF_FULLBRIGHT)
        vis->colormap = fullcolormap;     // full bright  // killough 3/20/98
    else
        vis->colormap = R_LoadColorMap(lightlevel); // diminished light
}

//
// R_AddSprites
// During BSP traversal, this adds sprites by sector.
//
// killough 9/18/98: add lightlevel as parameter, fixing underwater lighting
static void R_AddSprites(subsector_t __far* subsec, int16_t lightlevel)
{
  sector_t __far* sec=subsec->sector;
  mobj_t __far* thing;

  // BSP is traversed by subsector.
  // A sector might have been split into several
  //  subsectors during BSP building.
  // Thus we check whether its already added.

  if (sec->validcount == validcount)
    return;

  // Well, now it will be done.
  sec->validcount = validcount;

  // Handle all things in sector.

  for (thing = sec->thinglist; thing; thing = thing->snext)
    R_ProjectSprite(thing, lightlevel);
}


#if defined FLAT_WALL
#define R_DrawSegTextureColumn(w,x,y,z) R_DrawColumnFlat(x,z)
#else
static void R_DrawColumnInCache(const column_t __far* patch, byte* cache, int16_t originy, int16_t cacheheight)
{
    while (patch->topdelta != 0xff)
    {
        const byte __far* source = (const byte __far*)patch + 3;
        int16_t count = patch->length;
        int16_t position = originy + patch->topdelta;

        if (position < 0)
        {
            count += position;
            position = 0;
        }

        if (position + count > cacheheight)
            count = cacheheight - position;

        if (count > 0)
            _fmemcpy(cache + position, source, count);

        patch = (const column_t __far*)((const byte __far*)patch + patch->length + 4);
    }
}

/*
 * Draw a column of pixels of the specified texture.
 * If the texture is simple (1 patch, full height) then just draw
 * straight from const patch_t*.
*/

#define MAX_CACHE_ENTRIES 4
#define MAX_CACHE_TRIES 4

static uint16_t CACHE_ENTRY(int16_t column, int16_t texture)
{
	return column | (texture << 8);
}

static byte __far columnCache[MAX_CACHE_ENTRIES*128];
static uint16_t columnCacheEntries[MAX_CACHE_ENTRIES];

static uint16_t FindColumnCacheItem(int16_t texture, int16_t column)
{
	uint16_t hash = ((column >> 2) ^ (texture * 71)) & (MAX_CACHE_ENTRIES - 1);
	uint16_t key = hash;

	uint16_t cx = CACHE_ENTRY(column, texture);

	for (int16_t i = 0; i < MAX_CACHE_TRIES; i++)
	{
		if (columnCacheEntries[key] == 0 || columnCacheEntries[key] == cx)
			return key;

		key += 119;
		key &= (MAX_CACHE_ENTRIES - 1);
	}

	return hash;
}


static const byte __far* R_ComposeColumn(const int16_t texture, const texture_t __far* tex, int16_t texcolumn)
{
#if defined HIGH_DETAIL
    const int16_t xc = texcolumn & tex->widthmask;
#else
    const int16_t xc = (texcolumn & 0xfffc) & tex->widthmask;
#endif

    uint16_t cachekey = FindColumnCacheItem(texture, xc);

    byte __far* colcache = &columnCache[cachekey*128];
    uint16_t cacheEntry = columnCacheEntries[cachekey];

    //total++;

    if (cacheEntry != CACHE_ENTRY(xc, texture))
    {
        //misses++;
        static byte tmpCache[128];

        uint8_t i = 0;
        uint8_t patchcount = tex->patchcount;

        do
        {
            const texpatch_t __far* patch = &tex->patches[i];

            const int16_t x1 = patch->originx;

            if (xc < x1)
                continue;

            const patch_t __far* realpatch = W_GetLumpByNum(patch->patch_num);

            const int16_t x2 = x1 + realpatch->width;

            if (xc < x2)
            {
                const column_t __far* patchcol = (const column_t __far*)((const byte __far*)realpatch + realpatch->columnofs[xc - x1]);

                R_DrawColumnInCache (patchcol, tmpCache, patch->originy, tex->height);
            }
        } while(++i < patchcount);

        //Block copy will drop low 2 bits of len.
        _fmemcpy(colcache, tmpCache, (tex->height + 3) & ~3);

        columnCacheEntries[cachekey] = CACHE_ENTRY(xc, texture);
    }

    return colcache;
}

static void R_DrawSegTextureColumn(const texture_t __far* tex, int16_t texture, int16_t texcolumn, draw_column_vars_t* dcvars)
{
    if (!tex->overlapped)
    {
        int16_t patch_num;
        int16_t x_c;
        R_GetColumn(tex, texcolumn, &patch_num, &x_c);

        const patch_t __far* patch = W_GetLumpByNum(patch_num);

        const column_t __far* column = (const column_t __far*) ((const byte __far*)patch + patch->columnofs[x_c]);

        dcvars->source = (const byte __far*)column + 3;
        R_DrawColumnWall(dcvars);
    }
    else
    {
        dcvars->source = R_ComposeColumn(texture, tex, texcolumn);
        R_DrawColumnWall(dcvars);
    }
}
#endif

//
// R_RenderSegLoop
// Draws zero, one, or two textures (and possibly a masked texture) for walls.
// Can draw or mark the starting pixel of floor and ceiling textures.
// boolean segtextured is true if any of the segs textures might be visible.
// boolean markfloor is false if the back side is the same plane.
// CALLED: CORE LOOPING ROUTINE.
//

static void R_RenderSegLoop(int16_t rw_x, boolean segtextured, boolean markfloor, boolean markceiling)
{
    draw_column_vars_t dcvars;
    int16_t  texturecolumn = 0;   // shut up compiler warning

    dcvars.colormap = R_LoadColorMap(rw_lightlevel);

    for ( ; rw_x < rw_stopx ; rw_x++)
    {
        // mark floor / ceiling areas

        int16_t yh = bottomfrac>>FRACBITS;
        int16_t yl = (topfrac+FRACUNIT-1)>>FRACBITS;

        int16_t cc_rwx = ceilingclip[rw_x];
        int16_t fc_rwx = floorclip[rw_x];

        // no space above wall?
        int16_t bottom,top = cc_rwx+1;

        dcvars.x  = rw_x;

        if (yl < top)
            yl = top;

        if (markceiling)
        {
            bottom = yl-1;

            if (bottom >= fc_rwx)
                bottom = fc_rwx-1;

            if (top <= bottom)
            {
#if defined FLAT_SPAN
                dcvars.yl = top;
                dcvars.yh = bottom;
                if (ceilingplane_color == -2)
                    R_DrawSky(&dcvars);
                else
                    R_DrawColumnFlat(ceilingplane_color, &dcvars);
#else
                ceilingplane->top[rw_x] = top;
                ceilingplane->bottom[rw_x] = bottom;
                ceilingplane->modified = true;
#endif
            }
            // SoM: this should be set here
            cc_rwx = bottom;
        }

        bottom = fc_rwx-1;
        if (yh > bottom)
            yh = bottom;

        if (markfloor)
        {

            top  = yh < cc_rwx ? cc_rwx : yh;

            if (++top <= bottom)
            {
#if defined FLAT_SPAN
                dcvars.yl = top;
                dcvars.yh = bottom;
                R_DrawColumnFlat(floorplane_color, &dcvars);
#else
                floorplane->top[rw_x] = top;
                floorplane->bottom[rw_x] = bottom;
                floorplane->modified = true;
#endif
            }
            // SoM: This should be set here to prevent overdraw
            fc_rwx = top;
        }

        // texturecolumn and lighting are independent of wall tiers
        if (segtextured)
        {
            // calculate texture offset
#if !defined FLAT_WALL
			texturecolumn = rw_offset;
			int16_t ang = (angle16_t)(rw_centerangle + xtoviewangleTable[rw_x]) >> ANGLETOFINESHIFT_16;
			if (ang < 1024) {			//    0 <= ang < 1024
				fixed_t tan = finetangentTable_part_4[1023 - ang];
				texturecolumn += (rw_distance * tan) >> FRACBITS;
			} else if (ang < 2048) {	// 1024 <= ang < 2048
				fixed_t tan = finetangentTable_part_3[1023 - (ang - 1024)];
				texturecolumn += (rw_distance * tan) >> FRACBITS;
			} else if (ang < 3072) {	// 2048 <= ang < 3072
				fixed_t tan = finetangentTable_part_3[ang - 2048];
				texturecolumn -= (rw_distance * tan) >> FRACBITS;
			} else {					// 3072 <= ang < 4096
				fixed_t tan = finetangentTable_part_4[ang - 3072];
				texturecolumn -= (rw_distance * tan) >> FRACBITS;
			}
#endif

            dcvars.fracstep = FixedReciprocal((uint32_t)rw_scale) >> COLEXTRABITS;
        }

        // draw the wall tiers
        if (midtexture)
        {

            dcvars.yl = yl;     // single sided line
            dcvars.yh = yh;
            dcvars.texturemid = rw_midtexturemid;
            //

            R_DrawSegTextureColumn(texmidtexture, midtexture, texturecolumn, &dcvars);

            cc_rwx = VIEWWINDOWHEIGHT;
            fc_rwx = -1;
        }
        else
        {

            // two sided line
            if (toptexture)
            {
                // top wall
                int16_t mid = pixhigh>>FRACBITS;
                pixhigh += pixhighstep;

                if (mid >= fc_rwx)
                    mid = fc_rwx-1;

                if (mid >= yl)
                {
                    dcvars.yl = yl;
                    dcvars.yh = mid;
                    dcvars.texturemid = rw_toptexturemid;

                    R_DrawSegTextureColumn(textoptexture, toptexture, texturecolumn, &dcvars);

                    cc_rwx = mid;
                }
                else
                    cc_rwx = yl-1;
            }
            else  // no top wall
            {

                if (markceiling)
                    cc_rwx = yl-1;
            }

            if (bottomtexture)          // bottom wall
            {
                int16_t mid = (pixlow+FRACUNIT-1)>>FRACBITS;
                pixlow += pixlowstep;

                // no space above wall?
                if (mid <= cc_rwx)
                    mid = cc_rwx+1;

                if (mid <= yh)
                {
                    dcvars.yl = mid;
                    dcvars.yh = yh;
                    dcvars.texturemid = rw_bottomtexturemid;

                    R_DrawSegTextureColumn(texbottomtexture, bottomtexture, texturecolumn, &dcvars);

                    fc_rwx = mid;
                }
                else
                    fc_rwx = yh+1;
            }
            else        // no bottom wall
            {
                if (markfloor)
                    fc_rwx = yh+1;
            }

            // cph - if we completely blocked further sight through this column,
            // add this info to the solid columns array for r_bsp.c
            if ((markceiling || markfloor) && (fc_rwx <= cc_rwx + 1))
            {
                solidcol[rw_x] = 1;
                didsolidcol = true;
            }

            // save texturecol for backdrawing of masked mid texture
            if (maskedtexture)
                maskedtexturecol[rw_x] = texturecolumn;
        }

        rw_scale += rw_scalestep;
        topfrac += topstep;
        bottomfrac += bottomstep;

        floorclip[rw_x] = fc_rwx;
        ceilingclip[rw_x] = cc_rwx;
    }
}

static boolean R_CheckOpenings(const int16_t start)
{
    int16_t pos = lastopening - openings;
    int16_t need = (rw_stopx - start)*sizeof(int16_t) + pos;

#ifdef RANGECHECK
    if(need > MAXOPENINGS)
        I_Error("Openings overflow. Need = %ld", need);
#endif

    return need <= MAXOPENINGS;
}


static void R_ClearOpeningClippingDetermination(void)
{
	// opening / clipping determination
	for (uint8_t i = 0; i < VIEWWINDOWWIDTH; i++)
		floorclip[i] = VIEWWINDOWHEIGHT, ceilingclip[i] = -1;
}


static void R_ClearOpenings(void)
{
	lastopening = openings;
}


/* CPhipps -
 * Mod - returns a % b, guaranteeing 0 <= a < b
 * (notice that the C standard for % does not guarantee this)
 */

inline static int16_t CONSTFUNC Mod(uint8_t a, int16_t b)
{
    if(!a)
        return 0;

    if (b & (b-1))
    {
        int16_t r = a % b;
        return ((r<0) ? r+b : r);
    }
    else
        return (a & (b-1));
}


//
// R_StoreWallRange
// A wall segment will be drawn
//  between start and stop pixels (inclusive).
//
static void R_StoreWallRange(const int16_t start, const int16_t stop)
{
    // don't overflow and crash
    if (ds_p == &_s_drawsegs[MAXDRAWSEGS])
    {
#ifdef RANGECHECK
        I_Error("Drawsegs overflow.");
#endif
        return;
    }


    side_t    __far* sidedef = &_g_sides[curline->sidenum];
    const mapsidedef_t *mapsidedef = &_g_mapsides[curline->sidenum];	
    linedef = &_g_lines[curline->linenum];
    maplinedef = &_g_maplines[curline->linenum];

    // mark the segment as visible for auto map
    linedef->r_flags |= ML_MAPPED;

    // calculate rw_distance for scale calculation
    rw_normalangle = curline->angle;

    angle16_t offsetangle = rw_normalangle - rw_angle1;

#if defined _M_I86
    if (abs(offsetangle) > ANG90_16)
        offsetangle = ANG90_16;
#else
    if (D_abs((angle_t)offsetangle << FRACBITS) > ANG90)
        offsetangle = ANG90_16;
#endif

    int16_t hyp = R_PointToDist(curline->v1.x, curline->v1.y);

    rw_distance = (hyp * finecosineapprox(offsetangle >> ANGLETOFINESHIFT_16)) >> FRACBITS;

    int16_t rw_x = ds_p->x1 = start;
    ds_p->x2 = stop;
    ds_p->curline = curline;
    rw_stopx = stop+1;

    //Openings overflow. Nevermind.
    if(!R_CheckOpenings(start))
        return;

    // calculate scale at both ends and step
    ds_p->scale1 = rw_scale = R_ScaleFromGlobalAngle(start);

    if (stop > start)
    {
        ds_p->scale2 = R_ScaleFromGlobalAngle(stop);
        ds_p->scalestep = rw_scalestep = (ds_p->scale2 - rw_scale) / (stop - start);
    }
    else
        ds_p->scale2 = ds_p->scale1;

    // calculate texture boundaries
    //  and decide if floor / ceiling marks are needed

    worldtop = frontsector->ceilingheight - viewz;
    worldbottom = frontsector->floorheight - viewz;

    midtexture = toptexture = bottomtexture = maskedtexture = 0;
    ds_p->maskedtexturecol = NULL;

    boolean markfloor, markceiling;

    if (!backsector)
    {
        // single sided line
        midtexture = R_GetTextureTranslation(sidedef->midtexture);
        texmidtexture = R_GetTexture(midtexture);

        // a single sided line is terminal, so it must mark ends
        markfloor = markceiling = true;

        if (maplinedef->flags & ML_DONTPEGBOTTOM)
        {         // bottom of texture at bottom
            fixed_t vtop = frontsector->floorheight + ((int32_t)textureheight[sidedef->midtexture] << FRACBITS);
            rw_midtexturemid = vtop - viewz;
        }
        else        // top of texture at top
            rw_midtexturemid = worldtop;

        rw_midtexturemid += ((int32_t)Mod(mapsidedef->rowoffset, textureheight[midtexture])) << FRACBITS;

        ds_p->silhouette = SIL_BOTH;
        ds_p->sprtopclip = screenheightarray;
        ds_p->sprbottomclip = negonearray;
        ds_p->bsilheight = INT32_MAX;
        ds_p->tsilheight = INT32_MIN;
    }
    else      // two sided line
    {
        ds_p->sprtopclip = ds_p->sprbottomclip = NULL;
        ds_p->silhouette = SIL_NONE;

        if(linedef->r_flags & RF_CLOSED)
        { /* cph - closed 2S line e.g. door */
            // cph - killough's (outdated) comment follows - this deals with both
            // "automap fixes", his and mine
            // killough 1/17/98: this test is required if the fix
            // for the automap bug (r_bsp.c) is used, or else some
            // sprites will be displayed behind closed doors. That
            // fix prevents lines behind closed doors with dropoffs
            // from being displayed on the automap.

            ds_p->silhouette = SIL_BOTH;
            ds_p->sprtopclip = screenheightarray;
            ds_p->sprbottomclip = negonearray;
            ds_p->bsilheight = INT32_MAX;
            ds_p->tsilheight = INT32_MIN;

        }
        else
        { /* not solid - old code */

            if (frontsector->floorheight > backsector->floorheight)
            {
                ds_p->silhouette = SIL_BOTTOM;
                ds_p->bsilheight = frontsector->floorheight;
            }
            else
                if (backsector->floorheight > viewz)
                {
                    ds_p->silhouette = SIL_BOTTOM;
                    ds_p->bsilheight = INT32_MAX;
                }

            if (frontsector->ceilingheight < backsector->ceilingheight)
            {
                ds_p->silhouette |= SIL_TOP;
                ds_p->tsilheight = frontsector->ceilingheight;
            }
            else
                if (backsector->ceilingheight < viewz)
                {
                    ds_p->silhouette |= SIL_TOP;
                    ds_p->tsilheight = INT32_MIN;
                }
        }

        worldhigh = backsector->ceilingheight - viewz;
        worldlow = backsector->floorheight - viewz;

        // hack to allow height changes in outdoor areas
        if (frontsector->ceilingpic == skyflatnum && backsector->ceilingpic == skyflatnum)
            worldtop = worldhigh;

        markfloor = worldlow != worldbottom
                || backsector->floorpic != frontsector->floorpic
                || backsector->lightlevel != frontsector->lightlevel
                ;

        markceiling = worldhigh != worldtop
                || backsector->ceilingpic != frontsector->ceilingpic
                || backsector->lightlevel != frontsector->lightlevel
                ;

        if (backsector->ceilingheight <= frontsector->floorheight || backsector->floorheight >= frontsector->ceilingheight)
            markceiling = markfloor = true;   // closed door

        if (worldhigh < worldtop)   // top texture
        {
            toptexture = R_GetTextureTranslation(sidedef->toptexture);
            textoptexture = R_GetTexture(toptexture);
            rw_toptexturemid = maplinedef->flags & ML_DONTPEGTOP ? worldtop :
                                                                        backsector->ceilingheight + ((int32_t)textureheight[sidedef->toptexture] << FRACBITS) - viewz;
            rw_toptexturemid += ((int32_t)Mod(mapsidedef->rowoffset, textureheight[toptexture])) << FRACBITS;
        }

        if (worldlow > worldbottom) // bottom texture
        {
            bottomtexture = R_GetTextureTranslation(sidedef->bottomtexture);
            texbottomtexture = R_GetTexture(bottomtexture);
            rw_bottomtexturemid = maplinedef->flags & ML_DONTPEGBOTTOM ? worldtop : worldlow;

            rw_bottomtexturemid += ((int32_t)Mod(mapsidedef->rowoffset, textureheight[bottomtexture])) << FRACBITS;
        }

        // allocate space for masked texture tables
        if (sidedef->midtexture)    // masked midtexture
        {
            maskedtexture = true;
            ds_p->maskedtexturecol = maskedtexturecol = lastopening - rw_x;
            lastopening += rw_stopx - rw_x;
        }
    }

    // calculate rw_offset (only needed for textured lines)
    boolean segtextured = ((midtexture | toptexture | bottomtexture | maskedtexture) > 0);

    if (segtextured)
    {
        fixed_t rw_offset32 = hyp * -finesineapprox(offsetangle >> ANGLETOFINESHIFT_16);
        rw_offset = rw_offset32 >> FRACBITS;
        rw_offset += sidedef->textureoffset + curline->offset;

        rw_centerangle = ANG90_16 + viewangle16 - rw_normalangle;

        rw_lightlevel = frontsector->lightlevel;
    }

    // if a floor / ceiling plane is on the wrong side of the view
    // plane, it is definitely invisible and doesn't need to be marked.
    if (frontsector->floorheight >= viewz)       // above view plane
        markfloor = false;
    if (frontsector->ceilingheight <= viewz &&
            frontsector->ceilingpic != skyflatnum)   // below view plane
        markceiling = false;

    // calculate incremental stepping values for texture edges
    topstep = -FixedMul(worldtop, rw_scalestep);
    topfrac = (CENTERY * FRACUNIT) - FixedMul(worldtop, rw_scale);

    bottomstep = -FixedMul(worldbottom, rw_scalestep);
    bottomfrac = (CENTERY * FRACUNIT) - FixedMul(worldbottom, rw_scale);

    if (backsector)
    {
        if (worldhigh < worldtop)
        {
            pixhigh = (CENTERY * FRACUNIT) - FixedMul(worldhigh, rw_scale);
            pixhighstep = -FixedMul(worldhigh, rw_scalestep);
        }
        if (worldlow > worldbottom)
        {
            pixlow = (CENTERY * FRACUNIT) - FixedMul(worldlow, rw_scale);
            pixlowstep = -FixedMul(worldlow, rw_scalestep);
        }
    }

    // render it
    if (markceiling)
    {
#if defined FLAT_SPAN
        if (ceilingplane_color == -1)
            markceiling = false;
#else
        if (ceilingplane)   // killough 4/11/98: add NULL ptr checks
            ceilingplane = R_CheckPlane (ceilingplane, rw_x, rw_stopx-1);
        else
            markceiling = false;
#endif
    }

    if (markfloor)
    {
#if defined FLAT_SPAN
        if (floorplane_color == -1)
            markfloor = false;
#else
        if (floorplane)     // killough 4/11/98: add NULL ptr checks
            /* cph 2003/04/18  - ceilingplane and floorplane might be the same
       * visplane (e.g. if both skies); R_CheckPlane doesn't know about
       * modifications to the plane that might happen in parallel with the check
       * being made, so we have to override it and split them anyway if that is
       * a possibility, otherwise the floor marking would overwrite the ceiling
       * marking, resulting in HOM. */
            if (markceiling && ceilingplane == floorplane)
                floorplane = R_DupPlane (floorplane, rw_x, rw_stopx-1);
            else
                floorplane = R_CheckPlane (floorplane, rw_x, rw_stopx-1);
        else
            markfloor = false;
#endif
    }

    didsolidcol = false;
    R_RenderSegLoop(rw_x, segtextured, markfloor, markceiling);

    /* cph - if a column was made solid by this wall, we _must_ save full clipping info */
    if (backsector && didsolidcol)
    {
        if (!(ds_p->silhouette & SIL_BOTTOM))
        {
            ds_p->silhouette |= SIL_BOTTOM;
            ds_p->bsilheight = backsector->floorheight;
        }
        if (!(ds_p->silhouette & SIL_TOP))
        {
            ds_p->silhouette |= SIL_TOP;
            ds_p->tsilheight = backsector->ceilingheight;
        }
    }

    // save sprite clipping info
    if ((ds_p->silhouette & SIL_TOP || maskedtexture) && !ds_p->sprtopclip)
    {
        memcpy((byte*)lastopening, (const byte*)(ceilingclip+start), sizeof(int16_t)*(rw_stopx-start));
        ds_p->sprtopclip = lastopening - start;
        lastopening += rw_stopx - start;
    }

    if ((ds_p->silhouette & SIL_BOTTOM || maskedtexture) && !ds_p->sprbottomclip)
    {
        memcpy((byte*)lastopening, (const byte*)(floorclip+start), sizeof(int16_t)*(rw_stopx-start));
        ds_p->sprbottomclip = lastopening - start;
        lastopening += rw_stopx - start;
    }

    if (maskedtexture && !(ds_p->silhouette & SIL_TOP))
    {
        ds_p->silhouette |= SIL_TOP;
        ds_p->tsilheight = INT32_MIN;
    }

    if (maskedtexture && !(ds_p->silhouette & SIL_BOTTOM))
    {
        ds_p->silhouette |= SIL_BOTTOM;
        ds_p->bsilheight = INT32_MAX;
    }

    ds_p++;
}


// killough 1/18/98 -- This function is used to fix the automap bug which
// showed lines behind closed doors simply because the door had a dropoff.
//
// cph - converted to R_RecalcLineFlags. This recalculates all the flags for
// a line, including closure and texture tiling.

static void R_RecalcLineFlags(void)
{
    const side_t __far* side = &_g_sides[curline->sidenum];

    linedef->r_validcount = _g_gametic;

    /* First decide if the line is closed, normal, or invisible */
    if (!(maplinedef->flags & ML_TWOSIDED)
            || backsector->ceilingheight <= frontsector->floorheight
            || backsector->floorheight >= frontsector->ceilingheight
            || (
                // if door is closed because back is shut:
                backsector->ceilingheight <= backsector->floorheight

                // preserve a kind of transparent door/lift special effect:
                && (backsector->ceilingheight >= frontsector->ceilingheight ||
                    side->toptexture)

                && (backsector->floorheight <= frontsector->floorheight ||
                    side->bottomtexture)

                // properly render skies (consider door "open" if both ceilings are sky):
                && (backsector->ceilingpic != skyflatnum ||
                    frontsector->ceilingpic!= skyflatnum)
                )
            )
        linedef->r_flags = (RF_CLOSED | (linedef->r_flags & ML_MAPPED));
    else
    {
        // Reject empty lines used for triggers
        //  and special events.
        // Identical floor and ceiling on both sides,
        // identical light levels on both sides,
        // and no middle texture.
        // CPhipps - recode for speed, not certain if this is portable though
        if (backsector->ceilingheight != frontsector->ceilingheight
                || backsector->floorheight != frontsector->floorheight
                || side->midtexture
                || backsector->ceilingpic != frontsector->ceilingpic
                || backsector->floorpic != frontsector->floorpic
                || backsector->lightlevel != frontsector->lightlevel)
        {
            linedef->r_flags = (linedef->r_flags & ML_MAPPED);
        } else
            linedef->r_flags = (RF_IGNORE | (linedef->r_flags & ML_MAPPED));
    }
}


// CPhipps -
// R_ClipWallSegment
//
// Replaces the old R_Clip*WallSegment functions. It draws bits of walls in those
// columns which aren't solid, and updates the solidcol[] array appropriately

static void R_ClipWallSegment(int16_t first, int16_t last, const boolean solid)
{
    byte *p;
    while (first < last)
    {
        if (solidcol[first])
        {
            if (!(p = memchr(solidcol+first, 0, last-first)))
                return; // All solid

            first = p - solidcol;
        }
        else
        {
            int16_t to;
            if (!(p = memchr(solidcol+first, 1, last-first)))
                to = last;
            else
                to = p - solidcol;

            R_StoreWallRange(first, to-1);

            if (solid)
            {
                memset(solidcol + first, 1, to - first);
            }

            first = to;
        }
    }
}

//
// R_ClearClipSegs
//

//
// R_AddLine
// Clips the given segment
// and adds any visible pieces to the line list.
//

static void R_AddLine(const seg_t __far* line)
{
    curline = line;

    angle16_t angle1 = R_PointToAngle16(line->v1.x, line->v1.y);
    angle16_t angle2 = R_PointToAngle16(line->v2.x, line->v2.y);

    // Clip to view edges.
    angle16_t span = angle1 - angle2;

    // Back side, i.e. backface culling
    if (span >= ANG180_16)
        return;

    // Global angle needed by segcalc.
    rw_angle1 = angle1;
    angle1 -= viewangle16;
    angle2 -= viewangle16;

    angle16_t tspan = angle1 + clipangle;
    if (tspan > 2 * clipangle)
    {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;

        angle1 = clipangle;
    }

    tspan = clipangle - angle2;
    if (tspan > 2 * clipangle)
    {
        tspan -= 2 * clipangle;

        // Totally off the left edge?
        if (tspan >= span)
            return;
        angle2 = -clipangle;
    }

    // The seg is in the view range,
    // but not necessarily visible.

    // killough 1/31/98: Here is where "slime trails" can SOMETIMES occur:
    uint8_t x1 = viewangletox((angle16_t)(angle1 + ANG90_16) >> ANGLETOFINESHIFT_16);
    uint8_t x2 = viewangletox((angle16_t)(angle2 + ANG90_16) >> ANGLETOFINESHIFT_16);

    // Does not cross a pixel?
    if (x1 >= x2)       // killough 1/31/98 -- change == to >= for robustness
        return;

    backsector = line->backsectornum != NO_INDEX8 ? &_g_sectors[line->backsectornum] : NULL;

    /* cph - roll up linedef properties in flags */
    linedef = &_g_lines[curline->linenum];
    maplinedef = &_g_maplines[curline->linenum];

    if (linedef->r_validcount != (uint16_t)_g_gametic)
        R_RecalcLineFlags();

    if (!(linedef->r_flags & RF_IGNORE))
    {
        R_ClipWallSegment (x1, x2, linedef->r_flags & RF_CLOSED);
    }
}

//
// R_Subsector
// Determine floor/ceiling planes.
// Add sprites of things in sector.
// Draw one or more line segments.
//

static void R_Subsector(int16_t num)
{
    int16_t         count;
    const seg_t       __far* line;
    subsector_t __far* sub;

    sub = &_g_subsectors[num];
    frontsector = sub->sector;
    count = _g_mapsubsectors[num].numsegs;
    line = &_g_segs[_g_mapsubsectors[num].firstseg];

#if defined FLAT_SPAN
    if (frontsector->floorheight < viewz)
        floorplane_color = R_GetPlaneColor(frontsector->floorpic, frontsector->lightlevel);
    else
        floorplane_color = -1;
#else
    if(frontsector->floorheight < viewz)
    {
        floorplane = R_FindPlane(frontsector->floorheight,
                                     frontsector->floorpic,
                                     frontsector->lightlevel                // killough 3/16/98
                                     );
    }
    else
    {
        floorplane = NULL;
    }
#endif


#if defined FLAT_SPAN
    if (frontsector->ceilingpic == skyflatnum) {
        ceilingplane_color = -2;
        R_LoadSkyPatch();
    } else if (frontsector->ceilingheight > viewz)
        ceilingplane_color = R_GetPlaneColor(frontsector->ceilingpic, frontsector->lightlevel);
    else
        ceilingplane_color = -1;
#else
    if(frontsector->ceilingheight > viewz || (frontsector->ceilingpic == skyflatnum))
    {
        ceilingplane = R_FindPlane(frontsector->ceilingheight,     // killough 3/8/98
                                       frontsector->ceilingpic,
                                       frontsector->lightlevel
                                       );
    }
    else
    {
        ceilingplane = NULL;
    }
#endif

    R_AddSprites(sub, frontsector->lightlevel);
    while (count--)
    {
        R_AddLine (line);
        line++;
        curline = NULL; /* cph 2001/11/18 - must clear curline now we're done with it, so R_LoadColorMap doesn't try using it for other things */
    }
}

//
// R_CheckBBox
// Checks BSP node/subtree bounding box.
// Returns true
//  if some part of the bbox might be visible.
//

static const byte checkcoord[12][4] =
{
  {3,0,2,1},
  {3,0,2,0},
  {3,1,2,0},
  {0},
  {2,0,2,1},
  {0,0,0,0},
  {3,1,3,0},
  {0},
  {2,0,3,1},
  {2,1,3,1},
  {2,1,3,0}
};


static boolean R_CheckBBox(const int16_t __far* bspcoord)
{
    // Find the corners of the box
    // that define the edges from current viewpoint.
    int16_t boxpos = (viewx <= ((fixed_t)bspcoord[BOXLEFT]<<FRACBITS) ? 0 : viewx < ((fixed_t)bspcoord[BOXRIGHT]<<FRACBITS) ? 1 : 2) +
            (viewy >= ((fixed_t)bspcoord[BOXTOP]<<FRACBITS) ? 0 : viewy > ((fixed_t)bspcoord[BOXBOTTOM]<<FRACBITS) ? 4 : 8);

    if (boxpos == 5)
        return true;

    const byte* check = checkcoord[boxpos];
    angle16_t angle1 = R_PointToAngle16(bspcoord[check[0]], bspcoord[check[1]]) - viewangle16;
    angle16_t angle2 = R_PointToAngle16(bspcoord[check[2]], bspcoord[check[3]]) - viewangle16;


    // cph - replaced old code, which was unclear and badly commented
    // Much more efficient code now
    if ((int16_t)angle1 < (int16_t)angle2)
    { /* it's "behind" us */
        /* Either angle1 or angle2 is behind us, so it doesn't matter if we
     * change it to the corect sign
     */
        if (ANG180_16 <= angle1 && angle1 < ANG270_16)
            angle1 = INT16_MAX; /* which is ANG180_16 - 1 */
        else
            angle2 = INT16_MIN;
    }

    if ((int16_t)angle2 >=  (int16_t)clipangle) return false; // Both off left edge
    if ((int16_t)angle1 <= -(int16_t)clipangle) return false; // Both off right edge
    if ((int16_t)angle1 >=  (int16_t)clipangle) angle1 =  clipangle; // Clip at left edge
    if ((int16_t)angle2 <= -(int16_t)clipangle) angle2 = -clipangle; // Clip at right edge

    // Find the first clippost
    //  that touches the source post
    //  (adjacent pixels are touching).

    uint8_t sx1 = viewangletox((angle16_t)(angle1 + ANG90_16) >> ANGLETOFINESHIFT_16);
    uint8_t sx2 = viewangletox((angle16_t)(angle2 + ANG90_16) >> ANGLETOFINESHIFT_16);
    //    const cliprange_t *start;

    // Does not cross a pixel.
    if (sx1 == sx2)
        return false;

    if (!memchr(solidcol+sx1, 0, sx2-sx1)) return false;
    // All columns it covers are already solidly covered


    return true;
}

//Render a BSP subsector if bspnum is a leaf node.
//Return false if bspnum is frame node.





static boolean R_RenderBspSubsector(int16_t bspnum)
{
    // Found a subsector?
    if (bspnum & NF_SUBSECTOR)
    {
        if (bspnum == -1)
            R_Subsector (0);
        else
            R_Subsector (bspnum & (~NF_SUBSECTOR));

        return true;
    }

    return false;
}

// RenderBSPNode
// Renders all subsectors below a given node,
//  traversing subtree recursively.
// Just call with BSP root.

#if defined PROFILING
//Non recursive version.
//constant stack space used and easier to
//performance profile.
#define MAX_BSP_DEPTH 64

static void R_RenderBSPNode(int16_t bspnum)
{
    static int16_t stack_bsp[MAX_BSP_DEPTH];
    static int8_t stack_side[MAX_BSP_DEPTH];
    int16_t sp = 0;

    const mapnode_t __far* bsp;
    int8_t side;

    while (true)
    {
        //Front sides.
        while (!R_RenderBspSubsector(bspnum))
        {
            if (sp == MAX_BSP_DEPTH)
                break;

            bsp = &nodes[bspnum];
            side = R_PointOnSide(viewx, viewy, bsp);

            stack_bsp[sp]  = bspnum;
            stack_side[sp] = side ^ 1;
            sp++;

            bspnum = bsp->children[side];
        }

        if (sp == 0)
        {
            //back at root node and not visible. All done!
            return;
        }

        //Back sides.
        --sp;
        side   = stack_side[sp];
        bspnum = stack_bsp[sp];
        bsp    = &nodes[bspnum];

        // Possibly divide back space.
        //Walk back up the tree until we find
        //a node that has a visible backspace.
        while (!R_CheckBBox(bsp->bbox[side]))
        {
            if (sp == 0)
            {
                //back at root node and not visible. All done!
                return;
            }

            //Back side next.
            --sp;
            side   = stack_side[sp];
            bspnum = stack_bsp[sp];

            bsp = &nodes[bspnum];
        }

        bspnum = bsp->children[side];
    }
}
#else
static void R_RenderBSPNode(int16_t bspnum)
{
	if (R_RenderBspSubsector(bspnum))
		return;

	const mapnode_t __far* bsp = &nodes[bspnum];

//
// decide which side the view point is on
//
	int16_t side = R_PointOnSide(viewx, viewy, bsp);

	R_RenderBSPNode(bsp->children[side]); // recursively divide front space

	if (R_CheckBBox(bsp->bbox[side ^ 1]))	// possibly divide back space
		R_RenderBSPNode(bsp->children[side ^ 1]);
}
#endif


static void R_ClearDrawSegs(void)
{
    ds_p = _s_drawsegs;
}

static void R_ClearClipSegs (void)
{
    memset(solidcol, 0, VIEWWINDOWWIDTH);
}


//
// R_SetupFrame
//

static void R_SetupFrame (player_t *player)
{
    viewx = player->mo->x;
    viewy = player->mo->y;
    viewz = player->viewz;
    viewangle = player->mo->angle;
    viewangle16 = viewangle >> FRACBITS;

    extralight = player->extralight;

    viewsin = finesineapprox(  viewangle16 >> ANGLETOFINESHIFT_16);
    viewcos = finecosineapprox(viewangle16 >> ANGLETOFINESHIFT_16);

    if (player->fixedcolormap)
    {
        fixedcolormap = fullcolormap;
    }
    else
        fixedcolormap = NULL;

    validcount++;
}


//
// R_RenderView
//
void R_RenderPlayerView (player_t* player)
{
    R_SetupFrame (player);

    // Clear buffers.
    R_ClearClipSegs ();
    R_ClearDrawSegs ();
    R_ClearOpeningClippingDetermination ();

#if !defined FLAT_SPAN
    R_ClearPlanes ();
#endif

    R_ClearOpenings ();
    R_ClearSprites ();

    // The head node is the last node output.
    R_RenderBSPNode (numnodes-1);

#if defined FLAT_SPAN
    R_FreeSkyPatch ();
#else
    R_DrawPlanes ();
#endif

    R_DrawMasked ();
}


void R_InitDrawTables(void)
{
    extern void *malloc(size_t size);
    static boolean s_draw_tables_init = false;
    if (s_draw_tables_init) return;
    s_draw_tables_init = true;

    R_InitTables(); // Ensure finesineTable_part_1 is populated

    if (!openings) openings = malloc(MAXOPENINGS * sizeof(*openings));
    if (!_s_drawsegs) _s_drawsegs = (drawseg_t*)malloc(MAXDRAWSEGS * sizeof(drawseg_t));
    if (!tantoangleTable) tantoangleTable = (angle_t*)malloc(2049 * sizeof(angle_t));

    if (!finetangentTable_part_3) finetangentTable_part_3 = (uint16_t*)malloc(1024 * sizeof(uint16_t));
    if (!finetangentTable_part_4) finetangentTable_part_4 = (fixed_t*)malloc(1024 * sizeof(fixed_t));

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    tantoangle16Table = ((angle16_t*)&tantoangleTable[0]);
#else
    tantoangle16Table = ((angle16_t*)&tantoangleTable[0]) + 1;
#endif

    if (!openings || !_s_drawsegs || !tantoangleTable || !finetangentTable_part_3 || !finetangentTable_part_4)
        I_Error("Render table allocation");

    // 1. CORDIC for tantoangleTable
    static const uint32_t cordic_lut[24] = {
        536870912, 316933406, 167458907, 85004756, 42667357, 21354475,
        10679807, 5340245, 2670163, 1335086, 667544, 333772,
        166886, 83443, 41722, 20861, 10430, 5215, 2608, 1304,
        652, 326, 163, 81
    };
    for (int t = 0; t <= 2048; t++) {
        int64_t x = 2048LL << 16;
        int64_t y = (int64_t)t << 16;
        uint32_t angle = 0;
        for (int k = 0; k < 24; k++) {
            int64_t nx, ny;
            if (y > 0) {
                nx = x + (y >> k);
                ny = y - (x >> k);
                angle += cordic_lut[k];
            } else {
                nx = x - (y >> k);
                ny = y + (x >> k);
                angle -= cordic_lut[k];
            }
            x = nx;
            y = ny;
        }
        tantoangleTable[t] = angle;
    }

    // 3. finetangentTable_part_3 & part_4 from finesineTable_part_1
    extern uint16_t *finesineTable_part_1;
    for (int i = 0; i < 1024; i++) {
        uint32_t s_val = finesineTable_part_1[i];
        uint32_t c_val = finesineTable_part_1[2047 - i];
        finetangentTable_part_3[i] = (uint16_t)((s_val << 16) / c_val);
    }
    for (int i = 0; i < 1024; i++) {
        int i_idx = 1024 + i;
        uint32_t s_val = finesineTable_part_1[i_idx];
        uint32_t c_val = finesineTable_part_1[2047 - i_idx];
        if (c_val == 0) {
            finetangentTable_part_4[i] = 170910304;
        } else {
            finetangentTable_part_4[i] = (fixed_t)(((uint64_t)s_val << 16) / c_val);
        }
    }
}
