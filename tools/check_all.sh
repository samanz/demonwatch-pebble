#!/bin/sh
# Build the app and run every automated check. Run from the project root in
# WSL/Linux. Pass --emulator to also run the emulator playthrough.
set -e
cd "$(dirname "$0")/.."
mkdir -p work
SAN="-std=c11 -g -fsanitize=address,undefined"
VIEW="-DVIEWWINDOWWIDTH=120 -DVIEWWINDOWHEIGHT=114 -DFLAT_SPAN"

python3 tools/run_pebble.py build > work/build.log 2>&1 || { tail -30 work/build.log; exit 1; }
grep -E 'Total size|footprint|Free RAM' work/build.log || true  # absent when up to date
python3 tools/verify_pbw.py build/pdoom.pbw

python3 -m unittest discover -s tests
gcc $SAN -Itests tests/test_wad.c src/pebble/pebble_wad.c -o work/test_wad
ASAN_OPTIONS=detect_leaks=0 ./work/test_wad resources/arena.pbl
gcc $SAN $VIEW tests/test_video.c src/pebble/i_pebblev.c -o work/test_video
./work/test_video
gcc $SAN tests/test_ascii.c src/pebble/ascii.c -o work/test_ascii
./work/test_ascii && echo "PASS: ascii strcasecmp"
gcc $SAN -fshort-enums -DPEBBLE_EMERY $VIEW -Isrc/doom -Itests tests/test_info.c \
    -Wl,--unresolved-symbols=ignore-all -o work/test_info
./work/test_info

if [ "$1" = "--emulator" ]; then
    python3 tools/check_playthrough.py > work/playthrough-out.txt 2>&1 || {
        tr -d '\000' < work/playthrough-out.txt | grep -v pkjs | tail -20; exit 1; }
    tr -d '\000' < work/playthrough-out.txt | grep -E 'state |PASS'
fi
echo "ALL CHECKS PASSED"
