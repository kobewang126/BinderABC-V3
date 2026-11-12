#!/usr/bin/env python3
"""Check local Markdown links and chapter-source excerpts without building the site."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

def excerpts(path):
    text = path.read_text()
    pattern = r'源码：`(examples/[^`]+)`([^\n]*)\n\n(```|~~~)c\n([\s\S]*?)\n\3'
    for match in re.finditer(pattern, text):
        source = ROOT / match[1]
        yield match, source

def locate_excerpt(match, source):
    lines = source.read_text().splitlines()
    chunks, chunk = [], []
    for line in match[4].splitlines():
        if '...' in line or '省略' in line:
            if chunk: chunks.append(chunk); chunk = []
        else:
            chunk.append(line.strip())
    if chunk: chunks.append(chunk)
    found = []
    start = 0
    old = re.search(r'第\s*(\d+)', match[2])
    original = int(old[1]) - 1 if old else 0
    for chunk in chunks:
        while chunk and not chunk[0]: chunk.pop(0)
        while chunk and not chunk[-1]: chunk.pop()
        if not chunk: continue
        choices = [i for i in range(start, len(lines) - len(chunk) + 1)
                   if [x.strip() for x in lines[i:i + len(chunk)]] == chunk]
        if not choices: return None
        index = min(choices, key=lambda i: abs(i - original)) if not found else choices[0]
        found.extend(range(index + 1, index + len(chunk) + 1))
        start = index + len(chunk)
    return found

def main():
    errors, checked = [], 0
    docs = [ROOT / 'README.md', *sorted((ROOT / 'docs').glob('*/*.md'))]
    for path in docs:
        for target in re.findall(r'(?<!!)\[[^\]]*\]\(([^)]+)\)', path.read_text()):
            target = target.split('#', 1)[0]
            if not target or re.match(r'[a-zA-Z]+:', target): continue
            if not (path.parent / target).exists():
                errors.append(f'{path.relative_to(ROOT)}: missing link {target}')
        for match, source in excerpts(path):
            checked += 1
            if not source.exists():
                errors.append(f'{path.relative_to(ROOT)}: missing source {match[1]}'); continue
            found = locate_excerpt(match, source)
            if not found:
                errors.append(f'{path.relative_to(ROOT)}: excerpt differs from {match[1]}'); continue
            numbers = re.search(r'第\s*([\d\s–—、,\-至]+)\s*行', match[2])
            if not numbers:
                errors.append(f'{path.relative_to(ROOT)}: missing line numbers {match[1]}'); continue
            ranges = []
            for item in re.split(r'[、,]', numbers[1]):
                nums = [int(n) for n in re.findall(r'\d+', item)]
                if nums: ranges.append((nums[0], nums[-1]))
            if any(not any(a <= n <= b for a, b in ranges) for n in found):
                errors.append(f'{path.relative_to(ROOT)}: stale line numbers {match[1]} (actual {min(found)}–{max(found)})')
    print(f'Checked {checked} chapter-source excerpts and local links; {len(errors)} errors')
    for error in errors: print(error)
    return bool(errors)

if __name__ == '__main__': sys.exit(main())
