#pragma once

#include <stdint.h>
#include "../doom/doomtype.h"

#define PEBBLE_SCREEN_WIDTH   200
#define PEBBLE_SCREEN_HEIGHT  228
#define PEBBLE_VIEW_HEIGHT    190
#define PEBBLE_HUD_HEIGHT     38
#define PEBBLE_HUD_Y_START    190

// Framebuffer interface
void I_SetPebbleFramebuffer(uint8_t *fb, uint16_t width, uint16_t height);
uint8_t *I_GetPebbleFramebuffer(void);

// Input interface for G_BuildTiccmd
typedef struct {
    int16_t forward_move;   // +25 forward, -25 backward
    int16_t side_move;      // +25 right, -25 left (strafe)
    int16_t angle_turn;     // BAM angle delta from touch drag/flick
    boolean button_attack;  // Select button pressed
    boolean button_use;     // Back button pressed
    boolean weapon_cycle;   // Long select click
} pebble_input_state_t;

extern pebble_input_state_t g_pebble_input;

// Subsystem lifecycle
void I_PebbleInit(void);
void I_PebbleShutdown(void);
void I_PebbleTick(void);

// HUD drawer
void ST_PebbleDrawer(uint8_t *fb);

// Checkpoints: called by G_PlayerReborn; restores the inventory saved when
// the current level was entered, if a continue/retry asked for it.
struct player_s;
void I_PebbleRestoreCheckpoint(struct player_s *player);

// Fatal errors (I_Error): shows an error screen instead of hanging.
_Noreturn void I_PebbleFatal(const char *message);
