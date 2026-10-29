#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Conversor SVG -> VectorDrawable (Android), sem dependencias externas.

Uso:
    python tools/svg2vd.py ENTRADA.svg SAIDA.xml
    python tools/svg2vd.py --all docs/brand/svg app/src/main/res/drawable
        (usa o mapa de nomes DRAWABLE_NAMES de tools/brand_gen.py)

SUBCONJUNTO SUPORTADO (tudo fora disso falha com erro explicito):
  Elementos : svg, g, path, rect (com rx/ry), circle, polygon, polyline, line.
              <title>, <desc> e comentarios sao ignorados.
  Atributos : fill, stroke, stroke-width, stroke-linecap, stroke-linejoin,
              fill-rule (nonzero|evenodd), opacity, fill-opacity,
              stroke-opacity. Herdam de <svg>/<g> como no SVG.
  Cores     : #RGB, #RRGGBB, #RRGGBBAA, none, currentColor (vira #FFFFFFFF,
              para tintar por codigo com setTint).
  Path data : M m L l H h V v C c S s Q q T t A a Z z (repassado ao Android).
NAO SUPORTADO: transform, style="", classes/CSS, gradientes, <use>, <defs>,
  <clipPath>, <mask>, <text>, filtros, unidades diferentes de px, tracejado.
  (Texto deve virar forma antes; ver tools/brand_gen.py.)
