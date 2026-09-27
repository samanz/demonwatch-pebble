/*-----------------------------------------------------------------------------
 *  i_pebble_compat.c: Doom engine system interface and compatibility stubs
 *  for Pebble Time 2.
 *-----------------------------------------------------------------------------*/

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
extern int vsnprintf(char *, size_t, const char *, va_list);
#undef false
#undef true
#include "../doom/doomdef.h"
#include "../doom/doomtype.h"
#include "../doom/i_system.h"
#include "../doom/i_video.h"
#include "../doom/sounds.h"

extern void *malloc(size_t size);
extern void app_log(uint8_t log_level, const char* src_filename, int src_line_number, const char* fmt, ...);
static uint8_t *s_zone = NULL;
static uint32_t s_zone_size = 0;

uint8_t __far* I_ZoneBase(uint32_t *heapSize) {
    if (!s_zone) {
        *heapSize = 24 * 1024;
        uint8_t *raw = (uint8_t*)malloc(*heapSize + 32);
        if (!raw) {
            *heapSize = 18 * 1024;
            raw = (uint8_t*)malloc(*heapSize + 32);
        }
        if (!raw) I_Error("Cannot allocate Doom zone");
        s_zone_size = *heapSize;
        uintptr_t addr = ((uintptr_t)raw + 15) & ~(uintptr_t)15;
        s_zone = (uint8_t*)addr;
        app_log(100, "doom", 0, "Zone alloc: raw=%p aligned=%p size=%lu", raw, s_zone, *heapSize);
    }
    *heapSize = s_zone_size;
    return s_zone;
}

// Timer & System
static int32_t s_gametic = 0;
void I_InitTimer(void) { s_gametic = 0; }
int32_t I_GetTime(void) { return ++s_gametic; }

_Noreturn void I_Quit(void) { for(;;); }
_Noreturn void I_Error(const char *error, ...) {
    app_log(1, "doom", 0, "FATAL: %s", error);
    for(;;);
}
void exit(int code) { (void)code; for(;;); }

int printf(const char *fmt, ...) {
    app_log(100, "doom", 0, "%s", fmt);
    return 0;
}

int puts(const char *s) {
    app_log(100, "doom", 0, "%s", s);
    return 0;
}

void I_InitKeyboard(void) {}
void I_InitGraphics(void) {}
void I_FinishUpdate(void) {}
void I_ReloadPalette(void) {}
void I_SetPalette(int8_t pal) { (void)pal; }
void I_StartTic(void) {}

// Sound Effects / Audio Stubs
void I_InitSound(void) {}
void I_InitSound2(void) {}
int16_t I_StartSound(sfxenum_t id, int16_t channel, int16_t vol, int16_t sep) { (void)id; (void)channel; (void)vol; (void)sep; return 1; }
void DMX_Play(sfxenum_t id, int16_t channel) { (void)id; (void)channel; }
void DMX_Init(void) {}
void DMX_Init2(void) {}
void DMX_Shutdown(void) {}
void I_SetMusicVolume(int16_t volume) { (void)volume; }
void I_StopSong(musicenum_t handle) { (void)handle; }
void I_PlaySong(musicenum_t handle, boolean looping) { (void)handle; (void)looping; }

// Engine Configuration Globals
uint16_t _g_gamma = 0;
int16_t _g_alwaysRun = 0;
int16_t showMessages = 1;
boolean _g_menuactive = false;
char _g_savegamestrings[8][8] = {0};

// Menu Stubs
void M_Init(void) {}
void M_Ticker(void) {}
void M_Drawer(void) {}

// Heads-Up Display (Chat / Messages) Stubs
void HU_Init(void) {}
void HU_Start(void) {}
void HU_Ticker(void) {}
void HU_Drawer(void) {}

// Status Bar Stubs
void ST_Init(void) {}
void ST_Start(void) {}
void ST_Ticker(void) {}
void ST_Drawer(void) {}
void ST_doPaletteStuff(void) {}

// Finale Stubs
void F_Init(void) {}
void F_StartFinale(void) {}
void F_Ticker(void) {}
void F_Drawer(void) {}

// Video hardware initialization
void I_InitGraphicsHardwareSpecificCode(void) {}
void I_ShutdownGraphics(void) {}

// 2D Primitives
void V_ClearViewWindow(void) {}
void V_InitDrawLine(void) {}
void V_ShutdownDrawLine(void) {}
void V_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color) {
    (void)x0; (void)y0; (void)x1; (void)y1; (void)color;
}
void V_DrawBackground(int16_t backgroundnum) { (void)backgroundnum; }
void V_DrawRaw(int16_t num, uint16_t offset) { (void)num; (void)offset; }
void V_DrawRawFullScreen(int16_t num) { (void)num; }
void V_DrawPatchNotScaled(int16_t x, int16_t y, const patch_t __far* patch) { (void)x; (void)y; (void)patch; }
void V_DrawPatchScaled(   int16_t x, int16_t y, const patch_t __far* patch) { (void)x; (void)y; (void)patch; }

void wipe_StartScreen(void) {}
void D_Wipe(void) {}

// Screen and Text
void V_DrawCharacter(int16_t x, int16_t y, uint8_t color, char c) { (void)x; (void)y; (void)color; (void)c; }
void V_DrawString(int16_t x, int16_t y, uint8_t color, const char* s) { (void)x; (void)y; (void)color; (void)s; }
void V_ClearString(int16_t y, size_t len) { (void)y; (void)len; }
void I_InitScreenPages(void) {}
void I_InitScreenPage(void) {}
void V_DrawCharacterForeground(int16_t x, int16_t y, uint8_t color, char c) { (void)x; (void)y; (void)color; (void)c; }
void V_DrawSTCharacter(int16_t x, int16_t y, uint8_t color, char c) { (void)x; (void)y; (void)color; (void)c; }
void V_DrawSTString(int16_t x, int16_t y, uint8_t color, const char* s) { (void)x; (void)y; (void)color; (void)s; }
void V_SetSTPalette(void) {}

#include "../doom/globdata.h"

// Automap & Intermission Stubs
enum automapmode_e automapmode = 0;
boolean _g_acceleratestage = false;
void AM_Drawer(void) {}
void AM_Stop(void) {}
void AM_Ticker(void) {}
void WI_Init(void) {}
void WI_Drawer(void) {}
void WI_Start(wbstartstruct_t* ep) { (void)ep; }
void WI_End(void) {}
void WI_Ticker(void) {}
int WI_checkForAccelerate(void) { return 0; }
