/* Native watch lifecycle and input. Doom simulation runs at 35 Hz. */
#include <pebble.h>
#undef false
#undef true
#include "i_pebble.h"
#include "../doom/doomdef.h"
#include "../doom/d_player.h"
#include "../doom/d_main.h"
#include "../doom/g_game.h"
#include "../doom/r_main.h"
#include "../doom/globdata.h"
#include "../doom/i_system.h"
#include "../doom/w_wad.h"
#include "../doom/z_zone.h"
#include "../doom/p_map.h"
#include "../doom/tables.h"

static Window *s_main_window;
static Layer *s_canvas_layer;
static AppTimer *s_frame_timer;
pebble_input_state_t g_pebble_input;
static bool s_touching, s_up, s_down, s_select_held;
// A Select tap keeps the trigger down for at least this many tics, so a tap
// between two 35 Hz tics still fires one shot.
#define TAP_FIRE_TICS 3
static uint8_t s_fire_tics;
enum { TITLE, GAME, PAUSE, SETTINGS, HELP, SKILL, FATAL, ABOUT, MAP };
#define PDOOM_VERSION "0.3.0"   // keep in step with package.json
static uint8_t s_page=TITLE, s_parent=TITLE, s_choice, s_sensitivity=2, s_tilt_mode=1, s_skill=sk_medium;
static bool s_invert, s_vibe_enabled=true;
#define s_paused (s_page != GAME)
static bool s_touch_subscribed;
static int16_t s_touch_start_x, s_touch_start_y, s_touch_last_x;
static uint32_t s_touch_at, s_last_tick, s_accumulator;
static unsigned s_frames;
static uint32_t s_max_draw_ms, s_max_tick_ms, s_max_gap_ms;
static unsigned s_hits, s_clock_skips;
// Engine hint/pickup message ("You need a blue key...") shown for 2 s.
static const char *s_message;
static uint32_t s_message_until;
static gamestate_t s_last_state=GS_DEMOSCREEN;  // logged on change, e.g. "state 1 map 1" = level cleared

// Persistent storage keys (1-5 are settings).
enum { KEY_SKILL=6, KEY_SOUND=7, KEY_CHECKPOINT=10 };

// Checkpoint: the map, skill and inventory at the moment a level was entered
// (new game or next level). Continue, death retry and "Restart level" load
// the level again with this inventory; keys are per level, as in Doom.
#define CHECKPOINT_VERSION 1
typedef struct {
    uint8_t version, map, skill, backpack;
    int16_t health, armorpoints, armortype, readyweapon;
    int16_t weaponowned[NUMWEAPONS], ammo[NUMAMMO], maxammo[NUMAMMO];
} checkpoint_t;
static bool s_restore_pending;  // restore on the next player reborn
static bool s_restored;         // this level load came from a checkpoint

// Fatal engine errors longjmp back to the Pebble entry point that called
// into the engine; the engine is then abandoned and an error page shown.
// GCC's builtin setjmp/longjmp: newlib's versions pull in unwinder and
// abort() code the Pebble SDK cannot link. The builtin longjmp must be called
// from a different function than the setjmp, and its value is always 1.
#define setjmp(buf) __builtin_setjmp(buf)
#define longjmp(buf, v) __builtin_longjmp(buf, 1)
static void *s_fatal_jmp[5];
static bool s_fatal_armed;
static const char *s_fatal_message;
static uint32_t now_ms(void) {
    time_t sec; uint16_t ms; time_ms(&sec,&ms);
    return (uint32_t)sec*1000u+ms;
}
// 30 frames per second: the display needs no more, and the 35 Hz simulation
// is timed separately. Halves drawing work (and battery) versus 60 fps.
#define FRAME_INTERVAL_MS 33