"""
import re
import sys
import xml.etree.ElementTree as ET

NS = "{http://www.w3.org/2000/svg}"
PAINT_ATTRS = ("fill", "stroke", "stroke-width", "stroke-linecap", "stroke-linejoin",
               "fill-rule", "fill-opacity", "stroke-opacity", "opacity")
FORBIDDEN_ATTRS = ("transform", "style", "class", "clip-path", "mask", "filter", "stroke-dasharray")
SKIP_TAGS = ("title", "desc", "metadata")


class SvgError(Exception):
    pass


def num(v):
    s = ("%.3f" % float(v)).rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


def f(el, name, default=0.0):
    v = el.get(name)
    if v is None:
        return default
    v = v.strip()
    if v.endswith("px"):
        v = v[:-2]
    try:
        return float(v)
    except ValueError:
        raise SvgError("valor numerico invalido em %s=%r" % (name, v))


def color(value, what):
    v = value.strip()
    if v == "none":
        return None
    if v.lower() == "currentcolor":
        return "#FFFFFFFF"
    m = re.fullmatch(r"#([0-9a-fA-F]{3}|[0-9a-fA-F]{6}|[0-9a-fA-F]{8})", v)
    if not m:
        raise SvgError("cor nao suportada em %s: %r" % (what, value))
    h = m.group(1)
    if len(h) == 3:
        h = "".join(c * 2 for c in h)
    if len(h) == 6:
        return "#FF" + h.upper()
    # SVG usa RRGGBBAA; Android usa AARRGGBB
    return "#" + (h[6:8] + h[0:6]).upper()


def shape_to_path(el):
    tag = el.tag.replace(NS, "")
    if tag == "path":
        d = el.get("d")
        if not d:
            raise SvgError("<path> sem d")
        return re.sub(r"\s+", " ", d.strip())
    if tag == "rect":
        x, y, w, h = f(el, "x"), f(el, "y"), f(el, "width"), f(el, "height")
        rx, ry = f(el, "rx", -1), f(el, "ry", -1)
        if rx < 0 and ry < 0:
            rx = ry = 0.0
        elif rx < 0:
            rx = ry
        elif ry < 0:
            ry = rx
        rx, ry = min(rx, w / 2), min(ry, h / 2)
        if rx == 0 or ry == 0:
            return "M%s,%sh%sv%sh%sz" % (num(x), num(y), num(w), num(h), num(-w))
        return ("M%s,%s h%s a%s,%s 0 0 1 %s,%s v%s a%s,%s 0 0 1 %s,%s h%s a%s,%s 0 0 1 %s,%s v%s a%s,%s 0 0 1 %s,%s z" % (
            num(x + rx), num(y), num(w - 2 * rx), num(rx), num(ry), num(rx), num(ry),
            num(h - 2 * ry), num(rx), num(ry), num(-rx), num(ry),
            num(-(w - 2 * rx)), num(rx), num(ry), num(-rx), num(-ry),
            num(-(h - 2 * ry)), num(rx), num(ry), num(rx), num(-ry)))
    if tag == "circle":
        cx, cy, r = f(el, "cx"), f(el, "cy"), f(el, "r")
        return "M%s,%s a%s,%s 0 1,0 %s,0 a%s,%s 0 1,0 %s,0 z" % (
            num(cx - r), num(cy), num(r), num(r), num(2 * r), num(r), num(r), num(-2 * r))
    if tag in ("polygon", "polyline"):
        pts = [float(p) for p in re.split(r"[\s,]+", (el.get("points") or "").strip()) if p]
        if len(pts) < 4 or len(pts) % 2:
            raise SvgError("<%s> com points invalido" % tag)
        out = "M%s,%s" % (num(pts[0]), num(pts[1]))
        for i in range(2, len(pts), 2):
            out += " L%s,%s" % (num(pts[i]), num(pts[i + 1]))
        return out + (" z" if tag == "polygon" else "")
    if tag == "line":
        return "M%s,%s L%s,%s" % (num(f(el, "x1")), num(f(el, "y1")), num(f(el, "x2")), num(f(el, "y2")))
    raise SvgError("elemento nao suportado: <%s>" % tag)


def walk(el, inherited, out):
    for child in el:
        if not isinstance(child.tag, str):
            continue
        tag = child.tag.replace(NS, "")
        if tag in SKIP_TAGS:
            continue
        for bad in FORBIDDEN_ATTRS:
            if bad in child.attrib:
                raise SvgError("atributo nao suportado: %s em <%s>" % (bad, tag))
        style = dict(inherited)
        for a in PAINT_ATTRS:
            if a in child.attrib:
                style[a] = child.get(a)
        if tag == "g":
            walk(child, style, out)
        else:
            out.append((shape_to_path(child), style))


def attrs_for(style):
    fill = color(style.get("fill", "#000000"), "fill")
    stroke = color(style.get("stroke", "none"), "stroke")
    if not fill and not stroke:
        return None
    lines = []
    op = float(style.get("opacity", 1))
    fo = float(style.get("fill-opacity", 1)) * op
    so = float(style.get("stroke-opacity", 1)) * op
    if fill:
        lines.append('android:fillColor="%s"' % fill)
        if style.get("fill-rule") == "evenodd":
            lines.append('android:fillType="evenOdd"')
        if fo < 1:
            lines.append('android:fillAlpha="%s"' % num(fo))
    if stroke:
        lines.append('android:strokeColor="%s"' % stroke)
        lines.append('android:strokeWidth="%s"' % num(float(style.get("stroke-width", 1))))
        lines.append('android:strokeLineCap="%s"' % style.get("stroke-linecap", "butt"))
        lines.append('android:strokeLineJoin="%s"' % style.get("stroke-linejoin", "miter"))
        if so < 1:
            lines.append('android:strokeAlpha="%s"' % num(so))
    return lines


def convert(svg_text):
    root = ET.fromstring(svg_text)
    if root.tag.replace(NS, "") != "svg":
        raise SvgError("raiz nao e <svg>")
    vb = (root.get("viewBox") or "").replace(",", " ").split()
    if len(vb) != 4 or float(vb[0]) != 0 or float(vb[1]) != 0:
        raise SvgError("viewBox deve ser '0 0 W H'")
    vw, vh = float(vb[2]), float(vb[3])
    w = f(root, "width", vw)
    h = f(root, "height", vh)
    base = {}
    for a in PAINT_ATTRS:
        if a in root.attrib:
            base[a] = root.get(a)
    items = []
    walk(root, base, items)
    if not items:
        raise SvgError("SVG vazio")
    xml = ['<?xml version="1.0" encoding="utf-8"?>',
           '<!-- Gerado por tools/svg2vd.py a partir de docs/brand/svg. Nao edite a mao. -->',
           '<vector xmlns:android="http://schemas.android.com/apk/res/android"',
           '    android:width="%sdp"' % num(w),
           '    android:height="%sdp"' % num(h),
           '    android:viewportWidth="%s"' % num(vw),
           '    android:viewportHeight="%s">' % num(vh)]
    for d, style in items:
        a = attrs_for(style)
        if a is None:
            continue
        xml.append("    <path")
        for line in a:
            xml.append("        " + line)
        xml.append('        android:pathData="%s" />' % d)
    xml.append("</vector>")
    return "\n".join(xml) + "\n"


def main(argv):
    if len(argv) == 4 and argv[1] == "--all":
        import os
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import brand_gen
        for svg_name, vd_name in brand_gen.DRAWABLE_NAMES.items():
            src = os.path.join(argv[2], svg_name + ".svg")
            with open(src, encoding="utf-8") as fh:
                xml = convert(fh.read())
            with open(os.path.join(argv[3], vd_name + ".xml"), "w", encoding="utf-8", newline="\n") as fh:
                fh.write(xml)
        print("ok: %d drawables" % len(brand_gen.DRAWABLE_NAMES))
        return 0
    if len(argv) != 3:
        print(__doc__)
        return 2
    with open(argv[1], encoding="utf-8") as fh:
        xml = convert(fh.read())
    with open(argv[2], "w", encoding="utf-8", newline="\n") as fh:
        fh.write(xml)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except SvgError as e:
        print("erro: %s" % e, file=sys.stderr)
        sys.exit(1)
