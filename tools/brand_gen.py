#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Gera os SVGs mestres da identidade do Zombie Zone e docs/brand/preview.html.

Uso (a partir da raiz do repositorio):
    python tools/brand_gen.py            -> escreve docs/brand/svg/*.svg e docs/brand/preview.html
    python tools/svg2vd.py --all docs/brand/svg app/src/main/res/drawable

Os icones 24x24 ficam definidos aqui (corpo SVG em texto) para manter a familia
coerente: traco 2, pontas e juntas redondas, preenchimento pontual em
'currentColor'. A fonte de verdade visual sao os arquivos em docs/brand/svg.
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SVG_DIR = os.path.join(ROOT, "docs", "brand", "svg")

FILL = 'fill="currentColor" stroke="none"'

# (nome-svg, nome-drawable, categoria, rotulo, corpo)
ICONS = [
    # contratos
    ("recuperacao-silenciosa", "zz_ic_silent_recovery", "contrato", "Recuperacao silenciosa",
     '<path d="M4 9h4l5-4v14l-5-4H4z"/><path d="M16.5 9.5l4.5 5M21 9.5l-4.5 5"/>'),
    ("ninho-instavel", "zz_ic_unstable_nest", "contrato", "Ninho instavel",
     '<path d="M3.5 20a8.5 8.5 0 0 1 17 0z"/><path d="M13 4.5l-3 5h4l-3 5"/>'),
    ("noite-vermelha", "zz_ic_red_night", "contrato", "Noite vermelha",
     '<path d="M20 14.5A8.5 8.5 0 1 1 9.5 4a7 7 0 0 0 10.5 10.5z"/><circle cx="17" cy="6" r="1.4" ' + FILL + '/>'),
    ("caixa-perdida", "zz_ic_lost_crate", "contrato", "Caixa perdida",
     '<path d="M3 8l9-4 9 4v9l-9 4-9-4z"/><path d="M3 8l9 4 9-4M12 12v9"/>'),
    ("resistencia", "zz_ic_endurance", "contrato", "Resistencia",
     '<path d="M6 3h12M6 21h12"/><path d="M7 3c0 5 5 6 5 9s-5 4-5 9M17 3c0 5-5 6-5 9s5 4 5 9"/>'),
    # objetivos
    ("ninho", "zz_ic_nest", "objetivo", "Ninho",
     '<path d="M3 20h18"/><path d="M5 20a7 7 0 0 1 14 0"/><circle cx="12" cy="16" r="2.4" ' + FILL + '/><path d="M12 4v3M5 7l2 2M19 7l-2 2"/>'),
    ("cristal", "zz_ic_crystal", "objetivo", "Cristal",
     '<path d="M12 2.5l7 6.5-7 12.5L5 9z"/><path d="M5 9h14M12 2.5L9 9l3 12.5L15 9z"/>'),
    ("portal-extracao", "zz_ic_extraction_portal", "objetivo", "Portal de extracao",
     '<path d="M5 21V10a7 7 0 0 1 14 0v11"/><path d="M12 18v-6M9.5 14.5L12 12l2.5 2.5"/>'),
    # familias de ninho (forma + cor distintas)
    ("familia-horda", "zz_ic_family_horde", "familia", "Ninho de horda",
     '<circle cx="12" cy="6.5" r="3.5" ' + FILL + '/><circle cx="6" cy="16.5" r="3.5" ' + FILL + '/><circle cx="18" cy="16.5" r="3.5" ' + FILL + '/>'),
    ("familia-explosivo", "zz_ic_family_explosive", "familia", "Ninho explosivo",
     '<circle cx="10.5" cy="14.5" r="6.5"/><path d="M15 10l3.5-3.5M18.5 2.5v3M21.5 5.5h-3"/><circle cx="10.5" cy="14.5" r="2" ' + FILL + '/>'),
    ("familia-toxico", "zz_ic_family_toxic", "familia", "Ninho toxico",
     '<path d="M12 3c4 5 7 8 7 12a7 7 0 0 1-14 0c0-4 3-7 7-12z"/><circle cx="12" cy="15" r="2.2" ' + FILL + '/>'),
    # recursos
    ("sucata", "zz_ic_scrap", "recurso", "Sucata",
     '<path d="M12 3l8 4.5v9L12 21l-8-4.5v-9z"/><circle cx="12" cy="12" r="3"/>'),
    ("material-infectado", "zz_ic_infected_material", "recurso", "Material infectado",
     '<path d="M9 3h6M10 3v6l-5 9a2 2 0 0 0 1.7 3h10.6a2 2 0 0 0 1.7-3l-5-9V3"/><circle cx="10.5" cy="16" r="1.3" ' + FILL + '/><circle cx="14.5" cy="14" r="1" ' + FILL + '/>'),
    # implantaveis
    ("barricada-dobravel", "zz_ic_barricade", "implantavel", "Barricada dobravel",
     '<path d="M8 5L3.5 21M16 5l4.5 16M6.5 11h11M5 16h14M8 5h8"/>'),
    ("isca-sonora", "zz_ic_sound_lure", "implantavel", "Isca sonora",
     '<path d="M4 9h4l5-4v14l-5-4H4z"/><path d="M16.5 9a4 4 0 0 1 0 6M19 6.5a8 8 0 0 1 0 11"/>'),
    ("kit-de-campo", "zz_ic_field_kit", "implantavel", "Kit de campo",
     '<path d="M3 7h18v13H3z"/><path d="M9 7V4h6v3M12 10.5v6M9 13.5h6"/>'),
    # utilitarios
    ("compartilhar", "zz_ic_share", "utilitario", "Compartilhar",
     '<circle cx="18" cy="5" r="2.5" ' + FILL + '/><circle cx="6" cy="12" r="2.5" ' + FILL + '/><circle cx="18" cy="19" r="2.5" ' + FILL + '/><path d="M8.2 10.8l7.6-4.6M8.2 13.2l7.6 4.6"/>'),
    ("codigo-qr", "zz_ic_qr", "utilitario", "Codigo QR",
     '<rect x="3.5" y="3.5" width="7" height="7"/><rect x="13.5" y="3.5" width="7" height="7"/><rect x="3.5" y="13.5" width="7" height="7"/>'
     '<rect x="6" y="6" width="2" height="2" ' + FILL + '/><rect x="16" y="6" width="2" height="2" ' + FILL + '/><rect x="6" y="16" width="2" height="2" ' + FILL + '/>'
     '<rect x="13.5" y="13.5" width="3" height="3" ' + FILL + '/><rect x="18" y="18" width="3" height="3" ' + FILL + '/>'),
    ("relatorio", "zz_ic_report", "utilitario", "Relatorio / ficha",
     '<path d="M5 4h14v17H5z"/><path d="M9 4V2.5h6V4M8 10h8M8 14h8M8 18h5"/>'),
    ("bussola", "zz_ic_compass", "utilitario", "Bussola",
     '<circle cx="12" cy="12" r="9"/><path d="M15.5 8.5l-2 5-5 2 2-5z" ' + 'fill="currentColor"/>'),
    ("caveira", "zz_ic_skull", "utilitario", "Caveira / causa da morte",
     '<path d="M12 3a8 8 0 0 0-8 8c0 3 1.5 4.5 3 5.5V20h10v-3.5c1.5-1 3-2.5 3-5.5a8 8 0 0 0-8-8z"/>'
     '<circle cx="9" cy="11" r="1.8" ' + FILL + '/><circle cx="15" cy="11" r="1.8" ' + FILL + '/><path d="M10 20v-3M14 20v-3"/>'),
    ("relogio", "zz_ic_clock", "utilitario", "Relogio",
     '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>'),
    ("combo", "zz_ic_combo", "utilitario", "Combo",
     '<path d="M12 2.5c1 4.5 6 6 6 11.5a6 6 0 0 1-12 0c0-3 2-4 2-7 2 1 3 .5 4-4.5z"/><path d="M12 21a2.5 2.5 0 0 1-2.5-2.5c0-1.7 1.5-2.2 2.5-4 1 1.8 2.5 2.3 2.5 4A2.5 2.5 0 0 1 12 21z" ' + FILL + '/>'),
]

CAT_COLOR = {  # cores de apresentacao no preview (tokens da Palette)
    "contrato": "#3CE6FF", "objetivo": "#A8FF3E", "familia": "#FF8A3D", "recurso": "#FFC83D",
    "implantavel": "#A8FF3E", "utilitario": "#F4F7FF",
}
FAMILY_COLOR = {"familia-horda": "#FF8A3D", "familia-explosivo": "#FFD23D", "familia-toxico": "#4FE3A0"}

# ---------- fonte pixel 5x7 (voxel) para wordmark e selo ----------
GLYPHS = {
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "I": ["11111", "00100", "00100", "00100", "00100", "00100", "11111"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "N": ["10001", "11001", "10101", "10101", "10011", "10001", "10001"],
}


def rects(word, x0, y0, cell, fill):
    """Uma <path> com retangulos (corridas horizontais fundidas) para a palavra."""
    d = []
    x = x0
    for ch in word:
        if ch == " ":
            x += 3 * cell
            continue
        g = GLYPHS[ch]
        for r, row in enumerate(g):
            c = 0
            while c < 5:
                if row[c] == "1":
                    s = c
                    while c < 5 and row[c] == "1":
                        c += 1
                    d.append("M%g,%gh%gv%gh%gz" % (x + s * cell, y0 + r * cell, (c - s) * cell, cell, -(c - s) * cell))
                else:
                    c += 1
        x += 6 * cell
    return '<path d="%s" fill="%s"/>' % (" ".join(d), fill)


def wordmark():
    cell = 4
    # sombra (deslocada 2) e letras: ZOMBIE em lime, ZONE em branco-osso
    shadow = "#0E1A08"
    parts = [
        rects("ZOMBIE", 4, 4, cell, shadow), rects("ZONE", 4 + 6 * 6 * cell + 3 * cell, 4, cell, shadow),
        rects("ZOMBIE", 2, 2, cell, "#A8FF3E"), rects("ZONE", 2 + 6 * 6 * cell + 3 * cell, 2, cell, "#F4F7FF"),
    ]
    w = 2 + 6 * 6 * cell + 3 * cell + 4 * 6 * cell - cell + 4
    return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d 34" width="%d" height="34">\n  <title>ZOMBIE ZONE</title>\n  %s\n</svg>\n' % (
        w, w, "\n  ".join(parts))


def logo(bg=False):
    body = (
        '<polygon points="54,26 80,40 54,54 28,40" fill="#A8FF3E"/>'
        '<polygon points="28,40 54,54 54,84 28,70" fill="#5DA81E"/>'
        '<polygon points="54,54 80,40 80,70 54,84" fill="#3B7A12"/>'
        '<polygon points="54,34 60,40 54,46 48,40" fill="#FF3D3D"/>'
        '<path d="M54,12v9M54,87v9M12,54h9M87,54h9" fill="none" stroke="#F4F7FF" stroke-width="4" stroke-linecap="square"/>'
        '<path d="M30,22A42,42 0 0 1 78,22M30,86A42,42 0 0 0 78,86" fill="none" stroke="#A8FF3E" stroke-opacity="0.45" stroke-width="3"/>'
    )
    return body


def logo_svg():
    return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 108 108" width="108" height="108">\n  <title>Emblema Zombie Zone</title>\n  %s\n</svg>\n' % logo().replace("/>", "/>\n  ").rstrip()


def app_icon_svg():
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 108 108" width="108" height="108">\n'
            '  <title>Proposta de icone do app (fundo + emblema na zona segura 66dp)</title>\n'
            '  <rect width="108" height="108" fill="#08111F"/>\n'
            '  <g transform="translate(14.04 14.04) scale(0.74)">\n  %s\n  </g>\n</svg>\n') % logo().replace("/>", "/>\n  ").rstrip()


def seal_svg():
    zz = rects("ZZ", 26, 36, 4, "#A8FF3E")
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 96 96" width="96" height="96">\n'
        '  <title>Selo de expedicao</title>\n'
        '  <polygon points="30,3 66,3 93,30 93,66 66,93 30,93 3,66 3,30" fill="#111A2E" stroke="#FFC83D" stroke-width="3"/>\n'
        '  <circle cx="48" cy="48" r="35" fill="none" stroke="#A8FF3E" stroke-width="2" stroke-opacity="0.6"/>\n'
        '  <path d="M48,9v7M48,80v7M9,48h7M80,48h7" fill="none" stroke="#F4F7FF" stroke-width="3" stroke-linecap="square"/>\n'
        '  ' + zz + '\n'
        '  <path d="M30,70h36" fill="none" stroke="#FFC83D" stroke-width="3" stroke-linecap="square"/>\n'
        '</svg>\n')


def icon_svg(name, label, body):
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="currentColor" '
            'stroke-width="2" stroke-linecap="round" stroke-linejoin="round">\n  <title>%s</title>\n  %s\n</svg>\n') % (label, body)


DRAWABLE_NAMES = {n: d for (n, d, _c, _l, _b) in ICONS}
DRAWABLE_NAMES.update({"wordmark": "zz_wordmark", "logo": "zz_logo", "selo": "zz_seal"})


def write(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)


def inline(svg_text, size=None):
    s = svg_text.split("\n", 1)[1] if svg_text.startswith("<?xml") else svg_text
    if size:
        s = s.replace('width="24" height="24"', 'width="%d" height="%d"' % (size, size), 1)
    return s


def preview(files):
    css = ("body{margin:0;font-family:sans-serif}section{padding:16px 20px}.dark{background:#0A1120;color:#F4F7FF}.light{background:#F4F7FF;color:#0A1120}"
           "h1,h2{margin:0 0 10px}.grid{display:flex;flex-wrap:wrap;gap:14px}.cell{width:150px;text-align:center;font-size:11px}"
           ".row{display:flex;gap:10px;align-items:flex-end;justify-content:center;margin-bottom:4px}svg{display:block}code{opacity:.7}")
    out = ['<!doctype html><html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
           '<title>Zombie Zone - identidade visual</title><style>%s</style></head><body>' % css]
    for theme in ("dark", "light"):
        out.append('<section class="%s"><h1>Fundo %s</h1>' % (theme, "escuro" if theme == "dark" else "claro"))
        out.append('<h2>Marca</h2><div class="grid">')
        for key, label in (("wordmark", "wordmark"), ("logo", "emblema 108"), ("selo", "selo 96"), ("app-icon", "icone do app (proposta)")):
            out.append('<div class="cell"><div class="row">%s</div>%s</div>' % (inline(files[key]), label))
        out.append('</div>')
        cats = []
        for (n, d, c, l, b) in ICONS:
            if c not in cats:
                cats.append(c)
        for cat in cats:
            out.append('<h2>%s</h2><div class="grid">' % cat.capitalize())
            for (n, d, c, l, b) in ICONS:
                if c != cat:
                    continue
                col = FAMILY_COLOR.get(n, CAT_COLOR[c]) if theme == "dark" else "#0A1120"
                sizes = "".join(inline(files[n], s) for s in (24, 32, 48))
                out.append('<div class="cell"><div class="row" style="color:%s">%s</div>%s<br><code>%s</code></div>' % (col, sizes, l, d))
            out.append('</div>')
        out.append('</section>')
    out.append('</body></html>')
    return "\n".join(out)


def main():
    os.makedirs(SVG_DIR, exist_ok=True)
    files = {"wordmark": wordmark(), "logo": logo_svg(), "selo": seal_svg(), "app-icon": app_icon_svg()}
    for (n, d, c, l, b) in ICONS:
        files[n] = icon_svg(n, l, b)
    for k, v in files.items():
        write(os.path.join(SVG_DIR, k + ".svg"), v)
    write(os.path.join(ROOT, "docs", "brand", "preview.html"), preview(files))
    print("ok: %d svgs + preview.html" % len(files))


if __name__ == "__main__":
    sys.exit(main())
