# pDOOM 0.2 — release preparation

Native Doom64KB-derived game for Pebble Time 2 (Emery), built with Pebble SDK 4.33.1. This alpha contains one original three-room arena, two working doors, four enemies, armor, shotgun, ammunition, and an exit switch. Assets are adapted from Freedoom 0.13.0. No commercial Doom WAD is required or bundled.

## Install and play

Install `pdoom-alpha.pbw` using a Pebble app installation workflow supporting Emery. The development build is `build/pdoom.pbw`.

- Up / Down: move forward / backward.
- Hold Select: fire. While held, Up / Down turn left / right.
- Back once: use door or exit switch.
- Hold Select and click Back: cycle owned weapons.
- Tilt the watch left / right: turn (default) or strafe, chosen in Settings.
- Double Back: open the pause menu. Up/Down choose Resume, Restart, Settings, Controls, or Quit; Select confirms. Back resumes.
- Launch opens a title menu: New game, Settings, Controls.
- Settings save touch/tilt sensitivity (gentle/normal/fast), turn direction, tilt mode (off/steer/strafe), speaker music, and haptics.
- Losing app focus clears held inputs and pauses gameplay.
- Hold Back: system exit.
- Touch drag horizontally: turn; double tap: cycle weapons. Touch requires compatible firmware and remains unverified on a physical watch.
- After death: Select retries the current level.
- After the exit switch: Select continues to the next map (E1M2, E1M3, … when present, keeping inventory). After the last map, Select starts a new game.

Walk toward the door, press Back, fight through both combat rooms, then use the switch on the far wall. The door closes automatically and can be reopened.

## Verified scope and limits

The Emery emulator was used to exercise movement, pickup, door use, killing enemies in both combat rooms, exiting, restarting, and pause/restart. Native AddressSanitizer/UndefinedBehaviorSanitizer checks cover resource storage and framebuffer drawing. Four asset tests check map records, sprite frames, and texture references.

The renderer uses 120 x 114 logical pixels scaled to a 200 x 190 view, plus a health/ammo/armor HUD. Engine simulation runs at 35 ticks per second. Physical Emery hardware running PebbleOS 4.38.2 has now launched successfully and logged movement. Extended gameplay, battery use, and touch feel still need hardware testing. There is no sound, save system, full E1M1, or arbitrary WAD support. This is a playable prototype, not an App Store release.

SDK measurements: resources 145,013 bytes; static RAM footprint 58,317 bytes; initial available heap 72,755 bytes. Runtime free heap in this arena is approximately 16 KB. The loaded image (58,320 bytes) must fit the SDK header's 65,535-byte field, leaving about 7.2 KB of code/static-data headroom.

Space-saving conventions (keep them when adding content):
- `states[]` stores each action as an 8-bit index into `actionfuncs[]` and is packed to 8 bytes per state. Its fields must stay in the same order as the positional initializers in `info.c`; `tests/test_info.c` checks every index is in range.
- `sprnames` is an inline `char[][5]` table; function-pointer and string-pointer tables cost extra because position-independent code relocates them into RAM.
- Demo playback, SRAM save/load, the title demo loop, and engine `printf` progress messages are compiled out under `PEBBLE_EMERY`.
- Scratch tables needed only at startup (such as the sprite-frame table) are heap-allocated and freed.

Map lumps (THINGS through BLOCKMAP after each `E1Mx` marker) are released on every level load, so additional maps do not accumulate in the 24 KB Doom zone. Global lumps (textures, colormap) stay resident; sprite and wall patches stream through one scratch buffer.

## Build

Use Linux/WSL with Pebble SDK 4.33.1 and its ARM toolchain available on PATH:

```sh
python3 tools/build_arena.py
pebble build
python3 tools/verify_pbw.py build/pdoom.pbw
pebble install --emulator emery
```

`tools/run_pebble.py` is a convenience wrapper for the original development machine's SDK paths; adapt it for another machine or use `pebble` directly. The checked-in Freedoom art pack makes arena generation independent of downloading the original release.

To regenerate the art pack, place the official Freedoom 0.13.0 zip at `work/freedoom-0.13.0.zip`, run `python3 tools/import_freedoom.py work/freedoom-0.13.0.zip`, then regenerate the arena. Archive SHA256: `3f9b264f3e3ce503b4fb7f6bdcb1f419d93c7b546f4df3e874dd878db9688f59`.

