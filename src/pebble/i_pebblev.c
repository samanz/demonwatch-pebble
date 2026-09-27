/*-----------------------------------------------------------------------------
 *  i_pebblev.c: Pebble Time 2 (Emery) direct framebuffer renderer.
 *  Draws Doom columns directly into PebbleOS 200x228 RGB222 display buffer.
 *-----------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>
#undef false
#undef true
#include "i_pebble.h"
#include "../doom/doomtype.h"
#include "../doom/r_defs.h"
#include "../doom/r_main.h"
#include "../doom/d_player.h"
#include "../doom/d_items.h"
extern player_t _g_player;

#define COLEXTRABITS (8 - 1)
#define COLBITS (8 + 1)
#define CENTERY (VIEWWINDOWHEIGHT / 2)

static uint8_t *s_fb = NULL;
static uint16_t s_fb_width = PEBBLE_SCREEN_WIDTH;
static uint16_t s_fb_height = PEBBLE_SCREEN_HEIGHT;

void I_SetPebbleFramebuffer(uint8_t *fb, uint16_t width, uint16_t height) {
    s_fb = width >= PEBBLE_SCREEN_WIDTH && height >= PEBBLE_SCREEN_HEIGHT ? fb : NULL;
    s_fb_width = width;
    s_fb_height = height;
}

uint8_t *I_GetPebbleFramebuffer(void) {
    return s_fb;
}

// Logical pixel (x, y) covers screen columns s_xb[x] .. s_xb[x+1]-1 and rows
// s_yb[y] .. s_yb[y+1]-1: 120 x 114 scaled to 200 x 190, so every cell is
// 1-2 pixels each way. The tables replace four divisions per pixel.
static uint8_t s_xb[VIEWWINDOWWIDTH + 1], s_yb[VIEWWINDOWHEIGHT + 1];

static void init_scale_tables(void) {
    for (int x = 0; x <= VIEWWINDOWWIDTH; x++) s_xb[x] = x * PEBBLE_SCREEN_WIDTH / VIEWWINDOWWIDTH;
    for (int y = 0; y <= VIEWWINDOWHEIGHT; y++) s_yb[y] = y * PEBBLE_VIEW_HEIGHT / VIEWWINDOWHEIGHT;
}

static void draw_column(const draw_column_vars_t *dc, int flat, uint8_t color, int fuzz) {
    if (!s_fb || dc->x < 0 || dc->x >= VIEWWINDOWWIDTH) return;
    if (!s_xb[VIEWWINDOWWIDTH]) init_scale_tables();
    int lo=dc->yl<0 ? 0 : dc->yl;
    int hi=dc->yh>=VIEWWINDOWHEIGHT ? VIEWWINDOWHEIGHT-1 : dc->yh;
    if (lo > hi) return;
    uint16_t frac=(dc->texturemid >> COLEXTRABITS)+(lo-CENTERY)*dc->fracstep;
    // Flat colours arrive already lit (R_GetPlaneColor); textures and sprites
    // go through the sector's 64-entry light table.
    const uint8_t *light=flat ? NULL : dc->colormap;
    const int stride=s_fb_width;
    const int wide=s_xb[dc->x+1]-s_xb[dc->x] > 1;
    uint8_t *dst=s_fb+s_yb[lo]*stride+s_xb[dc->x];
    uint8_t pixel=0xc0 | (color & 63);
    for(int y=lo;y<=hi;++y) {
        if(!flat) {
            uint8_t index=dc->source[frac >> COLBITS];
            if(light) index=light[index & 63];
            pixel=0xc0 | (index & 63);
            frac+=dc->fracstep;
        }
        for(int rows=s_yb[y+1]-s_yb[y]; rows>0; --rows, dst+=stride) {
            if(fuzz) {
                dst[0]=0xc0 | ((dst[0] >> 1)&0x15);
                if(wide) dst[1]=0xc0 | ((dst[1] >> 1)&0x15);
            } else {
                dst[0]=pixel;
                if(wide) dst[1]=pixel;
            }
        }
    }
}
void R_DrawColumnWall(const draw_column_vars_t *dc) { draw_column(dc,0,0,0); }
void R_DrawColumnFlat(uint8_t color,const draw_column_vars_t *dc) { draw_column(dc,1,color,0); }
void R_DrawColumnSprite(const draw_column_vars_t *dc) { draw_column(dc,0,0,0); }
void R_DrawFuzzColumn(const draw_column_vars_t *dc) { draw_column(dc,1,0,1); }

// 5x7 mini bitmap font for numbers on watch display
static const uint8_t s_font5x7[10][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 0
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // 9
};

static void draw_digit_scaled2x(uint8_t *fb, int x0, int y0, int digit, uint8_t color) {
    if (digit < 0 || digit > 9) return;
    const uint8_t *cols = s_font5x7[digit];
    for (int cx = 0; cx < 5; cx++) {
        uint8_t bits = cols[cx];
        for (int cy = 0; cy < 7; cy++) {
            if (bits & (1 << cy)) {
                int px = x0 + cx * 2;
                int py = y0 + cy * 2;
                if (px + 1 < PEBBLE_SCREEN_WIDTH && py + 1 < PEBBLE_SCREEN_HEIGHT) {
                    fb[py * s_fb_width + px] = color;
                    fb[py * s_fb_width + (px + 1)] = color;
                    fb[(py + 1) * s_fb_width + px] = color;
                    fb[(py + 1) * s_fb_width + (px + 1)] = color;
                }
            }
        }
    }
}

static void draw_number(uint8_t *fb, int x, int y, int num, uint8_t color) {
    if (num < 0) num = 0;
    if (num > 999) num = 999;
    char buf[8];
    int len = 0;
    if (num == 0) {
        buf[len++] = '0';
    } else {
        int temp = num;
        char rev[8];
        int rlen = 0;
        while (temp > 0) {
            rev[rlen++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int i = rlen - 1; i >= 0; i--) buf[len++] = rev[i];
    }
    buf[len] = '\0';

    int cur_x = x;
    for (int i = 0; i < len; i++) {
        draw_digit_scaled2x(fb, cur_x, y, buf[i] - '0', color);
        cur_x += 12; // 10px digit width + 2px spacing
    }
}

// Watch-optimized Status Bar (200x38 at bottom of screen)
void ST_PebbleDrawer(uint8_t *fb) {
    if (!fb) return;

    // Dark charcoal background (RGB222: 0xd5)
    uint8_t bg_color = 0xc0;
    uint8_t border_color = 0xea;
    for(int y=PEBBLE_HUD_Y_START;y<PEBBLE_SCREEN_HEIGHT;++y)
        memset(fb+y*s_fb_width,bg_color,PEBBLE_SCREEN_WIDTH);

    // Separator line
    memset(fb + PEBBLE_HUD_Y_START * s_fb_width, border_color, PEBBLE_SCREEN_WIDTH);

    // Large Bold Numbers: Health, Ammo, Armor
    draw_number(fb, 18, 210, _g_player.health, 0xf0); // Red: HP
    int ammo=weaponinfo[_g_player.readyweapon].ammo;
    draw_number(fb, 85, 210, ammo < NUMAMMO ? _g_player.ammo[ammo] : 0, 0xfc);  // Yellow: Ammo
    draw_number(fb, 150, 210, _g_player.armorpoints, 0xcc);   // Green: Armor

    // Keycards: blue, yellow, red squares stacked at the right edge.
    static const uint8_t key_colors[NUMCARDS] = {0xc3, 0xfc, 0xf0};
    for (int k = 0; k < NUMCARDS; k++) {
        if (!_g_player.cards[k]) continue;
        for (int y = 0; y < 7; y++)
            memset(fb + (196 + k * 10 + y) * s_fb_width + 190, key_colors[k], 7);
    }
}
