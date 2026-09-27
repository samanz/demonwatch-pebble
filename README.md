# pDOOM 0.3

Native Doom64KB-derived game for Pebble Time 2 (Emery), built with Pebble SDK 4.33.1. It contains a three-map episode of original Doom-style levels (E1M1 Hangar Gate, E1M2 Toxin Refinery, E1M3 Command Center) with doors, key doors, a lift, stairs, nukage, windows and outdoor areas; zombiemen, shotgun guys, imps and demons; and the fist, pistol, shotgun and chaingun. Art is adapted from Freedoom 0.13.0. No commercial Doom WAD is required or bundled.

## Install and play

Install `pdoom-alpha.pbw` using a Pebble app installation workflow supporting Emery. The development build is `build/pdoom.pbw`.

- Up / Down: move forward / backward.
- Tap Select: fire one shot (a tap holds the trigger for 3 tics, so even a very quick tap fires). Hold Select: keep firing; while held, Up / Down turn left / right.
- Back once: use door or exit switch.
- Hold Select and click Back: cycle owned weapons.
- Tilt the watch left / right: turn (default) or strafe, chosen in Settings.
- Double Back: open the pause menu. Up/Down choose Resume, Map, Restart level, Settings, Controls, or Quit; Select confirms. Back resumes.
- Map (pause menu): a north-up line map of the walls you have seen, centred on you with a green arrow (dot = facing). Walls white, steps and ledges orange, doors and switches yellow, the exit red. Up/Down zoom; Back returns.
- Launch opens a title menu: Continue (when a checkpoint exists), New game, Settings, Controls, About (version, licences and credits). New game asks for a difficulty: Easy (Doom's "too young to die": half damage, double ammo), Normal, or Hard; the choice is remembered.
- Checkpoints: entering a level (new game or next map) saves the map, difficulty and inventory. Continue resumes there after quitting; dying and the pause menu's "Restart level" reload the level with that inventory (health at least 50). Keys are per level, as in Doom. Finishing the episode clears the checkpoint.
- Difficulty changes the monsters: Easy has about 40% of Normal+Hard's, Normal about 70%, Hard all of them plus a few extras; every difficulty gets the same health pickups.
- Sound effects: Freedoom's pistol, shotgun, door, switch/lift, item, pain and death sounds, streamed from the resource file at 8 kHz through the watch speaker (one at a time; distant sounds are quieter or dropped). The watch's own mute and Quiet Time silence them.
- If the engine hits a fatal error, an error page replaces the game (Back exits) instead of the app freezing.
- Settings save touch/tilt sensitivity (gentle/normal/fast), turn direction, tilt mode (off/steer/strafe), sound effects, and haptics (a buzz when you take damage). There is no music.
- Losing app focus clears held inputs and pauses gameplay.
- Hold Back: system exit.
- Touch: tap the screen to act: it uses a door, switch or lift straight ahead (within Doom's 64-unit use range), and otherwise fires one shot; an already open door counts as nothing ahead, so tapping in a doorway fires. On the death and level-end screens a tap moves on like Select. Drag horizontally to turn; touch and hold (half a second) to cycle weapons.
- After death: Select retries the current level.
- After the exit switch: Select continues to the next map (E1M2, E1M3, … when present, keeping inventory). After the last map, Select starts a new game.

Each level announces its name on entry and has one secret (a door disguised as wall, with a stash behind it); the level-end screen shows kills, secrets found and time, and the last map ends with an episode-clear screen. Doors open with Back and close again after a few seconds. Key doors (blue, yellow stripes beside the door) need the matching keycard. A lift is used by pressing Back at its side. Each map ends at an exit switch.

## Verified scope and limits

The Emery emulator was used to exercise movement, pickup, door use, killing enemies in both combat rooms, exiting, restarting, and pause/restart. Native AddressSanitizer/UndefinedBehaviorSanitizer checks cover resource storage and framebuffer drawing. Four asset tests check map records, sprite frames, and texture references.

The renderer uses 120 x 114 logical pixels scaled to a 200 x 190 view, plus a health/ammo/armor HUD with keycard squares (blue, yellow, red) at the right edge. Engine hint and pickup messages ("You need a blue key...", "Picked up a shotgun.") appear at the top for 2 seconds; the level-end screen shows kills and time. Sector lighting follows Doom64KB: each sector's light level picks one of 32 64-entry light tables (lights of 160 and above are full bright, 128 dim, 96 dark), horizontal and vertical walls get Doom's fake contrast, gun flashes brighten, and flicker/strobe/glow sector specials animate. Floors and ceilings are solid colours; the sky is a flat blue-grey. Freedoom art is converted with a mild colour boost and a 2x2 ordered dither so greys and browns keep their shading on the 64-colour display. Engine simulation runs at 35 ticks per second. Physical Emery hardware running PebbleOS 4.38.2 has now launched successfully and logged movement. Extended gameplay, battery use, and touch feel still need hardware testing. There is no sound, save system, full E1M1, or arbitrary WAD support. This is a playable prototype, not an App Store release.

SDK measurements: resources 244,457 bytes (of 256 KB); initial available heap 67,197 bytes, about 9 KB free during play. The loaded image (63,876 bytes) must fit the SDK header's 65,535-byte field, leaving only about 1.6 KB of code/static-data headroom: new features need matching savings first.

Space-saving conventions (keep them when adding content):
- `states[]` stores each action as an 8-bit index into `actionfuncs[]` and is packed to 8 bytes per state. Its fields must stay in the same order as the positional initializers in `info.c`; `tests/test_info.c` checks every index is in range.
- `sprnames` is an inline `char[][5]` table; function-pointer and string-pointer tables cost extra because position-independent code relocates them into RAM.
- Demo playback, SRAM save/load, the title demo loop, and engine `printf` progress messages are compiled out under `PEBBLE_EMERY`.
- Scratch tables needed only at startup (such as the sprite-frame table) are heap-allocated and freed.

Memory: the Doom zone is 28 KB (falling back to 24/20/18 KB if the heap is short), leaving about 7 KB of PebbleOS heap during play. About 6 KB of the zone holds permanent data (sprite frame tables store only the front view; TEXTURE1 carries names only; 2 KB of light tables), leaving about 22.8 KB per level; the tightest map keeps at least 7.5 KB free. The game draws at 30 frames per second (the simulation stays at 35 tics per second) to save battery. Map lumps (THINGS through BLOCKMAP after each `E1Mx` marker) are released on every level load. Global lumps stay resident; sprite and wall patches stream through one scratch buffer. The app log reports free zone memory (`zone`) every 105 tics and on every game-state change.

## Making levels

Levels are Python scripts in `tools/levels/` (`e1m1.py` ...) using `tools/doommap.py`: each sector is a polygon with floor/ceiling heights, flat colours (6-bit Pebble colour codes, `SKY` or `NUKAGE`), light, special, tag and a wall texture; holes make pillars and pools. Shared edges become two-sided lines automatically (T-junctions are split), upper/lower textures are chosen from the heights, and helpers add doors (`m.door`, including key doors) and per-edge specials/textures (`m.edge`). `tools/build_wad.py` runs a design check (things inside the map and clear of walls; exit reachable from the start through doors, lifts, stairs and keys), builds nodes with zdbsp, converts to Doom64KB's compact lumps (`tools/convert_map.py`), and fails if a map exceeds the format limits or the per-level zone budget. Texture names come from `tools/assets.py`.

zdbsp is installed without root: `apt-get download zdbsp && dpkg-deb -x zdbsp_*.deb ~/.local/zdbsp` (or set `ZDBSP`).

## Build

Use Linux/WSL with Pebble SDK 4.33.1 and its ARM toolchain available on PATH:

```sh
python3 tools/build_wad.py
pebble build
python3 tools/verify_pbw.py build/pdoom.pbw
pebble install --emulator emery
```

`tools/run_pebble.py` is a convenience wrapper for the original development machine's SDK paths; adapt it for another machine or use `pebble` directly. The checked-in Freedoom art pack makes the resource build independent of downloading the original release.

To regenerate the art pack (after changing `tools/assets.py`), place the official Freedoom 0.13.0 zip at `work/freedoom-0.13.0.zip`, run `python3 tools/import_freedoom.py work/freedoom-0.13.0.zip`, then `python3 tools/build_wad.py`. Archive SHA256: `3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59`.

```sh
python3 -m unittest discover -s tests -v
gcc -std=c11 -g -fsanitize=address,undefined -Itests tests/test_wad.c src/pebble/pebble_wad.c -o work/test_wad
ASAN_OPTIONS=detect_leaks=0 ./work/test_wad resources/pdoom.pbl
gcc -std=c11 -g -fsanitize=address,undefined -DVIEWWINDOWWIDTH=120 -DVIEWWINDOWHEIGHT=114 -DFLAT_SPAN tests/test_video.c src/pebble/i_pebblev.c -o work/test_video
./work/test_video
gcc -std=c11 -g -fsanitize=address,undefined tests/test_ascii.c src/pebble/ascii.c -o work/test_ascii
./work/test_ascii
```

`sh tools/check_all.sh` builds the resource file and the app and runs all of the above plus the `tests/test_info.c` table check; add `--emulator` to also visit every map in the emulator.

Create `work/` before running native tests. `tools/check_maps.py` builds the playtest variant (`PDOOM_PLAYTEST=1 pebble build`, output in `build-playtest/`, never `build/`), starts a new game and, for each map, waits for it to load, looks around (tilting the simulated watch), walks, screenshots (`work/map<N>-*.png`), then warps to the exit and continues; it fails on engine errors, a missing map, or low zone memory. The emulator presses one button at a time, so the playtest variant exits the level when Down is held for 2 seconds; it also makes the player invulnerable, immune to knockback, and able to walk through monsters. None of this is compiled into the release build. `tools/smoke_map.py` is a quicker single-map look, and `tools/check_menus.py` captures menu and settings-persistence screenshots.

The project is a git repository; on this machine git is available inside WSL, not on Windows.

## Releasing

See `store/LISTING.md` for the store text, name options and checklist. `python3 tools/package_release.py` puts the .pbw, the complete source zip (`git archive` of HEAD, as the GPL requires), store screenshots and SHA-256 sums in `dist/<version>/`.

## Attribution

Engine source is GPL-2.0; see LICENSE and original source headers. Upstream repositories:
- https://github.com/FrenkelS/Doom64KB (local reference commit 058e4d4a75da7187fff3c6f357647d9f069605f7)
- https://github.com/akiyan/genesis-DOOM64KB (local reference commit 5da4cf9d6b46e78078d208f46e52ee40c36c67b4)

Freedoom artwork is distributed under its BSD license; see licenses/Freedoom-COPYING.txt and licenses/Freedoom-CREDITS.txt. Source: https://github.com/freedoom/freedoom/releases/tag/v0.13.0 . Converted sprites retain logical dimensions with reduced color/detail. Wall textures are rescaled from the Freedoom textures listed in `tools/assets.py`. The original level geometry (`tools/levels/`) and the procedural fallback artwork used when the Freedoom art pack is absent are dedicated to CC0. The source archive includes the modified engine, port, asset generators, converted art, build configuration, and tests.

## Hardware startup fix (2026-09-27)

The initial emulator-tested build crashed on physical hardware in newlib strcasecmp, while accessing its character-classification table. The port now supplies an ASCII-only implementation for command-line option matching. Native sanitizer tests cover mixed case, prefixes, empty strings, and unsigned character handling. Installation and active game ticks were verified on the physical watch.

## Damage hitch investigation

The current diagnostic build reports maximum drawing time, game-tick time, timer gap (all milliseconds), damage-event count, and clock skips every 105 game ticks. A hardware capture with seven damage events showed game ticks <=4 ms and timer gaps <=40 ms in the damage intervals; an earlier interval had a 101 ms gap. The reported one-second hitch has not yet been reproduced on hardware and is not considered fixed. Timing counters reset after each report.

In the Emery emulator, `time_ms()` jumps by about one second around each second boundary (about two `skips` per report). Earlier builds simulated each jump as a 120 ms burst of game ticks, so the emulated game ran roughly 20% fast in one-second pulses. The frame timer now counts an implausible jump (backward, or more than 500 ms) as one nominal frame. Whether hardware shows the same clock behaviour is unverified; check the `skips` counter in a hardware log.

## Release preparation milestone

Added a launcher icon, title/pause/settings/controls screens, persisted touch preferences, and focus-loss pausing. Removed unused legacy sound mixing work from the silent Pebble build; future speaker effects will use the Pebble sound adapter. Renderer draw-segment capacity is 64 with its existing overflow guard, adequate for this 16-line map; re-test visibility when adding larger maps. Moved the clipping buffer to checked heap allocation. The three-level campaign, checkpoint saves, audio, and store listing remain future milestones.
