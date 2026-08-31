#include "identity_svg.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace identity {

namespace {

double fit_size(const char* text, uint8_t font, double want, double max_w,
                double tracking_ratio);

std::string num(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.3f", v);
    std::string s(buf);
    // Trim trailing zeros: an SVG full of "50.000" is three times the bytes.
    const auto dot = s.find('.');
    if (dot != std::string::npos) {
        s.erase(s.find_last_not_of('0') + 1);
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    return s.empty() ? "0" : s;
}

std::string hex(Rgb c) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

// Silhouette outlines in the 0-100 box. Ordered fills only -- no boolean
// geometry, so the SVG and the sampler in identity.cpp can agree exactly.
const char* silhouette_path(Sil s) {
    switch (s) {
    case Sil::Disc:  return "M50,2 A48,48 0 1,1 49.9,2 Z";
    case Sil::Block: return "M9,2 H91 Q98,2 98,9 V91 Q98,98 91,98 H9 Q2,98 2,91 "
                            "V9 Q2,2 9,2 Z";
    default:         return "M2,2 H98 V98 H2 Z";
    }
}

std::string rect(double x, double y, double w, double h, Rgb c) {
    return "<rect x=\"" + num(x) + "\" y=\"" + num(y) + "\" width=\"" + num(w)
         + "\" height=\"" + num(h) + "\" fill=\"" + hex(c) + "\"/>";
}

std::string division(Div d, Rgb c) {
    switch (d) {
    case Div::Plain:     return "";
    case Div::Pale:      return rect(50, 0, 50, 100, c);
    case Div::Fess:      return rect(0, 50, 100, 50, c);
    case Div::Quarterly: return rect(50, 0, 50, 50, c) + rect(0, 50, 50, 50, c);
    default:             return rect(0, 0, 100, 30, c);   // Chief
    }
}

std::string charge_shape(Charge ch, double s, Rgb c) {
    const std::string fill = "fill=\"" + hex(c) + "\"";
    switch (ch) {
    case Charge::None:    return "";
    case Charge::Roundel: return "<circle cx=\"50\" cy=\"50\" r=\"" + num(s / 2)
                               + "\" " + fill + "/>";
    case Charge::Annulet: return "<circle cx=\"50\" cy=\"50\" r=\"" + num(s / 2)
                               + "\" fill=\"none\" stroke=\"" + hex(c)
                               + "\" stroke-width=\"" + num(s / 4) + "\"/>";
    case Charge::Lozenge: return "<path d=\"M50," + num(50 - s / 2) + " L"
                               + num(50 + s / 2) + ",50 L50," + num(50 + s / 2)
                               + " L" + num(50 - s / 2) + ",50 Z\" " + fill + "/>";
    case Charge::Pale:    return rect(50 - s / 2, 0, s, 100, c);
    case Charge::Fess:    return rect(0, 50 - s / 2, 100, s, c);
    default:              return rect(50 - s / 4, 0, s / 2, 100, c)
                               + rect(0, 50 - s / 4, 100, s / 2, c);   // Cross
    }
}

// Shrink a line until it fits the space it was given. The masthead's
// magic-offset bug came from guessing a width instead of measuring one; the
// same measurement is what keeps a long head inside its cell.
double fit_size(const char* text, uint8_t font, double want, double max_w,
                double tracking_ratio) {
    const double w = text_width(text, font, want, want * tracking_ratio);
    return w <= max_w || w <= 0 ? want : want * max_w / w;
}

}  // namespace

std::string svg_mark(const Mark& m, double x, double y, double size, uint32_t uid) {
    const auto ps = palettes();
    const Palette& p = ps[m.palette];
    const Palette& q = ps[m.palette2];
    const std::string id = "m" + std::to_string(uid);
    const double k = size / 100.0;

    std::string s;
    s += "<clipPath id=\"" + id + "\"><path d=\"";
    s += silhouette_path(m.sil);
    s += "\"/></clipPath>";
    s += "<g transform=\"translate(" + num(x) + "," + num(y) + ") scale(" + num(k)
       + ")\" clip-path=\"url(#" + id + ")\">";
    s += rect(0, 0, 100, 100, p.field);
    s += division(m.div, q.field);
    s += charge_shape(m.charge, m.charge_size, p.ink);
    s += "</g>";
    return s;
}

double text_width(const char* s, uint8_t font, double size, double tracking) {
    double w = 0;
    for (const char* p = s; *p;) {
        const uint32_t cp = next_codepoint(p);
        if (const Glyph* g = glyph(font, cp)) w += g->advance * size + tracking;
    }
    return w > 0 ? w - tracking : 0;
}

std::string svg_text(const char* s, uint8_t font, double x, double y, double size,
                     double tracking, Rgb colour, double* width) {
    std::string out = "<g fill=\"" + hex(colour) + "\">";
    double pen = x;
    for (const char* p = s; *p;) {
        const uint32_t cp = next_codepoint(p);
        const Glyph* g = glyph(font, cp);
        if (!g) continue;
        if (g->path[0] != '\0')
            // Glyph outlines are y-up in em units; the negative y scale flips
            // them into SVG's y-down space.
            out += "<path transform=\"translate(" + num(pen) + "," + num(y)
                 + ") scale(" + num(size) + "," + num(-size) + ")\" d=\""
                 + g->path + "\"/>";
        pen += g->advance * size + tracking;
    }
    out += "</g>";
    if (width) *width = pen > x ? pen - x - tracking : 0;
    return out;
}

std::string svg_identity(const Mark& m, const Name& n, double w, double h,
                         uint32_t uid) {
    const Palette& p = palettes()[m.palette];
    const Rgb ink{0x1A, 0x1A, 0x1A};
    const uint8_t f = m.font;

    // Kind decides what appears; Lock decides where. An abstract mark carries
    // no words, a wordmark carries no emblem.
    const bool show_mark = m.kind != Kind::Wordmark;
    const bool show_text = m.kind != Kind::Abstract;
    const bool initials  = m.kind == Kind::Lettermark;

    std::string head = n.head;
    if (initials) {
        // The lettermark shows the head's own initial letters, whole
        // codepoints only.
        std::string acc;
        bool at_word_start = true;
        for (const char* q = n.head; *q; ++q) {
            if (*q == ' ') { at_word_start = true; continue; }
            if ((static_cast<unsigned char>(*q) & 0xC0) == 0x80) {
                if (!acc.empty() && at_word_start) acc += *q;
                continue;
            }
            if (at_word_start) { acc += *q; at_word_start = false; }
        }
        head = acc.empty() ? std::string(1, n.head[0]) : acc;
    }

    std::string s;
    const double pad = h * 0.12;

    switch (m.lock) {
    case Lock::Stacked:
    case Lock::StackedWords: {
        const double ms = show_mark ? h * 0.34 : 0;
        const double top = h * 0.08;
        if (show_mark) s += svg_mark(m, (w - ms) / 2, top, ms, uid);
        if (show_text) {
            const double ts = fit_size(head.c_str(), f, h * 0.18, w - 2 * pad, 0.04);
            const double tw = text_width(head.c_str(), f, ts, ts * 0.04);
            const double base = top + ms + h * 0.20;
            s += svg_text(head.c_str(), f, (w - tw) / 2, base, ts, ts * 0.04, ink);
            const double ss = fit_size(n.tail, f, h * 0.095, w - 2 * pad, 0.18);
            const double sw = text_width(n.tail, f, ss, ss * 0.18);
            s += svg_text(n.tail, f, (w - sw) / 2, base + h * 0.16, ss, ss * 0.18,
                          p.field);
        }
        break;
    }
    case Lock::Enclosed: {
        // The wordmark sits on a plate in the firm's own field colour.
        const double ts = fit_size(head.c_str(), f, h * 0.22, w - 4 * pad, 0.05);
        const double tw = text_width(head.c_str(), f, ts, ts * 0.05);
        const double bw = tw + ts * 1.2, bh = ts * 1.8;
        s += rect((w - bw) / 2, (h - bh) / 2, bw, bh, p.field);
        s += svg_text(head.c_str(), f, (w - tw) / 2, (h + bh) / 2 - ts * 0.55, ts,
                      ts * 0.05, p.ink);
        break;
    }
    case Lock::MarkRight: {
        const double ms = h * 0.5;
        const double ts = fit_size(head.c_str(), f, h * 0.20,
                                   w - ms - 3 * pad, 0.04);
        const double tw = text_width(head.c_str(), f, ts, ts * 0.04);
        s += svg_text(head.c_str(), f, pad, h * 0.55, ts, ts * 0.04, ink);
        if (show_mark) s += svg_mark(m, pad + tw + pad, (h - ms) / 2, ms, uid);
        break;
    }
    default: {   // Horizontal, Lettermark, Wordmark
        const double ms = show_mark ? h * 0.5 : 0;
        if (show_mark) s += svg_mark(m, pad, (h - ms) / 2, ms, uid);
        const double tx = pad + (show_mark ? ms + pad * 0.8 : 0);
        if (show_text) {
            const double avail = w - tx - pad;
            const double ts = fit_size(head.c_str(), f, h * (initials ? 0.26 : 0.20),
                                       avail, 0.04);
            s += svg_text(head.c_str(), f, tx, h * 0.52, ts, ts * 0.04, ink);
            if (!initials) {
                const double ss = fit_size(n.tail, f, h * 0.10, avail, 0.18);
                s += svg_text(n.tail, f, tx, h * 0.52 + h * 0.155, ss, ss * 0.18,
                              p.field);
            }
        }
        break;
    }
    }
    return s;
}

std::string svg_sheet(uint64_t run_seed, uint32_t count, Locale locale) {
    constexpr uint32_t kCols = 6;
    constexpr double kCellW = 300, kCellH = 120;
    const uint32_t rows = (count + kCols - 1) / kCols;
    const double w = kCols * kCellW, h = rows * kCellH;

    std::string s = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" + num(w)
                  + "\" height=\"" + num(h) + "\" viewBox=\"0 0 " + num(w) + " "
                  + num(h) + "\">";
    s += rect(0, 0, w, h, Rgb{0xF7, 0xF6, 0xF2});
    for (uint32_t i = 0; i < count; ++i) {
        const uint64_t seed = firm_seed(run_seed, i);
        const Mark m = mark(seed);
        const Name n = name(seed, locale);
        const double x = (i % kCols) * kCellW, y = (i / kCols) * kCellH;
        s += "<g transform=\"translate(" + num(x) + "," + num(y) + ")\">";
        s += svg_identity(m, n, kCellW, kCellH, i);
        s += "</g>";
    }
    s += "</svg>\n";
    return s;
}

}  // namespace identity
