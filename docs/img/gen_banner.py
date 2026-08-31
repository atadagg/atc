#!/usr/bin/env python3
"""Generates the ATC masthead: docs/img/banner-{light,dark}.svg

The reference is East Asian bank identity — ICBC, Bank of China, SMBC,
Bank of the Ryukyus — read structurally rather than copied:

  * the mark is an *emblem*, not a letterform. A coin: solid disc, square
    aperture, knocked out in the field colour. Symmetric, four shapes, and
    still legible at 16px in a repo sidebar.
  * one saturated ink. Not a two-colour split; that reads Western-corporate.
  * the lockup is hierarchical and divided by a hairline vertical rule —
    the device that sits between mark and wordmark on almost every JP/CN
    bank plate.

Vaporwave supplies only what the banks don't: the gradient field and a faint
grid. It stays in the background on purpose. The bank grammar in front of it
is what keeps the plate from reading as a meme.

Type is DIN — the institutional grotesque, and the closest thing to the
tracked Latin secondary lines on those lockups.

**Text is outlined to paths.** GitHub renders README SVGs with whatever fonts
it happens to have, so live <text> meant the tracking and the centring were
never what we tuned locally. Outlining also lets us *measure* the wordmark,
which is what actually centres the lockup — the previous plate used a
hand-tuned magic offset and was visibly off-centre.

    .venv/bin/python docs/img/gen_banner.py     # run from the repo root

fontTools is needed to regenerate. The emitted SVGs have no dependency.
"""

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen

W, H = 1200, 320

# --- ink ------------------------------------------------------------------
# Three colours, and no more. Navy is the ground, paperwhite is the figure,
# red is the single accent and does exactly one job (the divider rule).
# Paperwhite, deliberately cool: a bone/beige white drags the plate towards
# stationery, and these banks print on bright stock.
NAVY  = "#0F2038"
RED   = "#C8102E"
PAPER = "#FFFFFF"
NAVY_DEEP = "#0A1728"   # gradient partner, not a fourth colour
PALE      = "#EBEFF5"   # ditto, on the light plate

FONT_DIR = "/usr/share/fonts/dinish"
WORDMARK_FONT = f"{FONT_DIR}/DINish-Bold.ttf"
SUB_FONT      = f"{FONT_DIR}/DINish-Medium.ttf"

# --- emblem geometry, in its own 160x160 box ------------------------------
# Two shapes. A disc and a square hole — the coin, and nothing else. Every
# addition tried on top of this (a T stem, a crossbar, a second ring) made it
# read as a keyhole or a machine part instead of a coin, and none survived
# being shrunk to a 16px favicon. The letters live in the wordmark, which is
# where letters belong; an emblem that tries to spell is the bug this file
# was rewritten to fix.
EMBLEM   = 160
C        = EMBLEM / 2
DISC_R   = 76
APERTURE = 54            # the square hole

GAP      = 46            # emblem -> rule -> wordmark
RULE_W   = 2


# --- type -----------------------------------------------------------------
def outline(text, font_path, size, tracking):
    """Return (path_d, advance_width) for `text` set at `size` with `tracking`
    units of extra space per pair, as a single SVG path at the origin with the
    baseline at y=0. Kerning is not applied: these are short, widely tracked
    lines where the pair adjustments would be swamped by the tracking anyway."""
    font = TTFont(font_path)
    upem = font["head"].unitsPerEm
    scale = size / upem
    cmap = font.getBestCmap()
    glyphs = font.getGlyphSet()
    hmtx = font["hmtx"]

    parts, x = [], 0.0
    for ch in text:
        name = cmap.get(ord(ch))
        if name is None:
            raise KeyError(f"{font_path} has no glyph for {ch!r}")
        pen = SVGPathPen(glyphs, ntos=lambda v: f"{v:.2f}")
        glyphs[name].draw(pen)
        d = pen.getCommands()
        if d:  # a space contributes advance and no contour
            parts.append(f'<path transform="translate({x:.2f},0) '
                         f'scale({scale:.6f},{-scale:.6f})" d="{d}"/>')
        x += hmtx[name][0] * scale + tracking
    return "".join(parts), max(0.0, x - tracking)


def emblem(x, y, ink, field):
    """The coin: a solid disc with a square aperture knocked out of it.
    No strokes, no gradients, two shapes, one colour."""
    a = APERTURE / 2
    return (f'<g transform="translate({x},{y})">'
            f'<circle cx="{C}" cy="{C}" r="{DISC_R}" fill="{ink}"/>'
            f'<rect x="{C-a}" y="{C-a}" width="{APERTURE}" height="{APERTURE}" '
            f'fill="{field}"/>'
            f'</g>')


def plate(out, ground, grad_from, grad_to, ink, field, fg, rule_op, grid_op):
    word_d, word_w = outline("AT CAPITAL", WORDMARK_FONT, 68, 3.0)
    sub_d,  sub_w  = outline("FINANCIAL SIMULATION ENGINE", SUB_FONT, 15, 7.4)
    text_w = max(word_w, sub_w)

    total = EMBLEM + GAP + RULE_W + GAP + text_w
    ex = (W - total) / 2          # measured, not hand-tuned
    ey = (H - EMBLEM) / 2
    rx = ex + EMBLEM + GAP
    tx = rx + RULE_W + GAP

    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" '
         f'width="{W}" height="{H}" role="img" '
         f'aria-label="AT Capital — Financial Simulation Engine">']

    s.append('<defs>')
    s.append(f'<linearGradient id="field" x1="0" y1="0" x2="1" y2="1">'
             f'<stop offset="0" stop-color="{grad_from}"/>'
             f'<stop offset="1" stop-color="{grad_to}"/></linearGradient>')
    s.append(f'<pattern id="grid" width="40" height="40" '
             f'patternUnits="userSpaceOnUse">'
             f'<path d="M40 0H0V40" fill="none" stroke="{fg}" '
             f'stroke-width="1" opacity="{grid_op}"/></pattern>')
    s.append('</defs>')

    s.append(f'<rect width="{W}" height="{H}" fill="{ground}"/>')
    s.append(f'<rect width="{W}" height="{H}" fill="url(#field)" opacity="0.9"/>')
    s.append(f'<rect width="{W}" height="{H}" fill="url(#grid)"/>')

    s.append(emblem(ex, ey, ink, field))

    # the hairline rule: the JP/CN bank lockup's mark-to-wordmark divider
    s.append(f'<rect x="{rx:.1f}" y="{ey+16}" width="{RULE_W}" '
             f'height="{EMBLEM-32}" fill="{RED}"/>')

    s.append(f'<g transform="translate({tx:.1f},162)" fill="{fg}">{word_d}</g>')
    s.append(f'<rect x="{tx:.1f}" y="181" width="{text_w:.1f}" height="1.5" '
             f'fill="{fg}" opacity="{rule_op}"/>')
    s.append(f'<g transform="translate({tx:.1f},207)" fill="{fg}" '
             f'opacity="0.72">{sub_d}</g>')

    s.append('</svg>')
    open(out, "w").write("\n".join(s) + "\n")
    print(f"wrote {out}  (lockup {total:.0f}px, margins {ex:.0f})")


plate("docs/img/banner-dark.svg",
      ground=NAVY, grad_from=NAVY, grad_to=NAVY_DEEP,
      ink=PAPER, field=NAVY, fg=PAPER, rule_op=0.35, grid_op=0.10)

plate("docs/img/banner-light.svg",
      ground=PAPER, grad_from=PAPER, grad_to=PALE,
      ink=NAVY, field=PAPER, fg=NAVY, rule_op=0.30, grid_op=0.20)
