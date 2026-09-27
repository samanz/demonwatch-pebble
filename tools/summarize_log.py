"""Summarize an app log captured with `pebble logs` (emulator or watch):
game-state changes, checkpoints, errors, per-level duration, and the worst
frame/tick/gap timings, frame rate and memory figures.

Usage: python3 tools/summarize_log.py work/watch-playtest.log
"""
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(errors='replace').replace('\0', '')
lines = text.splitlines()


def seconds(line):
    m = re.match(r'\[(\d+):(\d+):(\d+)\]', line)
    return int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) if m else None


print('Events:')
level_start = {}
for line in lines:
    if re.search(r'state |checkpoint|new game|title:|FATAL|fault|PLAYTEST', line) and 'pkjs' not in line:
        print('  ' + line.strip())
    m = re.search(r'state (\d) map (\d)', line)
    t = seconds(line)
    if m and t is not None:
        state, level = m.groups()
        if state == '0':
            level_start[level] = t
        elif state == '1' and level in level_start:
            d = t - level_start.pop(level)
            print(f'    -> map {level} took {d // 60}:{d % 60:02d}')

ticks = [l for l in lines if 'tick ' in l and ' pos ' in l]
timing = [l for l in lines if 'timing draw' in l]
if not ticks:
    sys.exit('No gameplay reports in the log.')


def values(key, source):
    return [int(v) for l in source for v in re.findall(key + r'(-?\d+)', l)]


frames = values('frames', ticks)
print(f'\nReports: {len(ticks)} (every 105 tics = 3 s)')
print(f'Frame rate: median {sorted(frames)[len(frames) // 2] / 3:.1f} fps, lowest {min(frames) / 3:.1f} fps')
for key in ('draw', 'tick', 'gap'):
    v = values(key, timing)
    print(f'Worst {key}: {max(v)} ms (median of per-report maxima {sorted(v)[len(v) // 2]} ms)')
print(f'Clock skips: {sum(values("skips", timing))}   hits taken: {sum(values("hits", timing))}')
print(f'Lowest free heap: {min(values("heap", ticks))} B   lowest free zone: {min(values("zone", ticks))} B')
last = ticks[-1]
print('Last report: ' + last.strip())
slow = [l.strip() for l in timing if max(values('gap', [l])) > 150]
if slow:
    print(f'\nReports with a frame gap over 150 ms ({len(slow)}):')
    for l in slow[:15]:
        print('  ' + l)