static void clear_input(void) {
    memset(&g_pebble_input,0,sizeof(g_pebble_input));
    s_up=s_down=s_touching=s_select_held=false;
    s_fire_tics=0;
}
static void dirty(void) { if(s_canvas_layer) layer_mark_dirty(s_canvas_layer); }
// Sound effects (i_pebbles.c).
extern bool g_pebble_sound;
void I_PebbleSoundPump(void);
void I_PebbleSoundStop(void);
static void page(uint8_t next) {
    if(next!=GAME) I_PebbleSoundStop();
    s_page=next; s_choice=0; clear_input(); s_accumulator=0; dirty();
}
static void text(GContext *ctx,const char *str,int y,const char *font) {
    graphics_draw_text(ctx,str,fonts_get_system_font(font),GRect(10,y,180,40),GTextOverflowModeTrailingEllipsis,GTextAlignmentCenter,NULL);
}
/* After an exit, is there a map to continue to? E1M8 ends the episode. */
static bool next_map_exists(void) {
    if(_g_gamestate!=GS_INTERMISSION || _g_gamemap==8) return false;
    char name[]="E1M1";
    name[3]='1'+_g_wminfo.next;
    return W_GetNumForName(name)>=0;
}
// ----- checkpoints ---------------------------------------------------------
static __attribute__((noinline)) bool read_checkpoint(checkpoint_t *c) {
    return persist_exists(KEY_CHECKPOINT) &&
        persist_read_data(KEY_CHECKPOINT,c,sizeof(*c))==(int)sizeof(*c) &&
        c->version==CHECKPOINT_VERSION && c->map>=1 && c->map<=9 && c->skill<=sk_nightmare;
}
static __attribute__((noinline)) void save_checkpoint(void) {
    checkpoint_t c={CHECKPOINT_VERSION,(uint8_t)_g_gamemap,(uint8_t)_g_gameskill,(uint8_t)_g_player.backpack,
        _g_player.health,_g_player.armorpoints,_g_player.armortype,_g_player.readyweapon};
    memcpy(c.weaponowned,_g_player.weaponowned,sizeof(c.weaponowned));
    memcpy(c.ammo,_g_player.ammo,sizeof(c.ammo));
    memcpy(c.maxammo,_g_player.maxammo,sizeof(c.maxammo));
    persist_write_data(KEY_CHECKPOINT,&c,sizeof(c));
    APP_LOG(APP_LOG_LEVEL_INFO,"checkpoint saved: map %d skill %d hp%d",c.map,c.skill,c.health);
}
void I_PebbleRestoreCheckpoint(struct player_s *p) {
    checkpoint_t c;
    if(!s_restore_pending) return;
    s_restore_pending=false;
    if(!read_checkpoint(&c)) return;
    // A checkpoint taken on the brink of death would be unwinnable.
    p->health=c.health<50 ? 50 : c.health;
    p->armorpoints=c.armorpoints; p->armortype=c.armortype; p->backpack=c.backpack;
    memcpy(p->weaponowned,c.weaponowned,sizeof(c.weaponowned));
    memcpy(p->ammo,c.ammo,sizeof(c.ammo));
    memcpy(p->maxammo,c.maxammo,sizeof(c.maxammo));
    p->readyweapon=p->pendingweapon=(weapontype_t)c.readyweapon;
    s_restored=true;
    APP_LOG(APP_LOG_LEVEL_INFO,"checkpoint restored: map %d hp%d",c.map,p->health);
}

// ----- fatal errors -----------------------------------------------------------
static __attribute__((noinline)) void enter_fatal(void) {
    s_fatal_armed=false;
    s_page=FATAL; s_choice=0; clear_input(); dirty();
}
_Noreturn void I_PebbleFatal(const char *message) {
    s_fatal_message=message;
    if(s_fatal_armed) { s_fatal_armed=false; longjmp(s_fatal_jmp,1); }
    for(;;);   // no engine entry point to return to (startup window code)
}
// Run engine work; false if it hit a fatal error. Keeping the setjmp in this
// one small function spares the big callers from its register spills.
static __attribute__((noinline)) bool guarded(void (*fn)(void)) {
    if(setjmp(s_fatal_jmp)) return false;
    s_fatal_armed=true;
    fn();
    s_fatal_armed=false;
    return true;
}

// ----- menus ------------------------------------------------------------------
enum { A_CONTINUE, A_NEW, A_SETTINGS, A_CONTROLS, A_RESUME, A_RESTART, A_QUIT,
       A_EASY, A_NORMAL, A_HARD, A_SENS, A_TURN, A_TILT, A_HAPTIC, A_BACK, A_ABOUT, A_MAP, A_SOUND };
