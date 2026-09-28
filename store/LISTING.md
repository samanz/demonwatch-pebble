# Store listing draft

## Name

**Demonwatch** (launcher and store). Chosen instead of the development name
"pDOOM" because "DOOM" is a trademark of id Software / ZeniMax. Before
submitting, search the store and a trademark database for existing uses.

It is fine to say in the description that the game runs a port of the
Doom64KB engine and uses Freedoom art (that is attribution, not branding).

## Short description (one line)

A fast first-person shooter for your wrist: three levels of demons, doors, keys and secrets.

## Description

Blast through a three-level episode on your Pebble Time 2. Fight zombies,
shotgunners, imps and demons through tech bases, toxic refineries and an
outdoor command centre. Find keycards, ride lifts, and hunt for a hidden
secret in every level.

- Three hand-built levels, each with a secret; Easy, Normal and Hard
- Tap the screen to open doors or fire; tilt or drag to turn
- Automap, checkpoints and Continue, so a notification never costs you a run
- Freedoom sound effects through the watch speaker (can be switched off)
- Runs at ~27 fps on the watch; no phone needed while playing

Controls: Up/Down move, Select fires (hold and press Up/Down to turn), tilt
steers, tap the screen to use or fire, touch-and-hold changes weapon, Back
opens doors, double Back pauses.

Credits: engine is a port of Doom64KB (GPL-2.0, id Software and the Doom64KB
authors; source code: https://github.com/samanz/demonwatch-pebble). Art and sounds from Freedoom
(BSD licence). Levels are original (CC0).

## Store fields

- Category: Games
- Platform: Emery (Pebble Time 2)
- Screenshots: `dist/<version>/screenshots/` (200 x 228), made by
  `tools/package_release.py` from the emulator run
- Icons: `store/icon-48.png` and `store/icon-144.png` (store), `resources/menu_icon.png` (25 x 25 launcher); all drawn by `tools/build_icon.py`. The launcher shows the icon in greyscale, so the face uses bright red to stay light against its outline.

## Before submitting

1. Play all three levels on the watch on Easy, Normal and Hard.
2. `sh tools/check_all.sh --emulator`, commit, then
   `python3 tools/package_release.py`.
3. Push the release commit to https://github.com/samanz/demonwatch-pebble (the
   public source the GPL requires) and link it from the listing.
