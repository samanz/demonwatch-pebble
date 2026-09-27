# Store listing draft

## Name

"DOOM" is a trademark of id Software / ZeniMax, and "pDOOM" puts it front and
centre, so a store may reject or later remove the listing. Pick a name that
does not contain "Doom"; the in-app name lives in `package.json`
(`displayName`, `shortName`) and the title text in `src/pebble/i_pebble.c`
(`titles[]`). Options:

- **Demonwatch** – short, says what it is, nothing to confuse with id's games.
- **Wrist Inferno** – descriptive, a little playful.
- **Gatecrash** – ties in with the first level (Hangar Gate).

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
authors; source code is provided with the app). Art and sounds from Freedoom
(BSD licence). Levels are original (CC0).

## Store fields

- Category: Games
- Platform: Emery (Pebble Time 2)
- Screenshots: `dist/<version>/screenshots/` (200 x 228), made by
  `tools/package_release.py` from the emulator run
- Icon: `resources/menu_icon.png`

## Before submitting

1. Choose the name and update `package.json` and `titles[]`.
2. Play all three levels on the watch on Easy, Normal and Hard.
3. `sh tools/check_all.sh --emulator`, commit, then
   `python3 tools/package_release.py`.
4. Publish the source zip (or the git repository) somewhere public and link
   it from the listing: the GPL requires the source to be available.