static __attribute__((noinline)) int menu(const char **labels,uint8_t *actions) {
    int n=0;
#define ITEM(label,action) (labels[n]=(label),actions[n++]=(action))
    if(s_page==TITLE) {
        checkpoint_t c;
        if(read_checkpoint(&c)) ITEM("Continue",A_CONTINUE);
        ITEM("New game",A_NEW); ITEM("Settings",A_SETTINGS); ITEM("Controls",A_CONTROLS);
        ITEM("About",A_ABOUT);
    } else if(s_page==SKILL) {
        ITEM("Easy",A_EASY); ITEM("Normal",A_NORMAL); ITEM("Hard",A_HARD);
    } else if(s_page==PAUSE) {
        ITEM("Resume",A_RESUME); ITEM("Map",A_MAP); ITEM("Restart level",A_RESTART); ITEM("Settings",A_SETTINGS);
        ITEM("Controls",A_CONTROLS); ITEM("Quit",A_QUIT);
    } else if(s_page==SETTINGS) {
        ITEM(s_sensitivity==1 ? "Sens: gentle" : s_sensitivity==2 ? "Sens: normal" : "Sens: fast",A_SENS);
        ITEM(s_invert ? "Turn: inverted" : "Turn: normal",A_TURN);
        ITEM(s_tilt_mode==0 ? "Tilt: off" : s_tilt_mode==1 ? "Tilt: steer" : "Tilt: strafe",A_TILT);
        ITEM(g_pebble_sound ? "Sound: on" : "Sound: off",A_SOUND);
        ITEM(s_vibe_enabled ? "Haptic: on" : "Haptic: off",A_HAPTIC);
        ITEM("Back",A_BACK);
    }
#undef ITEM
    return n;
}
// ----- automap ----------------------------------------------------------------
// North-up line map of the walls the player has seen, centred on the player.
// Walls white, steps/ledges orange, doors/switches yellow, exit red; secret
// doors are drawn as plain walls, as in Doom.
static uint8_t s_map_zoom=1;   // 0..2: 4, 8 or 16 map units per pixel
static void draw_automap(GContext *ctx) {
    graphics_context_set_fill_color(ctx,GColorBlack);
    graphics_fill_rect(ctx,GRect(0,0,200,228),0,GCornerNone);
    if(_g_gamestate!=GS_LEVEL || !_g_player.mo) return;
    const int shift=2+s_map_zoom, cx=100, cy=100;
    const int px=_g_player.mo->x>>FRACBITS, py=_g_player.mo->y>>FRACBITS;
    for(int16_t i=0;i<_g_numlines;++i) {
        if(!(_g_lines[i].r_flags & ML_MAPPED)) continue;
        const line_t *l=&_g_maplines[i];
        const sector_t __far *back=(l->flags & ML_SECRET) ? NULL : LN_BACKSECTOR(l);
        const sector_t __far *front=LN_FRONTSECTOR(l);
        GColor c=GColorWhite;
        int16_t special=_g_lines[i].special;
        if(special==11 || special==51) c=GColorRed;
        else if(special && !(l->flags & ML_SECRET)) c=GColorYellow;
        else if(back) {
            if(back->floorheight!=front->floorheight) c=GColorOrange;
            else if(back->ceilingheight!=front->ceilingheight) c=GColorDarkGray;
            else continue;   // flat opening: nothing to show
        }
        graphics_context_set_stroke_color(ctx,c);
        graphics_draw_line(ctx,
            GPoint(cx+((l->v1.x-px)>>shift),cy-((l->v1.y-py)>>shift)),
            GPoint(cx+((l->v2.x-px)>>shift),cy-((l->v2.y-py)>>shift)));
    }
    // Player arrow: 8 px long along the view angle.
    int32_t a=_g_player.mo->angle>>ANGLETOFINESHIFT;
    int dx=(finecosine(a)*8)>>FRACBITS, dy=(finesine(a)*8)>>FRACBITS;
    graphics_context_set_stroke_color(ctx,GColorGreen);
    graphics_context_set_stroke_width(ctx,3);
    graphics_draw_line(ctx,GPoint(cx-dx,cy+dy),GPoint(cx+dx,cy-dy));
    graphics_context_set_stroke_width(ctx,1);
    graphics_context_set_fill_color(ctx,GColorGreen);
    graphics_fill_circle(ctx,GPoint(cx+dx,cy-dy),3);   // the facing end
    graphics_context_set_text_color(ctx,GColorWhite);
    text(ctx,"Up/Down: zoom   Back: return",207,FONT_KEY_GOTHIC_14);
}

