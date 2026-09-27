#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/pebble/i_pebble.h"
#include "../src/doom/r_defs.h"
#include "../src/doom/r_main.h"
#include "../src/doom/d_player.h"
#include "../src/doom/d_items.h"
player_t _g_player;
const weaponinfo_t weaponinfo[NUMWEAPONS]={{0}};
void R_DrawColumnFlat(uint8_t,const draw_column_vars_t *);
void R_DrawColumnSprite(const draw_column_vars_t *);
enum { STRIDE=208, GUARD=32 };
static uint8_t memory[GUARD+STRIDE*228+GUARD];
int main(void) {
    memset(memory,0x55,sizeof(memory));
    uint8_t *fb=memory+GUARD;
    I_SetPebbleFramebuffer(fb,STRIDE,228);
    draw_column_vars_t dc={0}; dc.yl=-20; dc.yh=200;
    for(int x=0;x<120;++x) { dc.x=x; R_DrawColumnFlat(42,&dc); }
    for(int y=0;y<190;++y) {
        for(int x=0;x<200;++x) assert(fb[y*STRIDE+x]==0xea);
        for(int x=200;x<STRIDE;++x) assert(fb[y*STRIDE+x]==0x55);
    }
    uint8_t source[128]; memset(source,63,sizeof(source));
    dc.x=60; dc.yl=80; dc.yh=113; dc.source=source; dc.fracstep=512;
    R_DrawColumnSprite(&dc);
    assert(fb[140*STRIDE+100]==0xff);
    _g_player.health=82; _g_player.ammo[0]=24; _g_player.armorpoints=7;
    ST_PebbleDrawer(fb);
    for(int y=190;y<228;++y)
        for(int x=200;x<STRIDE;++x) assert(fb[y*STRIDE+x]==0x55);
    for(int i=0;i<GUARD;++i) {
        assert(memory[i]==0x55); assert(memory[sizeof(memory)-1-i]==0x55);
    }
    I_SetPebbleFramebuffer(NULL,STRIDE,228);
    R_DrawColumnFlat(0,&dc);
    assert(I_GetPebbleFramebuffer()==NULL);
    I_SetPebbleFramebuffer(fb,100,100);
    assert(I_GetPebbleFramebuffer()==NULL);
    puts("PASS: viewport coverage, clipping, sprite pixels, padded stride, framebuffer guards");
}