```sh
python3 -m unittest discover -s tests -v
gcc -std=c11 -g -fsanitize=address,undefined -Itests tests/test_wad.c src/pebble/pebble_wad.c -o work/test_wad
ASAN_OPTIONS=detect_leaks=0 ./work/test_wad resources/arena.pbl
gcc -std=c11 -g -fsanitize=address,undefined -DVIEWWINDOWWIDTH=120 -DVIEWWINDOWHEIGHT=114 -DFLAT_SPAN tests/test_video.c src/pebble/i_pebblev.c -o work/test_video
./work/test_video
gcc -std=c11 -g -fsanitize=address,undefined tests/test_ascii.c src/pebble/ascii.c -o work/test_ascii
./work/test_ascii
```

`sh tools/check_all.sh` builds the app and runs all of the above plus the `tests/test_info.c` table check; add `--emulator` to include the playthrough.

Create `work/` before running native tests. `tools/check_playthrough.py` builds the playtest variant (`PDOOM_PLAYTEST=1 pebble build`, output in `build-playtest/`, never `build/`), levels the emulator accelerometer, then steers the app from spawn to the exit switch using the logged player position (walking, reopening doors and fighting when blocked). It saves screenshots and logs in `work/` and checks for the level-complete state change (`state 1 map 1`). The playtest variant makes the player invulnerable, immune to knockback, and able to walk through monsters, because the bot cannot aim; walls, doors, switches, pickups, and combat are otherwise unchanged. None of this is compiled into the release build. `tools/check_menus.py` captures menu and settings-persistence screenshots.

The project is a git repository; on this machine git is available inside WSL, not on Windows.

## Attribution

Engine source is GPL-2.0; see LICENSE and original source headers. Upstream repositories:
- https://github.com/FrenkelS/Doom64KB (local reference commit 058e4d4a75da7187fff3c6f357647d9f069605f7)
- https://github.com/akiyan/genesis-DOOM64KB (local reference commit 5da4cf9d6b46e78078d208f46e52ee40c36c67b4)

Freedoom artwork is distributed under its BSD license; see licenses/Freedoom-COPYING.txt and licenses/Freedoom-CREDITS.txt. Source: https://github.com/freedoom/freedoom/releases/tag/v0.13.0 . Converted sprites retain logical dimensions with reduced color/detail. Wall, door, and exit-panel textures are rescaled from Freedoom STARTAN3, BIGDOOR2, and SW1EXIT. Original arena geometry and the procedural fallback artwork used when the Freedoom art pack is absent are dedicated to CC0. The source archive includes the modified engine, port, asset generators, converted art, build configuration, and tests.

## Hardware startup fix (2026-09-27)

The initial emulator-tested build crashed on physical hardware in newlib strcasecmp, while accessing its character-classification table. The port now supplies an ASCII-only implementation for command-line option matching. Native sanitizer tests cover mixed case, prefixes, empty strings, and unsigned character handling. Installation and active game ticks were verified on the physical watch.

## Damage hitch investigation

The current diagnostic build reports maximum drawing time, game-tick time, timer gap (all milliseconds), damage-event count, and clock skips every 105 game ticks. A hardware capture with seven damage events showed game ticks <=4 ms and timer gaps <=40 ms in the damage intervals; an earlier interval had a 101 ms gap. The reported one-second hitch has not yet been reproduced on hardware and is not considered fixed. Timing counters reset after each report.

In the Emery emulator, `time_ms()` jumps by about one second around each second boundary (about two `skips` per report). Earlier builds simulated each jump as a 120 ms burst of game ticks, so the emulated game ran roughly 20% fast in one-second pulses. The frame timer now counts an implausible jump (backward, or more than 500 ms) as one nominal frame. Whether hardware shows the same clock behaviour is unverified; check the `skips` counter in a hardware log.

## Release preparation milestone

Added a launcher icon, title/pause/settings/controls screens, persisted touch preferences, and focus-loss pausing. Removed unused legacy sound mixing work from the silent Pebble build; future speaker effects will use the Pebble sound adapter. Renderer draw-segment capacity is 64 with its existing overflow guard, adequate for this 16-line map; re-test visibility when adding larger maps. Moved the clipping buffer to checked heap allocation. The three-level campaign, checkpoint saves, audio, and store listing remain future milestones.