static void draw_menu(GContext *ctx) {
    if(s_page==MAP) { draw_automap(ctx); return; }
    graphics_context_set_fill_color(ctx,GColorBlack);
    graphics_fill_rect(ctx,GRect(0,0,200,228),0,GCornerNone);
    graphics_context_set_text_color(ctx,GColorWhite);
    static const char *const titles[]={"pDOOM","","PAUSED","SETTINGS","CONTROLS","DIFFICULTY","ERROR","ABOUT"};
    text(ctx,titles[s_page],12,FONT_KEY_GOTHIC_28_BOLD);
    if(s_page==HELP) {
        const char *lines[]={"Up / Down: move","Select: fire","Tilt / drag: turn","Tap: open / fire","Hold screen: weapon","Back: use door","Double Back: pause"};
        for(int i=0;i<7;++i) text(ctx,lines[i],46+22*i,FONT_KEY_GOTHIC_18);
    } else if(s_page==ABOUT) {
        graphics_draw_text(ctx,
            "Version " PDOOM_VERSION "\n"
            "Doom64KB engine for Pebble.\n"
            "Engine: GPL-2.0, id Software and\n"
            "Doom64KB authors. Source is\n"
            "available with this app.\n"
            "Art: Freedoom (BSD licence).\n"
            "Levels: CC0.",
            fonts_get_system_font(FONT_KEY_GOTHIC_14),GRect(8,46,184,160),
            GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
    } else if(s_page==FATAL) {
        graphics_draw_text(ctx,"pDOOM hit an error and stopped.",fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
            GRect(10,50,180,50),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
        graphics_draw_text(ctx,s_fatal_message ? s_fatal_message : "",fonts_get_system_font(FONT_KEY_GOTHIC_14),
            GRect(10,104,180,90),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
    } else {
        const char *items[6]; uint8_t actions[6];
        int count=menu(items,actions);
        for(int i=0;i<count;++i) {
            int y=(count==6 ? 38+24*i : count==5 ? 44+24*i : count==4 ? 50+27*i : 60+27*i);
            graphics_context_set_fill_color(ctx,GColorDarkGray);
            if(i==s_choice) graphics_fill_rect(ctx,GRect(12,y+2,176,23),3,GCornersAll);
            text(ctx,items[i],y,FONT_KEY_GOTHIC_18_BOLD);
        }
    }
    text(ctx,s_page==HELP || s_page==ABOUT ? "Back: return" : s_page==FATAL ? "Back: exit" : "Up/Down  Select",207,FONT_KEY_GOTHIC_14);
}
static void render_view(void) {
    if(_g_gamestate==GS_LEVEL && _g_player.mo) R_RenderPlayerView(&_g_player);
}
static void canvas_update_proc(Layer *layer,GContext *ctx) {
    if(s_page!=GAME) { draw_menu(ctx); return; }
    uint32_t draw_start=now_ms();
    GBitmap *fb=graphics_capture_frame_buffer_format(ctx,GBitmapFormat8Bit);
    if(!fb) return;
    ++s_frames;
    uint8_t *pixels=gbitmap_get_data(fb);
    I_SetPebbleFramebuffer(pixels,gbitmap_get_bytes_per_row(fb),PEBBLE_SCREEN_HEIGHT);
    bool ok=guarded(render_view);
    if(ok) ST_PebbleDrawer(pixels);
    I_SetPebbleFramebuffer(NULL,PEBBLE_SCREEN_WIDTH,PEBBLE_SCREEN_HEIGHT);
    graphics_release_frame_buffer(ctx,fb);
    if(!ok) { enter_fatal(); draw_menu(ctx); return; }
    graphics_context_set_text_color(ctx,GColorWhite);
    GFont label=fonts_get_system_font(FONT_KEY_GOTHIC_14);
    graphics_draw_text(ctx,"HP",label,GRect(18,190,40,16),GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
    graphics_draw_text(ctx,"AMMO",label,GRect(85,190,50,16),GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
    graphics_draw_text(ctx,"ARM",label,GRect(150,190,45,16),GTextOverflowModeTrailingEllipsis,GTextAlignmentLeft,NULL);
    if(s_message && _g_gamestate==GS_LEVEL) {
        if((int32_t)(now_ms()-s_message_until)<0) {
            GRect box=GRect(4,2,192,34);
            GSize size=graphics_text_layout_get_content_size(s_message,label,box,GTextOverflowModeWordWrap,GTextAlignmentCenter);
            graphics_context_set_fill_color(ctx,GColorBlack);
            graphics_fill_rect(ctx,GRect(0,0,200,size.h+6),0,GCornerNone);
            graphics_draw_text(ctx,s_message,label,box,GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
        } else s_message=NULL;
    }
    if(_g_gamestate!=GS_LEVEL || _g_player.playerstate==PST_DEAD) {
        graphics_context_set_fill_color(ctx,GColorBlack);
        graphics_fill_rect(ctx,GRect(10,40,180,130),0,GCornerNone);
        const char *title,*hint;
        static char stats[112];
        if(_g_gamestate==GS_LEVEL) { title="YOU DIED"; hint="Select: retry level\nDouble Back: menu"; }
        else {
            bool more=next_map_exists();
            title=more ? "LEVEL CLEAR" : "EPISODE CLEAR";
            snprintf(stats,sizeof(stats),"Kills %d/%d  Secrets %d/%d\nTime %d:%02d\n%s",(int)_g_wminfo.plyr[0].skills,
                (int)_g_wminfo.maxkills,(int)_g_wminfo.plyr[0].ssecret,(int)_g_wminfo.maxsecret,
                (int)(_g_wminfo.plyr[0].stime/TICRATE/60),(int)(_g_wminfo.plyr[0].stime/TICRATE%60),
                more ? "Select: next level" : "The base is quiet. For now.\nSelect: play again");
            hint=stats;
        }
        graphics_draw_text(ctx,title,fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),GRect(10,44,180,30),GTextOverflowModeTrailingEllipsis,GTextAlignmentCenter,NULL);
        graphics_draw_text(ctx,hint,label,GRect(16,78,168,88),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
    }
    int32_t draw_ms=(int32_t)(now_ms()-draw_start);
    if(draw_ms>(int32_t)s_max_draw_ms) s_max_draw_ms=draw_ms;
}
// Run the 35 Hz simulation tics owed by s_accumulator.
static void run_tics(void) {
    while(s_accumulator>=1000) {
        s_accumulator-=1000;
        g_pebble_input.forward_move=g_pebble_input.button_attack ? 0 : (s_up-s_down)*25;
        g_pebble_input.side_move=0;
        if(g_pebble_input.button_attack && (s_up || s_down)) {
            g_pebble_input.angle_turn+=(s_up-s_down)*900;
        }
        if(s_tilt_mode) {
            AccelData accel;
            if(accel_service_peek(&accel)==0 && !accel.did_vibrate) {
                int16_t ax=accel.x;
                int16_t deadzone=100;
                if(abs(ax)>deadzone) {
                    int16_t diff=abs(ax)-deadzone;
                    if(diff>600) diff=600;
                    int dir=(ax>0 ? -1 : 1)*(s_invert ? -1 : 1);
                    if(s_tilt_mode==1) {
                        int32_t turn=g_pebble_input.angle_turn + dir * diff * s_sensitivity;
                        if(turn>30000) turn=30000;
                        if(turn<-30000) turn=-30000;
                        g_pebble_input.angle_turn=turn;
                    } else if(s_tilt_mode==2) {
                        g_pebble_input.side_move=(dir>0 ? -25 : 25);
                    }
                }
            }
        }
        uint32_t tick_start=now_ms();
        int old_health=_g_player.health;
#if defined PDOOM_PLAYTEST
        // Playtest build only: holding Down for 2 s exits the level, so the
        // emulator check (which can press one button at a time) can visit
        // every map.
        static int s_down_tics;
        s_down_tics=s_down ? s_down_tics+1 : 0;
        if(s_down_tics==2*TICRATE && _g_gamestate==GS_LEVEL) {
            APP_LOG(APP_LOG_LEVEL_WARNING,"PLAYTEST warp");
            G_ExitLevel();
        }
#endif
        G_BuildTiccmd(); G_Ticker(); ++_g_gametic;
        if(s_fire_tics && !--s_fire_tics && !s_select_held) g_pebble_input.button_attack=false;
        if(_g_player.message) {
            s_message=_g_player.message; s_message_until=now_ms()+2000;
            _g_player.message=NULL;
        }
        if(_g_gamestate!=s_last_state) {
            s_last_state=_g_gamestate;
            APP_LOG(APP_LOG_LEVEL_INFO,"state %d map %d zone%lu",_g_gamestate,_g_gamemap,
                (unsigned long)Z_GetTotalFreeMemory());
            // Entering a level fresh (new game, next map) saves a checkpoint;
            // finishing the last map clears it so the title offers no Continue.
            if(_g_gamestate==GS_LEVEL && !s_restored) save_checkpoint();
            if(_g_gamestate==GS_LEVEL) {
                static const char *const names[]={"E1M1: Hangar Gate","E1M2: Toxin Refinery","E1M3: Command Center"};
                if(_g_gamemap>=1 && _g_gamemap<=(int)ARRAY_LENGTH(names)) {
                    s_message=names[_g_gamemap-1]; s_message_until=now_ms()+2500;
                }
            }
            if(_g_gamestate==GS_INTERMISSION && !next_map_exists()) persist_delete(KEY_CHECKPOINT);
            s_restored=false;
        }
        if(_g_player.health<old_health) {
            ++s_hits;
            if(s_vibe_enabled) vibes_double_pulse();
        }
        int32_t tick_ms=(int32_t)(now_ms()-tick_start);
        if(tick_ms>(int32_t)s_max_tick_ms) s_max_tick_ms=tick_ms;
        if(_g_gametic%105==0 && _g_player.mo) {
            APP_LOG(APP_LOG_LEVEL_INFO,"tick %ld pos %ld,%ld hp%d ammo%d kills%d frames%u heap%lu zone%lu",_g_gametic,
                _g_player.mo->x>>16,_g_player.mo->y>>16,_g_player.health,
                _g_player.ammo[0],_g_player.killcount,s_frames,(unsigned long)heap_bytes_free(),
                (unsigned long)Z_GetTotalFreeMemory());
            APP_LOG(APP_LOG_LEVEL_INFO,"timing draw%lu tick%lu gap%lu hits%u skips%u",(unsigned long)s_max_draw_ms,(unsigned long)s_max_tick_ms,(unsigned long)s_max_gap_ms,s_hits,s_clock_skips);
            s_frames=s_hits=s_clock_skips=0; s_max_draw_ms=s_max_tick_ms=s_max_gap_ms=0;
        }
    }
}
static void frame_timer_callback(void *data) {
    s_frame_timer=NULL;
    // time_ms() jumps by about a second around each second boundary in the
    // emulator. Count an implausible jump as one nominal frame instead.
    uint32_t now=now_ms(), elapsed=FRAME_INTERVAL_MS;
    int32_t delta=(int32_t)(now-s_last_tick);
    s_last_tick=now;
    if(delta>=0 && delta<=500) elapsed=delta;
    else if(!s_paused) ++s_clock_skips;
    if(!s_paused && elapsed>s_max_gap_ms) s_max_gap_ms=elapsed;
    if(!s_paused) {
        if(elapsed>120) elapsed=120;
        s_accumulator+=elapsed*TICRATE;
        if(!guarded(run_tics) || !guarded(I_PebbleSoundPump)) { enter_fatal(); s_accumulator=0; }
    } else s_accumulator=0;
    if(s_canvas_layer) layer_mark_dirty(s_canvas_layer);
    s_frame_timer=app_timer_register(s_paused ? 200 : FRAME_INTERVAL_MS,frame_timer_callback,NULL);
}
static void touch_tap(void);
static void touch_handler(const TouchEvent *e,void *context) {
    if(!e || s_paused) return;
    uint32_t now=now_ms();
    if(e->type==TouchEvent_Touchdown) {
        s_touching=true;
        s_touch_start_x=s_touch_last_x=e->x; s_touch_start_y=e->y; s_touch_at=now;
    } else if(e->type==TouchEvent_PositionUpdate && s_touching) {
        int dx=e->x-s_touch_last_x; s_touch_last_x=e->x;
        int direction=s_invert ? -1 : 1;
        int32_t turn=g_pebble_input.angle_turn-direction*dx*(abs(dx)<=3 ? 40 : 80)*s_sensitivity;
        if(turn>30000) turn=30000;
        if(turn< -30000) turn=-30000;
        g_pebble_input.angle_turn=turn;
    } else if(e->type==TouchEvent_Liftoff && s_touching) {
        // A touch that stays put is a tap (action) or, held, a weapon change.
        if(abs(e->x-s_touch_start_x)<8 && abs(e->y-s_touch_start_y)<8) {
            if(now-s_touch_at<250) touch_tap();
            else if(now-s_touch_at>=500) g_pebble_input.weapon_cycle=true;
        }
        s_touching=false;
    }
}
// Start at map 1 with the given skill, or at the checkpoint (continue).
static __attribute__((noinline)) void start_game(bool from_checkpoint) {
    checkpoint_t c;
    APP_LOG(APP_LOG_LEVEL_INFO,"new game: zone%lu heap%lu",(unsigned long)Z_GetTotalFreeMemory(),
        (unsigned long)heap_bytes_free());
#if defined PDOOM_PLAYTEST
    APP_LOG(APP_LOG_LEVEL_WARNING,"PLAYTEST build: player invulnerable");
#endif
    page(GAME);
    if(from_checkpoint && read_checkpoint(&c)) {
        s_restore_pending=true;
        G_DeferedInitNewMap((skill_t)c.skill,c.map);
    } else {
        s_restore_pending=false;
        G_DeferedInitNew((skill_t)s_skill);
    }
}
// Reload the current level with the inventory it was entered with.
static void restart_level(void) {
    s_restore_pending=true;
    _g_player.playerstate=PST_REBORN;
}
static void move_choice(int step) {
    const char *labels[6]; uint8_t actions[6];
    int count=menu(labels,actions);
    if(count) s_choice=(s_choice+count+step)%count;
    dirty();
}
static void up_press(ClickRecognizerRef r,void *c) {
    if(s_page==MAP) { if(s_map_zoom) --s_map_zoom; dirty(); }
    else if(s_paused) move_choice(-1); else s_up=true;
}
static void up_release(ClickRecognizerRef r,void *c) { s_up=false; }
static void down_press(ClickRecognizerRef r,void *c) {
    if(s_page==MAP) { if(s_map_zoom<2) ++s_map_zoom; dirty(); }
    else if(s_paused) move_choice(1); else s_down=true;
}
static void down_release(ClickRecognizerRef r,void *c) { s_down=false; }
// On the death / level-end screens Select (or a tap) moves on; returns
// whether it did.
static bool advance_screen(void) {
    if(_g_gamestate==GS_LEVEL && _g_player.playerstate==PST_DEAD) restart_level();
    else if(next_map_exists()) G_WorldDone();
    else if(_g_gamestate!=GS_LEVEL) start_game(false);
    else return false;
    return true;
}
static void select_press(ClickRecognizerRef r,void *c) {
    if(s_page==HELP || s_page==ABOUT || s_page==MAP) { page(s_parent); return; }
    if(s_page==FATAL) return;
    if(s_page!=GAME) {
        const char *labels[6]; uint8_t actions[6];
        int count=menu(labels,actions);
        if(s_choice>=count) return;
        uint8_t previous=s_page;
        switch(actions[s_choice]) {
        case A_CONTINUE: start_game(true); break;
        case A_NEW: page(SKILL); s_choice=s_skill==sk_baby ? 0 : s_skill==sk_hard ? 2 : 1; dirty(); break;
        case A_EASY: case A_NORMAL: case A_HARD:
            // Easy is Doom's "I'm too young to die": half damage, double ammo.
            s_skill=actions[s_choice]==A_EASY ? sk_baby : actions[s_choice]==A_HARD ? sk_hard : sk_medium;
            persist_write_int(KEY_SKILL,s_skill);
            start_game(false); break;
        case A_SETTINGS: s_parent=previous; page(SETTINGS); break;
        case A_CONTROLS: s_parent=previous; page(HELP); break;
        case A_ABOUT: s_parent=previous; page(ABOUT); break;
        case A_MAP: s_parent=previous; page(MAP); break;
        case A_RESUME: page(GAME); break;
        case A_RESTART: page(GAME); restart_level(); break;
        case A_QUIT: window_stack_pop(true); break;
        case A_SENS: s_sensitivity=s_sensitivity%3+1; persist_write_int(1,s_sensitivity); break;
        case A_TURN: s_invert=!s_invert; persist_write_bool(2,s_invert); break;
        case A_TILT: s_tilt_mode=(s_tilt_mode+1)%3; persist_write_int(3,s_tilt_mode); break;
        case A_HAPTIC: s_vibe_enabled=!s_vibe_enabled; persist_write_bool(4,s_vibe_enabled); break;
        case A_SOUND: g_pebble_sound=!g_pebble_sound; persist_write_bool(KEY_SOUND,g_pebble_sound); break;
        case A_BACK: page(s_parent); break;
        }
        dirty(); return;
    }
    if(!advance_screen()) { g_pebble_input.button_attack=true; s_select_held=true; s_fire_tics=TAP_FIRE_TICS; }
}
// Screen tap: use a door or switch straight ahead, otherwise fire one shot.
static void touch_tap(void) {
    if(advance_screen()) return;
    if(_g_player.mo && P_UsableLineAhead(&_g_player)) g_pebble_input.button_use=true;
    else { g_pebble_input.button_attack=true; s_fire_tics=TAP_FIRE_TICS; }
}
static void select_release(ClickRecognizerRef r,void *c) {
    s_select_held=false;
    if(!s_fire_tics) g_pebble_input.button_attack=false;   // else released after the tap's tics
}
static void back_click(ClickRecognizerRef r,void *c) {
    if(s_page==FATAL) { window_stack_pop(true); return; }
    if(s_page==SKILL) { page(TITLE); return; }
    if(s_page==SETTINGS || s_page==HELP || s_page==ABOUT || s_page==MAP) { page(s_parent); return; }
    if(s_page==TITLE) { window_stack_pop(true); return; }
    if(s_page==PAUSE) { page(GAME); return; }
    if(click_number_of_clicks_counted(r)>1) page(PAUSE);
    else if(g_pebble_input.button_attack) g_pebble_input.weapon_cycle=true;
#if defined PDOOM_PLAYTEST
    // The emulator has no touch input: in the playtest build a single Back
    // runs the screen-tap action so tools/check_tap.py can test it.
    else touch_tap();
#else
    else g_pebble_input.button_use=true;
#endif
}
static void focus_changed(bool focused) { if(!focused && s_page==GAME) page(PAUSE); }
static void click_config_provider(void *context) {
    window_multi_click_subscribe(BUTTON_ID_BACK,1,2,250,true,back_click);
    window_raw_click_subscribe(BUTTON_ID_UP,up_press,up_release,NULL);
    window_raw_click_subscribe(BUTTON_ID_DOWN,down_press,down_release,NULL);
    window_raw_click_subscribe(BUTTON_ID_SELECT,select_press,select_release,NULL);
}
static void main_window_load(Window *window) {
    Layer *root=window_get_root_layer(window);
    s_canvas_layer=layer_create(layer_get_bounds(root));
    if(!s_canvas_layer) I_Error("Canvas allocation");
    layer_set_update_proc(s_canvas_layer,canvas_update_proc);
    layer_add_child(root,s_canvas_layer);
    WatchInfoVersion v=watch_info_get_firmware_version();
    if(v.major>4 || (v.major==4 && v.minor>=33)) {
        app_touch_navigation_enable(false);
        window_set_touch_bridge_disabled(window,true);
        if(touch_service_is_enabled()) {
            touch_service_subscribe(touch_handler,NULL); s_touch_subscribed=true;
        }
    }
    s_last_tick=now_ms();
    accel_service_set_sampling_rate(ACCEL_SAMPLING_50HZ);
    s_frame_timer=app_timer_register(s_paused ? 200 : FRAME_INTERVAL_MS,frame_timer_callback,NULL);
}
static void main_window_unload(Window *window) {
    I_PebbleSoundStop();
    if(s_frame_timer) { app_timer_cancel(s_frame_timer); s_frame_timer=NULL; }
    if(s_touch_subscribed) touch_service_unsubscribe();
    layer_destroy(s_canvas_layer); s_canvas_layer=NULL;
}
static void start_engine(void) {
    const char *argv[]={"pdoom"};
    D_DoomMain(1,argv);
}
int main(void) {
    int saved=persist_read_int(1);
    if(saved>=1 && saved<=3) s_sensitivity=saved;
    s_invert=persist_read_bool(2);
    if(persist_exists(3)) {
        int saved_tilt=persist_read_int(3);
        if(saved_tilt>=0 && saved_tilt<=2) s_tilt_mode=saved_tilt;
    }
    if(persist_exists(4)) s_vibe_enabled=persist_read_bool(4);
    if(persist_exists(KEY_SOUND)) g_pebble_sound=persist_read_bool(KEY_SOUND);
    if(persist_exists(KEY_SKILL)) {
        int skill=persist_read_int(KEY_SKILL);
        if(skill>=sk_baby && skill<=sk_hard) s_skill=skill;
    }
    app_focus_service_subscribe(focus_changed);
    if(!guarded(start_engine)) enter_fatal();   // engine start-up failed
    else {
        checkpoint_t c;
        APP_LOG(APP_LOG_LEVEL_INFO,"title: continue %d",read_checkpoint(&c) ? 1 : 0);
    }
    s_main_window=window_create();
    if(!s_main_window) I_Error("Window allocation");
    window_set_background_color(s_main_window,GColorBlack);
    window_set_click_config_provider(s_main_window,click_config_provider);
    window_set_window_handlers(s_main_window,(WindowHandlers){.load=main_window_load,.unload=main_window_unload});
    window_stack_push(s_main_window,false);
    app_event_loop();
    app_focus_service_unsubscribe();
    window_destroy(s_main_window);
    return 0;
}
