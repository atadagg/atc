#!/usr/bin/env python3
"""Reports what a generated identity sheet actually contains.

Stdlib only, like validate.py: this runs with no environment at all.

    ./build/atc --identity-sheet output/marks.svg --identity-count 1000
    python3 scripts/identity_stats.py output/marks.svg

A generator is judged on two numbers at once. "A million distinct marks" is
satisfied by one that also emits a single mark 40% of the time, so the
frequency table matters as much as the distinct count.

The sheet carries no text -- every glyph is an outline -- so names are read
back by inverting src/identity_glyphs.hpp, which maps codepoint to path. That
inversion is also a check on the renderer: a path in the sheet that no font
claims means text was drawn from something other than the vendored glyphs.
"""
import collections
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
GLYPH_HEADER = os.path.join(HERE, os.pardir, "src", "identity_glyphs.hpp")


def glyph_index(path=GLYPH_HEADER):
    """path-data -> codepoint, over every vendored font."""
    with open(path, encoding="utf-8") as f:
        src = f.read()
    index = {}
    for cp, _adv, d in re.findall(r'\{(\d+)u, ([\d.]+)f, "([^"]*)"\}', src):
        if d:
            index.setdefault(d, chr(int(cp)))
    return index


def words(svg, index):
    """Each <g fill=...> holding glyph paths is one rendered line of text."""
    out = []
    for group in re.findall(r'<g fill="#[0-9A-F]{6}">(.*?)</g>', svg):
        paths = re.findall(r'<path transform="[^"]*" d="([^"]*)"/>', group)
        if not paths:
            continue
        line = "".join(index.get(d, "�") for d in paths)
        if line:
            out.append(line)
    return out


def report(svg, index):
    texts = svg.count("<text")
    clips = re.findall(r'<clipPath id="([^"]+)"', svg)
    fills = re.findall(r'fill="(#[0-9A-F]{6})"', svg)
    lines = words(svg, index)
    unknown = sum(line.count("�") for line in lines)

    print(f"paths           {svg.count('<path')}")
    print(f"<text> elements {texts}   (must be 0 -- a font resolved at render "
          f"time is a different identity per machine)")
    print(f"clip paths      {len(clips)}  distinct {len(set(clips))}"
          f"   {'OK' if len(clips) == len(set(clips)) else 'COLLISION'}")
    print(f"distinct fills  {len(set(fills))}")
    print(f"glyphs unmatched by any vendored font: {unknown}")
    print()

    # The head is the first line drawn in each cell: the mark's own name. The
    # tail (sector, legal form) is drawn in the palette colour after it.
    freq = collections.Counter(lines)
    print(f"text lines      {len(lines)}  distinct {len(freq)}")
    print("most frequent lines:")
    for line, n in freq.most_common(10):
        print(f"  {n:5d}  {n / len(lines):6.2%}  {line}")
    print()
    print("most used palette fills:")
    for colour, n in collections.Counter(fills).most_common(8):
        print(f"  {colour}  {n:6d}  {n / len(fills):6.1%}")

    ok = texts == 0 and len(clips) == len(set(clips)) and unknown == 0
    return 0 if ok else 1


def main(argv):
    if len(argv) != 2:
        print(f"usage: {argv[0]} <sheet.svg>", file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8") as f:
        return report(f.read(), glyph_index())


if __name__ == "__main__":
    sys.exit(main(sys.argv))
