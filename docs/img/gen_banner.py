#!/usr/bin/env python3
"""Generates the ATC masthead: docs/img/banner-{light,dark}.svg

Corporate identity, not decoration. A flat colour field, one geometric mark,
a tight grotesque wordmark. Every value that matters is a constant below, so
the plate is reproducible rather than hand-edited SVG.

    python3 docs/img/gen_banner.py        # run from the repo root
"""

W, H = 1200, 320

NAVY  = "#0F2038"
PAPER = "#F4F2ED"
RED   = "#C8102E"

GROTESQUE = ("Inter,&apos;Helvetica Neue&apos;,Helvetica,"
             "&apos;Segoe UI&apos;,Arial,sans-serif")

# --- mark geometry, in its own 164x164 box -------------------------------
MARK   = 132
MARK_X, MARK_Y = 275, (H - MARK) // 2   # 275 centres the whole lockup
BAR_Y, BAR_H = 90, 14            # the red crossbar, within the mark box
APEX, INSET, BASE = 18, 18, 112  # outer triangle: apex y, side inset, base y
CTR_APEX, CTR_INSET, CTR_BASE = 52, 48, 88   # the counter that makes it a letter


def mark(x, y, field, figure):
    """A solid square holding a triangular A cut by a crossbar that bleeds
    off the right edge — the T. Flat shapes only, no strokes, no gradients."""
    g = [f'<g transform="translate({x},{y})">']
    g.append(f'<rect width="{MARK}" height="{MARK}" fill="{field}"/>')
    g.append(f'<path d="M{MARK/2},{APEX} L{MARK-INSET},{BASE} L{INSET},{BASE} Z" fill="{figure}"/>')
    g.append(f'<path d="M{MARK/2},{CTR_APEX} L{MARK-CTR_INSET},{CTR_BASE} L{CTR_INSET},{CTR_BASE} Z" fill="{field}"/>')
    g.append(f'<rect x="{INSET+4}" y="{BAR_Y}" width="{MARK-INSET-4}" height="{BAR_H}" fill="{RED}"/>')
    g.append('</g>')
    return "".join(g)


def plate(bg, fg, mark_field, mark_figure, out):
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" '
         f'width="{W}" height="{H}" role="img" aria-label="AT Capital">']
    s.append(f'<rect width="{W}" height="{H}" fill="{bg}"/>')
    s.append(mark(MARK_X, MARK_Y, mark_field, mark_figure))

    tx = MARK_X + MARK + 48
    s.append(f'<text x="{tx}" y="169" font-family="{GROTESQUE}" font-size="76" '
             f'font-weight="700" letter-spacing="-1" fill="{fg}">AT CAPITAL</text>')
    s.append(f'<text x="{tx+3}" y="207" font-family="{GROTESQUE}" font-size="17" '
             f'font-weight="500" letter-spacing="6.4" fill="{fg}" opacity="0.68">'
             f'FINANCIAL SIMULATION ENGINE</text>')

    s.append('</svg>')
    open(out, "w").write("\n".join(s) + "\n")
    print("wrote", out)


plate(PAPER, NAVY,  NAVY,  PAPER, "docs/img/banner-light.svg")
plate(NAVY,  PAPER, PAPER, NAVY,  "docs/img/banner-dark.svg")
