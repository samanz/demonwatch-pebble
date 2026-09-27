/* Sound effects: engine sounds mapped onto a few Freedoom samples (8 kHz
 * signed PCM lumps in the resource, see tools/assets.py SOUNDS) and streamed
 * to the speaker one at a time. Nothing is loaded into RAM beyond a small
 * transfer buffer; the frame timer tops the stream up every frame. */
#include <pebble.h>
#undef false
#undef true
#include "../doom/doomdef.h"
#include "../doom/m_fixed.h"
#include "../doom/d_player.h"
#include "../doom/sounds.h"
#include "../doom/w_wad.h"
#include "../doom/globdata.h"

bool g_pebble_sound = true;   // Settings > Sound

static const char s_names[][9] = {
    "DSPISTOL", "DSSHOTGN", "DSDOROPN", "DSSWTCHN", "DSITEMUP", "DSPLPAIN", "DSPODTH1",
};
enum { PISTOL, SHOTGUN, DOOR, SWITCH, ITEM, PAIN, DEATH };
static int16_t s_lumps[ARRAY_LENGTH(s_names)];   // 0 = not looked up yet

// Engine sound -> sample and priority (a sound only interrupts one of equal
// or lower priority). Unlisted sounds are silent.
static const struct { uint8_t sfx, sample, priority; } s_map[] = {
    {sfx_pistol, PISTOL, 2}, {sfx_shotgn, SHOTGUN, 2},
    {sfx_doropn, DOOR, 1}, {sfx_dorcls, DOOR, 1},
    {sfx_swtchn, SWITCH, 1}, {sfx_swtchx, SWITCH, 1}, {sfx_pstart, SWITCH, 1},
    {sfx_itemup, ITEM, 1}, {sfx_wpnup, ITEM, 1}, {sfx_getpow, ITEM, 1},
    {sfx_plpain, PAIN, 3}, {sfx_pldeth, PAIN, 3}, {sfx_pdiehi, PAIN, 3},
    {sfx_podth1, DEATH, 2}, {sfx_podth2, DEATH, 2}, {sfx_podth3, DEATH, 2},
    {sfx_bgdth1, DEATH, 2}, {sfx_bgdth2, DEATH, 2}, {sfx_sgtdth, DEATH, 2},
};

#define VOLUME 70           // 0-100
#define HEARING 1400        // map units (|dx| + |dy|) beyond which sounds are dropped

static int16_t s_lump = -1;  // playing lump, or -1
static uint16_t s_pos, s_len;
static uint8_t s_priority;
static uint8_t s_buf[256];

void I_PebbleSoundStop(void) {
    if (s_lump >= 0) speaker_stop();
    s_lump = -1;
}

// Feed the open stream until it is full or the sample has been written.
void I_PebbleSoundPump(void) {
    if (s_lump < 0) return;
    while (s_pos < s_len) {
        uint16_t n = W_ReadLumpRange(s_lump, s_pos, s_buf, sizeof(s_buf));
        uint32_t written = speaker_stream_write(s_buf, n);
        s_pos += written;
        if (written < n) return;   // stream buffer full: continue next frame
    }
    speaker_stream_close();        // plays out what is buffered
    s_lump = -1;
}

void I_PebbleSound(int16_t sfx, boolean positioned, fixed_t x, fixed_t y) {
    if (!g_pebble_sound || speaker_is_muted()) return;
    unsigned i = 0;
    while (i < ARRAY_LENGTH(s_map) && s_map[i].sfx != sfx) ++i;
    if (i == ARRAY_LENGTH(s_map)) return;
    if (s_lump >= 0 && s_map[i].priority < s_priority) return;

    int volume = VOLUME;
    if (positioned && _g_player.mo) {
        int32_t d = D_abs((x - _g_player.mo->x) >> FRACBITS) + D_abs((y - _g_player.mo->y) >> FRACBITS);
        if (d > HEARING) return;
        if (d > 400) volume = VOLUME * (HEARING - d) / (HEARING - 400) + 10;
    }

    uint8_t sample = s_map[i].sample;
    if (!s_lumps[sample]) {
        int16_t lump = W_GetNumForName(s_names[sample]);
        s_lumps[sample] = lump >= 0 ? lump : -1;
    }
    if (s_lumps[sample] < 0) return;   // not in this resource build

    speaker_stop();
    if (!speaker_stream_open(SpeakerPcmFormat_8kHz_8bit, volume)) { s_lump = -1; return; }
    s_lump = s_lumps[sample];
    s_pos = 0;
    s_len = W_LumpLength(s_lump);
    s_priority = s_map[i].priority;
#if defined PDOOM_PLAYTEST
    APP_LOG(APP_LOG_LEVEL_INFO, "sfx %s vol %d", s_names[sample], volume);
#endif
    I_PebbleSoundPump();
}
