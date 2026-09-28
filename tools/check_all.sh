#!/bin/sh
# Build the resource file and the app, then run every automated check. Run
# from the project root in WSL/Linux. Pass --emulator to also visit every map
# in the emulator (tools/check_maps.py).
set -e
cd "$(dirname "$0")/.."
mkdir -p work
SAN="-std=c11 -g -fsanitize=address,undefined"
VIEW="-DVIEWWINDOWWIDTH=120 -DVIEWWINDOWHEIGHT=114 -DFLAT_SPAN"

python3 tools/build_wad.py
python3 tools/run_pebble.py build > work/build.log 2>&1 || { tail -30 work/build.log; exit 1; }
grep -E 'Total size|footprint|Free RAM' work/build.log || true  # absent when up to date
python3 tools/verify_pbw.py build/pdoom.pbw

python3 -m unittest discover -s tests
gcc $SAN -Itests tests/test_wad.c src/pebble/pebble_wad.c -o work/test_wad
ASAN_OPTIONS=detect_leaks=0 ./work/test_wad resources/pdoom.pbl
gcc $SAN $VIEW tests/test_video.c src/pebble/i_pebblev.c -o work/test_video
./work/test_video
gcc $SAN tests/test_ascii.c src/pebble/ascii.c -o work/test_ascii
./work/test_ascii && echo "PASS: ascii strcasecmp"
gcc $SAN -fshort-enums -DPEBBLE_EMERY $VIEW -Isrc/doom -Itests tests/test_info.c \
    -Wl,--unresolved-symbols=ignore-all -o work/test_info
./work/test_info

if [ "$1" = "--emulator" ]; then
    python3 tools/check_maps.py > work/check-maps-out.txt 2>&1 || {
        tr -d '\000' < work/check-maps-out.txt | grep -v pkjs | tail -20; exit 1; }
    tr -d '\000' < work/check-maps-out.txt | grep -E 'state |zone free|PASS'
    python3 tools/check_tap.py > work/check-tap-out.txt 2>&1 || {
        tail -20 work/check-tap-out.txt; exit 1; }
    tail -1 work/check-tap-out.txt
    python3 tools/check_boss.py > work/check-boss-out.txt 2>&1 || {
        tail -20 work/check-boss-out.txt; exit 1; }
    tail -1 work/check-boss-out.txt
    # Round 2: play the release build a little on the round-screen emulator.
    EMULATOR=gabbro python3 tools/smoke_map.py build/pdoom.pbw round > work/smoke-round.txt 2>&1
    grep -q 'no engine errors' work/smoke-round.txt || { tail -20 work/smoke-round.txt; exit 1; }
    echo "PASS: Round 2 (gabbro) smoke run, no engine errors"
fi
echo "ALL CHECKS PASSED"
