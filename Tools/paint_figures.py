"""SVG figures for the MintChoco paint deck, computed from the formulas.

    python Tools/paint_figures.py captures/paintlab/run1/figures [--png]

Every curve, vector and grid is computed here from the same constants the
port uses; nothing is a hand-placed coordinate on a picture. --png also
rasterizes each SVG through headless Edge for the deck's fallback images.
"""

import argparse
import base64
import math
import struct
import subprocess
import sys
from pathlib import Path

INK = "#2B1B12"
TEXT = "#2A221D"
MUTED = "#8A7F78"
MINT = "#1FA88E"
MINT_LIGHT = "#E2F4EF"
CHOCO = "#6B4430"
CHOCO_LIGHT = "#F3ECE6"
GOLD = "#E0A526"
GRID = "#D9D2CC"
BLUE = "#4F7CC9"
RED = "#D9654B"
GREEN = "#5DAA59"
FONT = "Malgun Gothic, 'Segoe UI', sans-serif"
MONO = "Consolas, monospace"
W, H = 1600, 900
# the band above this is left to the slide's own title
TOP = 70

DIRECTION_COLORS = {"Front": RED, "Back": "#E7A08F", "Right": GREEN,
                    "Left": "#A9D3A6", "Up": BLUE, "Down": "#A9BDE3"}


def esc(text):
    return (str(text).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


class Svg:
    def __init__(self, title, w=W, h=H):
        self.w, self.h = w, h
        self.items = []
        self.title = title

    def add(self, s):
        self.items.append(s)

    def rect(self, x, y, w, h, fill="none", stroke=INK, sw=2, rx=0, opacity=1.0, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{rx}" '
                 f'fill="{fill}" stroke="{stroke}" stroke-width="{sw}" opacity="{opacity}"{d}/>')

    def line(self, x1, y1, x2, y2, stroke=INK, sw=2, dash=None, arrow=False, opacity=1.0):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        m = ' marker-end="url(#arrow)"' if arrow else ""
        self.add(f'<line x1="{x1:.2f}" y1="{y1:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" stroke="{stroke}" '
                 f'stroke-width="{sw}" opacity="{opacity}"{d}{m}/>')

    def arrow(self, x1, y1, x2, y2, stroke=INK, sw=3, dash=None):
        # the head is drawn here so it takes the stroke's color
        self.line(x1, y1, x2, y2, stroke, sw, dash)
        a = math.atan2(y2 - y1, x2 - x1)
        size = 10 + sw * 2
        p1 = (x2 - size * math.cos(a - 0.4), y2 - size * math.sin(a - 0.4))
        p2 = (x2 - size * math.cos(a + 0.4), y2 - size * math.sin(a + 0.4))
        self.polygon([(x2, y2), p1, p2], fill=stroke, stroke="none")

    def polyline(self, pts, stroke=INK, sw=3, fill="none", dash=None, opacity=1.0):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        p = " ".join(f"{x:.2f},{y:.2f}" for x, y in pts)
        self.add(f'<polyline points="{p}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" '
                 f'stroke-linejoin="round" opacity="{opacity}"{d}/>')

    def polygon(self, pts, fill=MINT_LIGHT, stroke=INK, sw=2, opacity=1.0):
        p = " ".join(f"{x:.2f},{y:.2f}" for x, y in pts)
        self.add(f'<polygon points="{p}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" '
                 f'opacity="{opacity}" stroke-linejoin="round"/>')

    def circle(self, x, y, r, fill="none", stroke=INK, sw=2, opacity=1.0, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r:.2f}" fill="{fill}" stroke="{stroke}" '
                 f'stroke-width="{sw}" opacity="{opacity}"{d}/>')

    def ellipse(self, x, y, rx, ry, rot=0.0, fill="none", stroke=INK, sw=2, opacity=1.0, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<ellipse cx="{x:.2f}" cy="{y:.2f}" rx="{rx:.2f}" ry="{ry:.2f}" '
                 f'transform="rotate({rot:.2f} {x:.2f} {y:.2f})" fill="{fill}" stroke="{stroke}" '
                 f'stroke-width="{sw}" opacity="{opacity}"{d}/>')

    def text(self, x, y, s, size=28, fill=TEXT, anchor="start", weight="normal", font=FONT, italic=False):
        # the figure lands about 8 inches wide: nothing smaller than 24 reads
        size = max(size, 24)
        style = ' font-style="italic"' if italic else ""
        self.add(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{font}" font-size="{size}" '
                 f'fill="{fill}" text-anchor="{anchor}" font-weight="{weight}"{style}>{esc(s)}</text>')

    def image(self, x, y, w, h, png):
        # embedded, so the figure travels into the deck whole
        data = base64.b64encode(Path(png).read_bytes()).decode()
        self.add(f'<image x="{x}" y="{y}" width="{w}" height="{h}" '
                 f'href="data:image/png;base64,{data}" preserveAspectRatio="xMidYMid meet"/>')

    def box(self, x, y, w, h, label, fill=CHOCO_LIGHT, stroke=CHOCO, size=26, sub=None, weight="bold"):
        self.rect(x, y, w, h, fill=fill, stroke=stroke, sw=2.5, rx=14)
        cy = y + h / 2 + (size * 0.35 if not sub else -4)
        self.text(x + w / 2, cy, label, size=size, anchor="middle", weight=weight, fill=INK)
        if sub:
            self.text(x + w / 2, cy + size + 4, sub, size=size * 0.72, anchor="middle", fill=TEXT)

    def heading(self, s):
        # the slide's title says it; the figure keeps it only as its <title>
        self.title = f"{self.title}: {s}"

    def save(self, path):
        body = "\n".join(self.items)
        h = self.h - TOP
        Path(path).write_text(
            f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 {TOP} {self.w} {h}" '
            f'width="{self.w}" height="{h}">\n<title>{esc(self.title)}</title>\n'
            f'<rect y="{TOP}" width="{self.w}" height="{h}" fill="#FFFFFF"/>\n{body}\n</svg>\n',
            encoding="utf-8",
        )


class Plot:
    """a data-to-pixel mapping for one chart area, with axes"""

    def __init__(self, svg, x, y, w, h, xr, yr):
        self.svg, self.x, self.y, self.w, self.h, self.xr, self.yr = svg, x, y, w, h, xr, yr

    def px(self, vx, vy):
        return (self.x + (vx - self.xr[0]) / (self.xr[1] - self.xr[0]) * self.w,
                self.y + self.h - (vy - self.yr[0]) / (self.yr[1] - self.yr[0]) * self.h)

    def axes(self, xlabel, ylabel, xticks=(), yticks=(), fmt="{:g}"):
        s = self.svg
        for t in xticks:
            px, _ = self.px(t, self.yr[0])
            s.line(px, self.y, px, self.y + self.h, stroke=GRID, sw=1)
            s.text(px, self.y + self.h + 30, fmt.format(t), size=20, anchor="middle", fill=MUTED)
        for t in yticks:
            _, py = self.px(self.xr[0], t)
            s.line(self.x, py, self.x + self.w, py, stroke=GRID, sw=1)
            s.text(self.x - 10, py + 7, fmt.format(t), size=20, anchor="end", fill=MUTED)
        s.line(self.x, self.y + self.h, self.x + self.w, self.y + self.h, sw=2)
        s.line(self.x, self.y, self.x, self.y + self.h, sw=2)
        s.text(self.x + self.w / 2, self.y + self.h + 62, xlabel, size=22, anchor="middle")
        s.add(f'<text x="{self.x - 64:.1f}" y="{self.y + self.h / 2:.1f}" font-family="{FONT}" '
              f'font-size="22" fill="{TEXT}" text-anchor="middle" '
              f'transform="rotate(-90 {self.x - 64:.1f} {self.y + self.h / 2:.1f})">{esc(ylabel)}</text>')

    def curve(self, f, x0, x1, n=240, **kw):
        pts = [self.px(x0 + (x1 - x0) * i / n, f(x0 + (x1 - x0) * i / n)) for i in range(n + 1)]
        self.svg.polyline(pts, **kw)
        return pts


# ---------------------------------------------------------------- the math

PAD = 13
MIN_ATLAS, MAX_ATLAS = 256, 2048
TEXEL_CM = 0.5
PAINT_MAX_HEIGHT = 9.0


def dominant(n):
    """classifyPaintFaceDirection: the largest |component|, ties X > Y > Z"""
    ax, ay, az = (abs(c) for c in n)
    if ax >= ay and ax >= az:
        return "Front" if n[0] >= 0 else "Back"
    if ay >= az:
        return "Right" if n[1] >= 0 else "Left"
    return "Up" if n[2] >= 0 else "Down"


def texel_for(side_cm):
    """the texel a square Up island needs to fit the largest atlas"""
    need = side_cm / TEXEL_CM + 2 * PAD
    if need <= MAX_ATLAS:
        return TEXEL_CM
    return side_cm / (MAX_ATLAS - 2 * PAD)


def build_radius(volume, speed, base=25.0, rps=0.005, max_r=120.0):
    return min(base * math.sqrt(volume) + rps * speed, max_r)


def stretch_of(theta_deg, max_s=3.0):
    return min(max(1.0 / max(math.cos(math.radians(theta_deg)), 1e-4), 1.0), max_s)


def bspline(x):
    a = abs(x)
    if a < 1:
        return 0.5 * a ** 3 - a * a + 2.0 / 3.0
    if a < 2:
        return (2 - a) ** 3 / 6.0
    return 0.0


def bspline_slope(x):
    a = abs(x)
    s = -1.0 if x < 0 else 1.0
    if a < 1:
        return s * (1.5 * a * a - 2 * a)
    if a < 2:
        return -s * (2 - a) ** 2 * 0.5
    return 0.0


def ggx(rough, noh):
    a = max(rough * rough, 2e-3)
    a2 = a * a
    d = (noh * a2 - noh) * noh + 1
    return a2 / (math.pi * d * d)


def charlie(rough, noh):
    r = max(rough * rough, 0.07)
    sin2 = max(1 - noh * noh, 1e-4)
    return (2 + 1 / r) * sin2 ** (0.5 / r) / (2 * math.pi)


class RandomStream:
    """FRandomStream, bit for bit: the seed mutates before every draw"""

    def __init__(self, seed):
        self.seed = seed & 0xFFFFFFFF

    def frand(self):
        self.seed = (self.seed * 196314165 + 907633515) & 0xFFFFFFFF
        return struct.unpack("<f", struct.pack("<I", 0x3F800000 | (self.seed >> 9)))[0] - 1.0

    def range(self, lo, hi):
        return lo + (hi - lo) * self.frand()


def hash_combine(a, b):
    return (a ^ ((b + 0x9E3779B9 + ((a << 6) & 0xFFFFFFFF) + (a >> 2)) & 0xFFFFFFFF)) & 0xFFFFFFFF


GROUPS = {"Forward": (50, 35, 70, 0.10, 0.20, 0.15, 0.0),
          "Side": (35, 25, 60, 0.06, 0.14, 0.05, 90.0),
          "Back": (40, 15, 45, 0.04, 0.10, 0.0, 180.0)}


def split_groups(count, share):
    forward_share = 0.4 + share * (0.75 - 0.4)
    f = min(max(math.floor(count * forward_share + 0.5), 0), count)
    rest = count - f
    b = min(max(math.floor(rest * 0.3 + 0.5), 0), rest)
    return f, rest - b, b


def droplets_for(velocity, seed, ball_radius=12.0):
    """generateDroplets on a floor, in doubles: enough for a picture"""
    vn = -velocity[2]
    vt = (velocity[0], velocity[1])
    ts = math.hypot(*vt)
    if vn < 400:
        return []
    stream = RandomStream(hash_combine(seed & 0xFFFFFFFF, 0x53504C48))
    launch = vn + 0.5 * ts
    fwd = (vt[0] / ts, vt[1] / ts) if ts > 1 else (1.0, 0.0)
    side = (-fwd[1], fwd[0])
    nf, ns, nb = split_groups(16, ts / max(ts + vn, 1e-4))
    out = []
    for name, num in (("Forward", nf), ("Side", ns), ("Back", nb)):
        spread, e0, e1, s0, s1, slide, heading = GROUPS[name]
        for i in range(num):
            fan = spread * ((i + stream.frand()) / num * 2 - 1)
            h = -heading if name == "Side" and i % 2 == 1 else heading
            az = math.radians(h + fan)
            el = math.radians(min(max(stream.range(e0, e1), 0), 89))
            speed = launch * stream.range(s0, s1)
            radius = (0.12 + 0.33 * stream.frand() ** 2) * ball_radius
            inplane = (fwd[0] * math.cos(az) + side[0] * math.sin(az), fwd[1] * math.cos(az) + side[1] * math.sin(az))
            v = [inplane[0] * math.sin(el) * speed + fwd[0] * slide * ts,
                 inplane[1] * math.sin(el) * speed + fwd[1] * slide * ts,
                 math.cos(el) * speed]
            m = math.sqrt(sum(c * c for c in v))
            if m > 900:
                v = [c * 900 / m for c in v]
            out.append({"group": name, "v": v, "r": radius})
    out.sort(key=lambda d: -d["r"])
    return out


# ---------------------------------------------------------------- section A

def a1_directions(path):
    s = Svg("A1 방향 분류")
    s.heading("방향 6개: 노멀의 지배축(dominant axis), 동률이면 X > Y > Z")
    # a slice of the unit sphere in the X-Z plane, each sample colored by its class
    cx, cy, r = 430, 510, 270
    for i in range(720):
        a = 2 * math.pi * i / 720
        n = (math.cos(a), 0.0, math.sin(a))
        c = DIRECTION_COLORS[dominant(n)]
        x1, y1 = cx + r * 0.78 * math.cos(a), cy - r * 0.78 * math.sin(a)
        x2, y2 = cx + r * math.cos(a), cy - r * math.sin(a)
        s.line(x1, y1, x2, y2, stroke=c, sw=3)
    s.line(cx - r - 30, cy, cx + r + 30, cy, stroke=MUTED, sw=1.5)
    s.line(cx, cy + r + 30, cx, cy - r - 30, stroke=MUTED, sw=1.5)
    for name, a in (("Front +X", 0), ("Up +Z", 90), ("Back −X", 180), ("Down −Z", 270)):
        x, y = cx + (r + 70) * math.cos(math.radians(a)), cy - (r + 70) * math.sin(math.radians(a))
        s.text(x, y + 8, name, size=26, anchor="middle", weight="bold")
    for a in (45, 135, 225, 315):
        x, y = cx + r * 1.05 * math.cos(math.radians(a)), cy - r * 1.05 * math.sin(math.radians(a))
        s.circle(x, y, 7, fill=GOLD, stroke="none")
    s.text(40, 110, "X-Z 단면, 금색 점 = 45°: 동률이면 X가 이김", size=24, fill=MUTED)

    # a box: each face projected along its own axis into one island
    ox, oy = 1000, 560
    k = 150

    def iso(x, y, z):
        return (ox + (x - y) * k * 0.87, oy + (x + y) * k * 0.5 - z * k)
    faces = {
        "Up": [(0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1)],
        "Front": [(1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1)],
        "Right": [(0, 1, 0), (1, 1, 0), (1, 1, 1), (0, 1, 1)],
    }
    for name, quad in faces.items():
        s.polygon([iso(*p) for p in quad], fill=DIRECTION_COLORS[name], stroke=INK, sw=2.5, opacity=0.9)
    s.text(*iso(0.5, 0.5, 1.05), "Up", size=28, anchor="middle", weight="bold", fill="white")
    fx, fy = iso(1.0, 0.5, 0.45)
    s.text(fx + 10, fy, "Front", size=28, anchor="middle", weight="bold", fill="white")
    rx, ry = iso(0.5, 1.0, 0.45)
    s.text(rx - 10, ry, "Right", size=28, anchor="middle", weight="bold", fill="white")
    s.text(1180, 830, "면마다 자기 축으로 평면 투영 → island", size=26, anchor="middle")
    s.save(path)


def shelf_pack(sizes):
    """PaintIslandLayout's shelves: tallest first, left to right, a new shelf on overflow"""
    order = sorted(range(len(sizes)), key=lambda i: -sizes[i][1])
    atlas = MIN_ATLAS
    while True:
        x = y = shelf = 0
        rects = {}
        ok = True
        for i in order:
            w, h = sizes[i]
            if x + w > atlas:
                x, y, shelf = 0, y + shelf, 0
            if w > atlas or y + h > atlas:
                ok = False
                break
            rects[i] = (x, y, w, h)
            x += w
            shelf = max(shelf, h)
        if ok:
            return atlas, rects
        atlas *= 2


def a2_atlas(path):
    s = Svg("A2 아틀라스")
    s.heading("방향마다 island 하나, shelf packing으로 정사각 아틀라스 한 장")
    # the crate, 300 cm, Up + Front + Right at 0.5 cm/texel and pad 13
    side = int(300 / TEXEL_CM) + 2 * PAD
    names = ["Up", "Front", "Right"]
    atlas, rects = shelf_pack([(side, side)] * 3)
    x0, y0, size = 80, 130, 640
    k = size / atlas
    s.rect(x0, y0, size, size, fill="#222222", stroke=INK)
    for i, (x, y, w, h) in rects.items():
        s.rect(x0 + x * k, y0 + y * k, w * k, h * k, fill=DIRECTION_COLORS[names[i]], stroke="white", sw=2)
        s.rect(x0 + (x + PAD) * k, y0 + (y + PAD) * k, (w - 2 * PAD) * k, (h - 2 * PAD) * k, stroke="white", sw=1, dash="6 4")
        s.text(x0 + (x + w / 2) * k, y0 + (y + h / 2) * k + 10, names[i], size=26, anchor="middle", fill="white", weight="bold")
    s.text(x0, y0 + size + 44, f"crate 300 cm: island {side}² texel (pad {PAD}) → atlas {atlas}", size=24)

    # the floor's texel grows once a side no longer fits 2048
    p = Plot(s, 960, 160, 560, 520, (0, 2400), (0, 1.4))
    p.axes("정사각 표면 한 변 (cm)", "texel (cm)", xticks=(0, 600, 1200, 1800, 2400), yticks=(0, 0.5, 1.0))
    p.curve(texel_for, 1, 2400, stroke=MINT, sw=4)
    knee = (MAX_ATLAS - 2 * PAD) * TEXEL_CM
    kx, ky = p.px(knee, TEXEL_CM)
    s.circle(kx, ky, 8, fill=GOLD, stroke="none")
    s.text(kx - 10, ky - 20, f"{knee:.0f} cm에서 2048 가득", size=22, anchor="end")
    fx, fy = p.px(2000, texel_for(2000))
    s.circle(fx, fy, 8, fill=CHOCO, stroke="none")
    s.text(fx - 12, fy - 18, f"floor 2000 cm → {texel_for(2000):.3f} cm", size=22, anchor="end")
    s.save(path)


def a3_texel(path):
    s = Svg("A3 텍셀")
    s.heading("RGBA8 한 텍셀 = 팀 id + 세대 · 높이 · 경계까지 거리")
    x0, y0, cw = 120, 170, 160
    bits = [("id", 3, MINT), ("gen (StarGen)", 5, GOLD)]
    s.text(x0, y0 - 20, "R", size=34, weight="bold")
    x = x0 + 50
    for name, n, color in bits:
        w = n * cw / 1.4
        s.rect(x, y0, w, 90, fill=color, stroke=INK, sw=2)
        for b in range(1, n):
            s.line(x + b * w / n, y0, x + b * w / n, y0 + 90, stroke="white", sw=1)
        s.text(x + w / 2, y0 + 56, f"{name} · {n} bit", size=26, anchor="middle", weight="bold", fill="white")
        x += w
    s.text(x + 30, y0 + 40, "R = id | gen << 3", size=26, font=MONO)
    s.text(x + 30, y0 + 78, "id 0~3 팀, 7 = 빈칸/지우개", size=24, fill=MUTED)
    rows = [("G", "높이: 0 ~ 1 (1.0 = 9 cm), 팀과 무관하게 누적", BLUE),
            ("B", "1 − d / 4: 자기 영역 경계까지 texel 거리", RED),
            ("A", "사용 안 함", GRID)]
    for i, (ch, label, color) in enumerate(rows):
        y = y0 + 150 + i * 120
        s.text(x0, y + 56, ch, size=34, weight="bold")
        s.rect(x0 + 50, y, 560, 90, fill=color, stroke=INK, sw=2, opacity=0.85)
        s.text(x0 + 640, y + 56, label, size=26)
    s.text(x0, 820, "clear 값 (7/255, 0, 0): id 7 = 아무 팀도 없음", size=26, font=FONT, fill=MUTED)
    s.save(path)


def a4_stamp_space(path):
    s = Svg("A4 스탬프 공간")
    s.heading("월드 → 스탬프 공간: uv = (U / RS, V / R), n = N / R")
    cx, cy, R, S = 420, 470, 200, 1.6
    s.ellipse(cx, cy, R * S, R, fill=MINT_LIGHT, stroke=MINT, sw=3)
    s.ellipse(cx, cy, R * S * 0.5, R * 0.5, stroke=MINT, sw=2, dash="8 6")
    s.arrow(cx, cy, cx + R * S + 60, cy, stroke=INK)
    s.arrow(cx, cy, cx, cy - R - 60, stroke=INK)
    s.text(cx + R * S + 70, cy + 8, "U (AxisU)", size=26)
    s.text(cx + 10, cy - R - 70, "V", size=26)
    s.text(cx + R * S * 0.5 + 10, cy - R * 0.5 - 10, "|uv| = 0.5: 본체", size=22, fill=MINT)
    s.text(cx, cy + R + 50, "R·S", size=24, anchor="middle")
    # the ellipsoid cut: a slice at depth n keeps sqrt(1 - n^2) of the disc
    p = Plot(s, 1080, 170, 440, 420, (0, 1), (0, 1.05))
    p.axes("|n| = |N| / R (표면 밖 거리)", "남는 반지름 비율", xticks=(0, 0.5, 1), yticks=(0, 0.5, 1))
    p.curve(lambda n: math.sqrt(max(1 - n * n, 0)), 0, 1, stroke=CHOCO, sw=4)
    for n in (0.5, 0.8):
        px, py = p.px(n, math.sqrt(1 - n * n))
        s.circle(px, py, 7, fill=GOLD, stroke="none")
        s.text(px + 12, py - 10, f"n {n}: {math.sqrt(1 - n * n):.2f}", size=22)
    s.text(1240, 720, "타원체로 잘라서 모서리를 감싸며 줄어듦", size=24, anchor="middle")
    s.text(1240, 760, "|n| ≥ 1 이면 스탬프 밖", size=24, anchor="middle", fill=MUTED)
    s.save(path)


def a5_build_splat(path):
    s = Svg("A5 BuildSplat")
    s.heading("BuildSplat: 입사각이 늘림·중심 이동·ImpactU를 정한다")
    theta = 50.0
    S = stretch_of(theta)
    R = 110
    gx, gy = 420, 560
    s.line(80, gy, 820, gy, stroke=INK, sw=3)
    t = math.radians(theta)
    s.arrow(gx - 330 * math.sin(t), gy - 330 * math.cos(t), gx, gy, stroke=CHOCO, sw=4)
    s.text(gx - 330 * math.sin(t) - 10, gy - 330 * math.cos(t) - 14, "공의 진행 방향", size=24, anchor="middle")
    s.line(gx, gy, gx, gy - 260, stroke=MUTED, sw=2, dash="6 6")
    s.text(gx - 40, gy - 150, f"θ {theta:.0f}°", size=26, anchor="end")
    shift = R * (S - 1) / 2
    s.ellipse(gx + shift, gy + 70, R * S, R * 0.45, fill=MINT_LIGHT, stroke=MINT, sw=3)
    s.circle(gx, gy + 70, 8, fill=GOLD, stroke="none")
    s.circle(gx + shift, gy + 70, 8, fill=MINT, stroke="none")
    s.arrow(gx, gy + 150, gx + shift, gy + 150, stroke=INK, sw=2)
    s.text(gx + shift / 2, gy + 185, f"R(S−1)/2 = {shift / R:.2f}R", size=22, anchor="middle")
    s.text(gx - 10, gy + 115, "충돌점", size=20, anchor="end")
    s.text(80, 840, f"S = 1/cosθ = {S:.2f}   ImpactU = −(S−1)/(2S) = {-(S - 1) / (2 * S):.3f}", size=24, font=MONO)
    p = Plot(s, 1000, 170, 500, 420, (0, 80), (1, 3.2))
    p.axes("입사각 θ (°)", "Stretch S", xticks=(0, 20, 40, 60, 80), yticks=(1, 2, 3))
    p.curve(lambda d: stretch_of(d), 0, 80, stroke=CHOCO, sw=4)
    p.curve(lambda d: min(1 / max(math.cos(math.radians(d)), 1e-4), 3.2), 0, 72, stroke=MUTED, sw=2, dash="6 6")
    cx, cy = p.px(math.degrees(math.acos(1 / 3)), 3)
    s.text(cx - 6, cy - 14, "MaxStretch 3에서 고정", size=20, anchor="end")
    s.save(path)


def a6_sdf(path, stills):
    s = Svg("A6 원에서 진짜 스플랫까지")
    s.heading("SDF 단계 0 → 8: 원 하나에서 MintChoco 스탬프까지")
    names = ["원 |p| − 0.5", "타원 + 타원체 절단", "wobble", "해바라기 (hard min)",
             "시드 위성", "spike + tear", "smooth-min", "crinkle", "deposit = 최종"]
    for k in range(9):
        x = 40 + (k % 5) * 300
        y = 90 + (k // 5) * 330
        href = Path(stills, f"stage{k}.png")
        if href.exists():
            s.image(x, y, 280, 280, href)
        else:
            s.rect(x, y, 280, 280, fill="#222222")
        s.text(x + 140, y + 310, f"{k}  {names[k]}", size=21, anchor="middle")
    # min against smooth-min across two discs
    p = Plot(s, 1240, 450, 320, 270, (-1.0, 1.0), (-0.35, 0.6))
    d1 = lambda x: abs(x + 0.35) - 0.3
    d2 = lambda x: abs(x - 0.35) - 0.3
    # k exaggerated from the stamp's 0.07 so the rounding shows at this size
    k = 0.3

    def smin(a, b):
        h = max(k - abs(a - b), 0) / k
        return min(a, b) - h * h * k * 0.25
    p.curve(lambda x: min(d1(x), d2(x)), -1, 1, stroke=MUTED, sw=6)
    p.curve(lambda x: smin(d1(x), d2(x)), -1, 1, stroke=MINT, sw=3)
    zx, zy = p.px(-1, 0)
    s.line(zx, zy, zx + 320, zy, stroke=INK, sw=1)
    s.text(1570, 790, "회색 min · 민트 smooth-min", size=20, anchor="end")
    s.text(1570, 826, "(k 과장)", size=20, anchor="end", fill=MUTED)
    s.save(path)


def a7_brush(path):
    s = Svg("A7 사각형 경로 브러시")
    s.heading("브러시: 스탬프가 닿는 사각형만 그린다 → scratch → 복사")
    ax, ay, size = 70, 140, 420
    s.rect(ax, ay, size, size, fill="#222222")
    islands = [(0, 0, 0.47, 0.47, BLUE), (0.5, 0, 0.47, 0.47, RED), (0, 0.5, 0.47, 0.47, GREEN)]
    for x, y, w, h, c in islands:
        s.rect(ax + x * size, ay + y * size, w * size, h * size, fill=c, stroke="white", sw=1.5, opacity=0.85)
    stamp = [(0.30, 0.12, 0.17, 0.26), (0.5, 0.12, 0.12, 0.26)]
    for x, y, w, h in stamp:
        s.rect(ax + x * size, ay + y * size, w * size, h * size, fill="none", stroke=GOLD, sw=4)
    s.text(ax + size / 2, ay + size + 40, "paint RT: island 3개, 사각형 2개", size=22, anchor="middle")
    steps = [("① fs_brush", "이전 paint를 읽어\nscratch에 씀", 600), ("② fs_copy", "같은 사각형만\npaint로 복사", 1060)]
    for title, sub, x in steps:
        s.rect(x, 230, 360, 240, fill=CHOCO_LIGHT, stroke=CHOCO, sw=2.5, rx=16)
        s.text(x + 180, 300, title, size=30, anchor="middle", weight="bold")
        for i, line in enumerate(sub.split("\n")):
            s.text(x + 180, 360 + i * 36, line, size=24, anchor="middle")
    s.arrow(ax + size + 20, 350, 590, 350, stroke=INK)
    s.arrow(965, 350, 1050, 350, stroke=INK)
    s.text(800, 640, "블렌드 없음 · 비용은 아틀라스가 아니라 스탬프 넓이에 비례", size=26, anchor="middle")
    s.text(800, 690, "RT를 읽으면서 같은 RT에 쓸 수 없어서 scratch를 거친다", size=24, anchor="middle", fill=MUTED)
    s.save(path)


def a8_cells(path):
    s = Svg("A8 셀 점령")
    s.heading("점수: 25 cm 셀 중심이 코어 타원체 (0.5R·S, 0.5R, R) 안이면 점령")
    cell = 25.0
    R, S = 93.0, 1.166
    cx, cy = 10.0, 5.0
    k = 3.0
    ox, oy = 150, 120
    ext = 300
    for i in range(int(ext / cell)):
        for j in range(int(ext / cell)):
            x = -ext / 2 + (i + 0.5) * cell
            y = -ext / 2 + (j + 0.5) * cell
            u = (x - cx) / (0.5 * R * S)
            v = (y - cy) / (0.5 * R)
            inside = u * u + v * v <= 1.0
            px, py = ox + (x + ext / 2 - cell / 2) * k, oy + (y + ext / 2 - cell / 2) * k
            s.rect(px, py, cell * k, cell * k, fill=MINT if inside else "white", stroke=GRID, sw=1, opacity=0.75 if inside else 1)
            s.circle(px + cell * k / 2, py + cell * k / 2, 3, fill=INK if inside else MUTED, stroke="none")
    ex, ey = ox + (cx + ext / 2) * k, oy + (cy + ext / 2) * k
    s.ellipse(ex, ey, R * S * k, R * k, stroke=MINT, sw=2, dash="8 6")
    s.ellipse(ex, ey, 0.5 * R * S * k, 0.5 * R * k, stroke=INK, sw=3)
    s.text(ox + ext * k / 2, oy + ext * k + 50, "표면 단면 · 실선: 코어 타원체 · 점선: 그림 스탬프 R", size=22, anchor="middle")
    # Sutherland-Hodgman: a triangle clipped to one voxel column gives the exact area
    bx, by = 1130, 230
    s.text(1110, 170, "셀 면적: 삼각형을 셀로 자르기", size=22)
    s.text(1110, 200, "(Sutherland-Hodgman)", size=20, fill=MUTED)
    tri = [(0.1, 0.9), (1.3, 0.2), (0.8, 1.4)]
    q = 300

    def clip(poly, edge, keep):
        out = []
        for i in range(len(poly)):
            a, b = poly[i], poly[(i + 1) % len(poly)]
            ia, ib = keep(a), keep(b)
            if ia:
                out.append(a)
            if ia != ib:
                t = edge(a, b)
                out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
        return out
    poly = tri
    poly = clip(poly, lambda a, b: (0 - a[0]) / (b[0] - a[0]), lambda p: p[0] >= 0)
    poly = clip(poly, lambda a, b: (1 - a[0]) / (b[0] - a[0]), lambda p: p[0] <= 1)
    poly = clip(poly, lambda a, b: (0 - a[1]) / (b[1] - a[1]), lambda p: p[1] >= 0)
    poly = clip(poly, lambda a, b: (1 - a[1]) / (b[1] - a[1]), lambda p: p[1] <= 1)
    s.rect(bx, by, q, q, stroke=INK, sw=2)
    s.polygon([(bx + x * q, by + y * q) for x, y in tri], fill="none", stroke=CHOCO, sw=2)
    s.polygon([(bx + x * q, by + y * q) for x, y in poly], fill=MINT, stroke=INK, sw=2.5, opacity=0.8)
    area = 0.5 * abs(sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1] for i in range(len(poly))))
    s.text(bx + q / 2, by + 1.4 * q + 46, f"셀 안 넓이 {area:.3f} (셀 = 1)", size=22, anchor="middle")
    s.text(1110, 830, "RT를 읽지 않는다:", size=24, weight="bold", fill=CHOCO)
    s.text(1110, 866, "같은 스플랫을 CPU가 따로 계산", size=24, weight="bold", fill=CHOCO)
    s.save(path)


def a9_edge(path):
    s = Svg("A9 팀 경계")
    s.heading("팀 경계: 코너 4 texel의 ±d를 bilerp → 0을 지나는 곳이 경계")
    p = Plot(s, 140, 170, 640, 440, (-2.0, 2.0), (-0.2, 1.2))
    p.axes("경계를 가로지르는 거리 (texel)", "coverage", xticks=(-2, -1, 0, 1, 2), yticks=(0, 0.5, 1))
    p.curve(lambda x: 1.0 if math.floor(x + 0.5) >= 0.5 else 0.0, -2, 2, n=800, stroke=MUTED, sw=3, dash="8 6")
    w = 0.25

    def sstep(e0, e1, x):
        t = min(max((x - e0) / (e1 - e0), 0), 1)
        return t * t * (3 - 2 * t)
    p.curve(lambda x: sstep(-w, w, x - 0.18), -2, 2, n=400, stroke=MINT, sw=4)
    s.text(460, 740, "점선: nearest (계단) · 민트: smoothstep(−w, w, sd), w ≤ ½ texel", size=22, anchor="middle")
    # four corners and their signed distances
    gx, gy, q = 1000, 200, 220
    vals = [(+0.9, -0.3), (+0.5, -0.8)]
    for j in range(2):
        for i in range(2):
            v = vals[j][i]
            x, y = gx + i * q, gy + j * q
            s.circle(x, y, 26, fill=MINT if v > 0 else CHOCO_LIGHT, stroke=INK, sw=2)
            s.text(x, y + 9, f"{v:+.1f}", size=22, anchor="middle", weight="bold")
    s.rect(gx, gy, q, q, stroke=INK, sw=2, dash="6 6")
    # the bilinear zero set inside the quad
    pts = []
    for k in range(41):
        fy = k / 40
        a = vals[0][0] + (vals[1][0] - vals[0][0]) * fy
        b = vals[0][1] + (vals[1][1] - vals[0][1]) * fy
        if (a > 0) != (b > 0):
            fx = a / (a - b)
            pts.append((gx + fx * q, gy + fy * q))
    s.polyline(pts, stroke=RED, sw=4)
    s.text(gx + q / 2, gy + q + 70, "빨강: bilinear sd = 0 (서브텍셀 경계)", size=22, anchor="middle")
    s.save(path)


def a10_blend(path):
    s = Svg("A10 팀 블렌드")
    s.heading("팀 블렌드: α = cov / (1 − S) 는 이음매로 바닥이 새지 않는다")
    ts = [i / 8 for i in range(9)]
    for col, (title, consumed) in enumerate((("순진한 lerp", False), ("consumed coverage", True))):
        x0 = 140 + col * 720
        s.text(x0 + 280, 140, title, size=28, anchor="middle", weight="bold")
        for k, t in enumerate(ts):
            cm, cc = 1 - t, t
            base = 1.0
            mint = choco = 0.0
            S = 0.0
            for team, c in ((0, cm), (1, cc)):
                a = c if not consumed else min(max(c / max(1 - S, 1e-4), 0), 1)
                base, mint, choco = base * (1 - a), mint * (1 - a) + (a if team == 0 else 0), choco * (1 - a) + (a if team == 1 else 0)
                S = min(S + c, 1)
            y = 600
            h = 380
            bx = x0 + k * 64
            parts = [(base, "#BDB6B0"), (mint, MINT), (choco, CHOCO)]
            top = y
            for share, color in parts:
                s.rect(bx, top - share * h, 50, share * h, fill=color, stroke="none")
                top -= share * h
            s.text(bx + 25, y + 30, f"{t:.2f}", size=18, anchor="middle", fill=MUTED)
        s.text(x0 + 280, 680, "이음매를 가로지름 (초코 coverage)", size=22, anchor="middle")
    s.text(800, 760, "회색 = 바닥이 비쳐 보이는 몫", size=24, anchor="middle", fill=MUTED)
    s.text(800, 806, "대신 뒤 팀이 이음매를 다 가져가 경계가 단단해진다", size=24, anchor="middle", fill=MUTED)
    s.save(path)


def a11_height(path):
    s = Svg("A11 높이 누적")
    s.heading("높이 G = saturate(G + kernel · HeightAdd), 1 = 9 cm")
    p = Plot(s, 140, 170, 560, 440, (0, 5), (0, 1.1))
    p.axes("같은 자리 발 수", "G", xticks=(0, 1, 2, 3, 4, 5), yticks=(0, 0.35, 0.7, 1.0))
    pts = []
    g = 0.0
    for shot in range(6):
        x, y = p.px(shot, g)
        pts.append((x, y))
        s.circle(x, y, 8, fill=MINT, stroke="none")
        s.text(x + 10, y - 14, f"{g:.2f}", size=20)
        g = min(g + 0.35, 1.0)
    s.polyline(pts, stroke=MINT, sw=3)
    q = Plot(s, 940, 170, 560, 440, (-1.2, 1.2), (0, 1.1))
    q.axes("스탬프 중심에서 (d / 경계)", "kernel", xticks=(-1, 0, 1), yticks=(0, 0.5, 1))

    def kernel(x):
        d = abs(x) - 1.0
        t = min(max(-d / 0.25, 0), 1)
        return t * t * (3 - 2 * t)
    q.curve(kernel, -1.2, 1.2, stroke=CHOCO, sw=4)
    q.curve(lambda x: kernel(x) * (1 + 0.2 * math.sin(9 * x)), -1.2, 1.2, stroke=GOLD, sw=2, dash="6 5")
    s.text(1220, 740, "가장자리 0.25 구간에서 0 → 1, 얼룩 ±20%", size=22, anchor="middle")
    s.save(path)


def a12_bspline(path):
    s = Svg("A12 B-spline")
    s.heading("높이 읽기: 12×12 cubic B-spline, 기울기도 같은 탭에서 해석적으로")
    p = Plot(s, 140, 170, 640, 460, (-2.6, 2.6), (-0.75, 0.75))
    p.axes("texel 오프셋 / s", "가중치", xticks=(-2, -1, 0, 1, 2), yticks=(-0.5, 0, 0.5))
    p.curve(bspline, -2.6, 2.6, stroke=MINT, sw=4)
    p.curve(bspline_slope, -2.6, 2.6, stroke=CHOCO, sw=3, dash="8 6")
    s.text(460, 760, "민트: B(x) · 점선: B′(x)", size=22, anchor="middle")
    # the window: 12 taps per axis, the kernel widened by s = clamp(1 + 1.5·flow, 1, 2.5)
    gx, gy, cs = 960, 170, 36
    flow = 1.0
    width = min(max(1 + 1.5 * flow, 1), 2.5)
    for j in range(12):
        for i in range(12):
            ox = (i - 5 - 0.3) / width
            oy = (j - 5 - 0.4) / width
            wgt = bspline(ox) * bspline(oy)
            shade = min(wgt / 0.44, 1)
            s.rect(gx + i * cs, gy + j * cs, cs - 2, cs - 2, fill=MINT, stroke="none", opacity=0.08 + 0.92 * shade)
    s.text(gx + 6 * cs, gy + 12 * cs + 46, f"flow 1 → s = {width:.1f}: 12×12 탭 고정 (unroll)", size=22, anchor="middle")
    s.save(path)


def a13_normal(path):
    s = Svg("A13 높이 → 노멀")
    s.heading("노멀 = cross(Pu + hu·n, Pv + hv·n)")
    ox, oy = 520, 560
    k = 260

    def iso(x, y, z):
        return (ox + (x - y) * k * 0.87, oy + (x + y) * k * 0.5 - z * k)
    s.polygon([iso(0, 0, 0), iso(1.2, 0, 0), iso(1.2, 1.2, 0), iso(0, 1.2, 0)], fill="#F1EEEA", stroke=GRID)
    hu, hv = 0.45, -0.25
    pu, pv, n = (1, 0, 0), (0, 1, 0), (0, 0, 1)
    a = (pu[0], pu[1], hu)
    b = (pv[0], pv[1], hv)
    c = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
    m = math.sqrt(sum(x * x for x in c))
    c = tuple(x / m for x in c)
    o = iso(0, 0, 0)
    for vec, color, label in ((pu, MUTED, "Pu"), (pv, MUTED, "Pv"), (a, MINT, "Pu + hu·n"), (b, MINT, "Pv + hv·n"), (n, INK, "n"), (c, RED, "노멀")):
        e = iso(*vec)
        s.arrow(o[0], o[1], e[0], e[1], stroke=color, sw=4 if color != MUTED else 2.5, dash="8 6" if color == MUTED else None)
        below = 34 if color == MUTED else -8
        # the v pair points the same way: its tilted label goes to the left
        left = label.startswith("Pv +")
        s.text(e[0] - 16 if left else e[0] + 12, e[1] + below, label, size=24,
               fill=color if color != MUTED else TEXT, weight="bold", anchor="end" if left else "start")
    s.text(1250, 300, "Pu, Pv: PositionMap 중앙차분", size=26, anchor="middle")
    s.text(1250, 350, "hu, hv: B-spline 기울기 × 9 cm", size=26, anchor="middle")
    s.text(1250, 400, "island 경계에서는 EdgeFade로 0", size=26, anchor="middle")
    s.text(1250, 450, "이웃이 다른 면이면 정점 노멀", size=26, anchor="middle", fill=MUTED)
    s.save(path)


def a14_lobes(path):
    s = Svg("A14 BSDF 로브")
    s.heading("BSDF 로브: 본체 slab (diffuse · GGX · haze · fuzz) 위에 coat slab")
    cx, cy, r = 470, 640, 420
    light = math.radians(40)
    s.line(cx - r - 20, cy, cx + r + 20, cy, stroke=INK, sw=2)
    s.arrow(cx - r * 0.8 * math.sin(light), cy - r * 0.8 * math.cos(light), cx, cy, stroke=GOLD, sw=3)
    s.text(cx - r * 0.8 * math.sin(light) - 10, cy - r * 0.8 * math.cos(light) - 10, "빛", size=24, anchor="end")

    def lobe(f, color, label, scale, dash=None):
        # each lobe over its own peak: the shapes compare, not the energies
        samples = []
        for i in range(181):
            v = math.radians(-90 + i)
            hvec = (math.sin(v) - math.sin(light), math.cos(v) + math.cos(light))
            noh = hvec[1] / math.hypot(*hvec)
            samples.append((v, f(noh, math.cos(v))))
        peak = max(val for _, val in samples) or 1.0
        pts = [(cx + val / peak * scale * r * math.sin(v), cy - val / peak * scale * r * math.cos(v)) for v, val in samples]
        s.polyline(pts, stroke=color, sw=3.5, dash=dash)
        return pts
    lobe(lambda noh, nov: max(nov, 0.0), "#BDB6B0", "diffuse", 0.45)
    lobe(lambda noh, nov: ggx(0.62, noh), MINT, "GGX", 0.8)
    lobe(lambda noh, nov: ggx(0.9, noh), BLUE, "haze", 0.65, dash="8 6")
    lobe(lambda noh, nov: charlie(0.7, noh), CHOCO, "fuzz", 0.6)
    lobe(lambda noh, nov: ggx(0.12, noh), RED, "coat", 0.95)
    for i, (label, color) in enumerate((("diffuse", "#BDB6B0"), ("GGX rough 0.62", MINT), ("haze 2nd lobe 0.9", BLUE), ("fuzz (Charlie)", CHOCO), ("coat rough 0.12", RED))):
        s.rect(80, 120 + i * 40, 26, 14, fill=color, stroke="none")
        s.text(116, 134 + i * 40, label, size=22)
    # the vertical layer: what the coat lets through
    p = Plot(s, 1080, 170, 420, 360, (0, 90), (0, 1.0))
    p.axes("시선 각도 (°)", "본체에 닿는 몫", xticks=(0, 45, 90), yticks=(0, 0.5, 1))
    for cw, color in ((0.04, MINT), (0.4, CHOCO), (1.0, RED)):
        def through(deg, cw=cw):
            f = 0.04 + 0.96 * (1 - math.cos(math.radians(deg))) ** 5
            return 1 - cw * f
        p.curve(through, 0, 89.5, stroke=color, sw=3)
    s.text(1290, 640, "(1 − cw·F(NoV))(1 − cw·F(NoL)), cw = coat × 0.4", size=20, anchor="middle")
    s.save(path)


def a15_collapse(path):
    s = Svg("A15 Blendable GBuffer")
    s.heading("Blendable GBuffer로 접히는 흐름 (UE 소스 기준 추정)")
    s.box(80, 170, 420, 120, "Substrate Vertical Layer", sub="coat slab / body slab")
    s.box(80, 360, 420, 120, "body slab", sub="SSS MFP · fuzz · haze")
    s.arrow(500, 300, 640, 360, stroke=INK)
    s.box(640, 290, 400, 140, "closure 1개", fill=MINT_LIGHT, stroke=MINT, sub="파라미터 블렌딩")
    s.arrow(1040, 360, 1160, 300, stroke=INK)
    s.arrow(1040, 360, 1160, 470, stroke=INK)
    s.box(1160, 230, 380, 120, "Cloth", sub="fuzz > 0 (민트)")
    s.box(1160, 420, 380, 120, "DefaultLit", sub="fuzz = 0 (초코, 바닥)")
    rows = ["coat → roughness·F0에 lerp로 섞임", "haze → 한 roughness로 lerp", "diffusion SSS → 사라짐", "레거시 셰이딩 모델로 export"]
    for i, r in enumerate(rows):
        s.text(120, 620 + i * 46, "· " + r, size=26)
    s.text(1500, 860, "캡처 검증은 범위 밖", size=22, anchor="end", fill=MUTED)
    s.save(path)


def a16_splash(path):
    s = Svg("A16 스플래시")
    s.heading("스플래시: 접촉 하나 → 물방울 16개, 점수와 그림이 일부러 다르다")
    v = (1500.0, 0.0, -2500.0)
    drops = droplets_for(v, 11035)
    counts = {g: sum(1 for d in drops if d["group"] == g) for g in GROUPS}
    colors = {"Forward": MINT, "Side": GOLD, "Back": MUTED}
    cx, cy = 420, 470
    s.circle(cx, cy, 10, fill=INK, stroke="none")
    for d in drops:
        vx, vy, vz = d["v"]
        s.arrow(cx, cy, cx + vx * 0.32, cy + vy * 0.32, stroke=colors[d["group"]], sw=2 + d["r"] / 2)
    s.arrow(cx - 300, cy, cx - 20, cy, stroke=CHOCO, sw=5)
    s.text(cx - 300, cy - 20, "공", size=24)
    for i, g in enumerate(("Forward", "Side", "Back")):
        s.rect(80, 680 + i * 44, 26, 16, fill=colors[g], stroke="none")
        s.text(116, 696 + i * 44, f"{g} {counts[g]}", size=24)
    s.text(cx, 870, "위에서 본 발사 방향 (굵기 = 크기)", size=22, anchor="middle")
    # side view: the four largest, drag-free (score) against drag 0.4 (picture)
    p = Plot(s, 940, 150, 580, 420, (0, 520), (0, 170))
    p.axes("앞으로 (cm)", "높이 (cm)", xticks=(0, 200, 400), yticks=(0, 50, 100, 150))
    g = 980.0
    for d in drops[:4]:
        vx = math.hypot(d["v"][0], d["v"][1]) * (1 if d["v"][0] >= 0 else -1)
        vz = d["v"][2]
        if vx < 0:
            continue
        tland = 2 * vz / g
        p.curve(lambda x, vx=vx, vz=vz: max(vz * (x / vx) - 0.5 * g * (x / vx) ** 2, 0), 0, vx * tland, stroke=GOLD, sw=3)
        k = 0.4
        pts = []
        t = 0.0
        while t < 2.0:
            e = math.exp(-k * t)
            x = vx * (1 - e) / k
            z = (vz + g / k) * (1 - e) / k - g / k * t
            if z < 0 and t > 0.05:
                break
            pts.append(p.px(x, max(z, 0)))
            t += 1 / 240
        s.polyline(pts, stroke=MINT, sw=3, dash="8 6")
    s.text(1230, 660, "노랑: 점수 (포물선, drag 없음, 큰 4개)", size=22, anchor="middle")
    s.text(1230, 700, "민트 점선: 그림 (drag 0.4, 큰 8개가 자국)", size=22, anchor="middle")
    s.text(1230, 740, "법선 속도 < 400 cm/s 이면 스플래시 없음", size=22, anchor="middle", fill=MUTED)
    s.save(path)


def a17_decal(path):
    s = Svg("A17 데칼 대신 RenderTarget2D")
    s.heading("왜 데칼이 아니라 RenderTarget2D인가")
    p = Plot(s, 130, 160, 520, 440, (0, 2000), (0, 1.1))
    p.axes("쌓인 스플랫 수", "매 프레임 비용 (상대)", xticks=(0, 500, 1000, 1500, 2000), yticks=(0, 0.5, 1))
    p.curve(lambda n: n / 2000, 0, 2000, stroke=RED, sw=4)
    p.curve(lambda n: 0.12, 0, 2000, stroke=MINT, sw=4)
    s.text(390, 700, "빨강: 데칼 (스플랫마다 투영 볼륨)", size=22, anchor="middle")
    s.text(390, 740, "민트: RT 샘플 한 번", size=22, anchor="middle")
    rows = [
        ("누적 높이", "이전 값을 읽고 더해야 함 → 텍셀에 G로 저장"),
        ("팀 경계", "텍셀마다 id + 거리 → signed distance"),
        ("모서리·곡면", "방향별 island에 투영 → 늘어짐 없음"),
        ("덮어쓰기", "나중 팀이 같은 텍셀을 바꿈, 지우개는 id 7"),
        ("비용", "스플랫 수와 무관, 메모리 고정"),
        ("점수", "CPU 셀 그리드가 따로 → RT는 그림 전용"),
    ]
    for i, (k, v) in enumerate(rows):
        y = 190 + i * 100
        s.rect(760, y - 46, 820, 84, fill=MINT_LIGHT if i % 2 == 0 else CHOCO_LIGHT, stroke="none", rx=10)
        s.text(780, y + 6, k, size=26, weight="bold")
        s.text(965, y + 6, v, size=23)
    s.save(path)


# ---------------------------------------------------------------- section B

def b1_what(path):
    s = Svg("B1 무엇을 복제하나")
    s.heading("무엇을 복제하나: 스플랫 이벤트만")
    rows = [("스플랫 이벤트 (FPaintSplat)", "복제", "FastArray 로그, 순서·유실 없음", MINT),
            ("커버리지 그리드", "복제 안 함", "각 머신이 이벤트로 다시 계산", CHOCO),
            ("WorldCoverage 요약", "0.2 s마다", "서버 그리드 합계만, KO 판정도 여기서", MINT),
            ("물방울 자국 (bDrawOnly)", "복제 안 함", "머신마다 로컬, 점수 없음", CHOCO),
            ("phantom 착지 (bScoreOnly)", "보내지 않음", "스플랫에서 각 머신이 똑같이 유도", GOLD)]
    for i, (what, how, why, color) in enumerate(rows):
        y = 130 + i * 140
        s.rect(80, y, 1440, 110, fill="white", stroke=GRID, rx=14)
        s.rect(80, y, 18, 110, fill=color, stroke="none")
        s.text(130, y + 66, what, size=30, weight="bold")
        s.text(820, y + 66, how, size=30, weight="bold", fill=color)
        s.text(1040, y + 66, why, size=24)
    s.save(path)


def b2_flow(path):
    s = Svg("B2 권한 흐름")
    s.heading("권한 흐름: 쏜 사람이 시드를 고르고 서버가 스플랫을 확정")
    boxes = [(60, "쏜 클라이언트", "시드 선택 → ServerFire"), (420, "서버", "라인트레이스 · BuildSplat"),
             (780, "SubmitSplat", "권한자만, LockGens 기록"), (1140, "스플랫 로그", "FastArray에 추가")]
    for x, title, sub in boxes:
        s.box(x, 200, 320, 150, title, sub=sub)
    for x in (380, 740, 1100):
        s.arrow(x, 275, x + 40, 275)
    for i, name in enumerate(("서버 자신", "클라이언트 A", "클라이언트 B")):
        x = 420 + i * 380
        s.box(x, 520, 340, 110, name, fill=MINT_LIGHT, stroke=MINT, size=24, sub="ApplySplat: RT + 셀 그리드")
        s.arrow(1300, 350, x + 170, 516)
    s.text(80, 740, "클라이언트는 히트를 보내지 않고 완성된 스플랫을 받는다", size=26)
    s.text(80, 790, "→ 모든 머신이 같은 FPaintLocalStamp", size=26, weight="bold", fill=CHOCO)
    s.save(path)


def b3_log(path):
    s = Svg("B3 로그와 늦은 입장")
    s.heading("스플랫 로그와 늦은 입장: 로그 전체를 다시 재생")
    x0, y0, cw = 120, 200, 110
    for i in range(12):
        s.rect(x0 + i * cw, y0, cw - 10, 80, fill=MINT_LIGHT if i < 9 else GOLD, stroke=MINT, rx=10)
        s.text(x0 + i * cw + (cw - 10) / 2, y0 + 50, f"#{i}", size=24, anchor="middle")
    s.text(x0, y0 - 24, "서버 순서 = 로그 순서 (줄어드는 건 ClearPaint뿐)", size=24)
    s.text(x0, 380, "기존 클라이언트: OnRep_SplatLog에서 [AppliedSplatCount, Num)만 적용", size=26)
    s.text(x0, 460, "늦게 들어온 클라이언트: 로그 전체를 표면별 큐로 재생", size=26)
    for t in range(4):
        s.rect(x0 + t * 340, 520, 300, 70, fill=CHOCO_LIGHT, stroke=CHOCO, rx=10)
        s.text(x0 + t * 340 + 150, 565, f"틱 {t}: 표면당 16개", size=24, anchor="middle")
    s.text(x0, 680, "한 프레임에 몰아서 그리지 않아 입장 순간 멈춤이 없음", size=24, fill=MUTED)
    s.save(path)


def b4_authority(path):
    s = Svg("B4 점수는 권한자 그리드만")
    s.heading("점수는 권한자의 커버리지 그리드만 유효")
    for i, (title, sub, color, fill) in enumerate((("서버 그리드", "점수 · KO 판정 · WorldCoverage", MINT, MINT_LIGHT),
                                                   ("클라이언트 그리드", "디버그 표시용으로 남겨 둠", CHOCO, CHOCO_LIGHT))):
        x = 120 + i * 720
        s.rect(x, 160, 640, 480, fill=fill, stroke=color, rx=18)
        s.text(x + 320, 220, title, size=32, anchor="middle", weight="bold")
        s.text(x + 320, 266, sub, size=24, anchor="middle")
        for j in range(8):
            for k in range(6):
                on = (j * 3 + k * 5 + i) % 4 != 0
                s.rect(x + 80 + j * 60, 320 + k * 46, 56, 42, fill=color if on else "white", stroke="white", opacity=0.75)
    s.text(800, 720, "같은 이벤트로 만든 그리드지만 클라이언트 값은 점수에 쓰지 않는다", size=26, anchor="middle")
    s.save(path)


def b5_quantize(path):
    s = Svg("B5 양자화")
    s.heading("서버는 원본 스플랫, 클라이언트는 양자화된 값을 적용")
    p = Plot(s, 160, 160, 600, 480, (1.5, 4.5), (2.5, 4.5))
    p.axes("x (cm)", "y (cm)", xticks=(2, 3, 4), yticks=(3, 4))
    raw = (2.37, 3.62)
    q = (round(raw[0]), round(raw[1]))
    rx, ry = p.px(*raw)
    qx, qy = p.px(*q)
    s.circle(rx, ry, 12, fill=MINT, stroke="none")
    s.circle(qx, qy, 12, fill=CHOCO, stroke="none")
    s.arrow(rx, ry, qx, qy, stroke=INK, sw=2)
    s.text(rx + 20, ry + 34, "서버: 원본 (2.37, 3.62)", size=22)
    s.text(qx + 20, qy - 20, "클라이언트: 1 cm 격자 (2, 4)", size=22)
    rows = ["Location: FVector_NetQuantize (1 cm)", "Normal · AxisU: NetQuantizeNormal (16 bit)",
            "IncidentSpeed u16, BallRadius u8, Seed u16", "→ 클라이언트 그리드가 조금 어긋나도 허용"]
    for i, r in enumerate(rows):
        s.text(880, 260 + i * 70, r, size=26, font=MONO if i < 3 else FONT, weight="bold" if i == 3 else "normal")
    s.save(path)


def b6_mismatch(path):
    s = Svg("B6 허용되는 불일치")
    s.heading("클라이언트 간 비주얼 불일치는 허용, 점수 불일치는 없음")
    for i, (title, items, color, fill) in enumerate((
            ("달라도 되는 것 (그림)", ["물방울 자국: 머신마다 Niagara 비행", "서버 원본 vs 클라이언트 양자화", "늦은 입장 중 재생 순서의 순간"], CHOCO, CHOCO_LIGHT),
            ("같아야 하는 것 (점수)", ["서버 커버리지 그리드", "phantom 착지: 스플랫에서 결정적", "0.2 s마다 복제되는 WorldCoverage"], MINT, MINT_LIGHT))):
        x = 100 + i * 720
        s.rect(x, 160, 680, 560, fill=fill, stroke=color, rx=18)
        s.text(x + 340, 230, title, size=32, anchor="middle", weight="bold")
        for j, it in enumerate(items):
            s.text(x + 50, 330 + j * 110, "· " + it, size=27)
    s.save(path)


def b7_locks(path):
    s = Svg("B7 잠금 세대")
    s.heading("잠금 세대(LockGens)를 스플랫에 박아서 보낸다")
    s.box(120, 200, 520, 160, "서버가 본 LockGens", sub="팀마다 1 byte, uint32 하나")
    s.arrow(640, 280, 780, 280)
    s.box(780, 200, 680, 160, "FPaintSplat.LockGens", sub="재생해도 같은 텍셀을 건너뜀")
    s.text(160, 480, "잠긴 다른 id의 텍셀은 덮지 않고, 같은 id는 자기 gen을 유지", size=26)
    s.text(160, 540, "늦게 들어온 클라이언트가 나중에 재생해도 그 순간의 잠금 그대로", size=26)
    s.text(160, 620, "→ 재생 시점의 시계가 아니라 이벤트에 담긴 값으로 결정", size=26, weight="bold", fill=CHOCO)
    s.save(path)


def b8_multicast(path):
    s = Svg("B8 멀티캐스트에서 로그로")
    s.heading("초기 설계: 신뢰성 없는 멀티캐스트 → 지금: 스플랫 로그")
    for i, (title, items, color, fill) in enumerate((
            ("NetMulticast, Unreliable", ["패킷 유실 → 머신마다 다른 그림과 점수", "순서 보장 없음 → 덮어쓰기 결과가 다름", "늦은 입장 → 이전 스플랫을 모름"], RED, "#FBEAE6"),
            ("FastArray 스플랫 로그", ["유실 없음 (복제 상태)", "서버 순서 그대로", "늦은 입장 = 로그 재생"], MINT, MINT_LIGHT))):
        x = 100 + i * 720
        s.rect(x, 160, 680, 520, fill=fill, stroke=color, rx=18)
        s.text(x + 340, 230, title, size=30, anchor="middle", weight="bold", font=MONO)
        for j, it in enumerate(items):
            s.text(x + 50, 330 + j * 100, "· " + it, size=27)
    s.arrow(790, 420, 810, 420, stroke=INK, sw=4)
    s.text(800, 760, "사이드 스플랫(transient)만 지금도 멀티캐스트: 이펙트일 뿐 점수 없음", size=24, anchor="middle", fill=MUTED)
    s.save(path)


FIGURES = {
    "A1": a1_directions, "A2": a2_atlas, "A3": a3_texel, "A4": a4_stamp_space,
    "A5": a5_build_splat, "A7": a7_brush, "A8": a8_cells, "A9": a9_edge,
    "A10": a10_blend, "A11": a11_height, "A12": a12_bspline, "A13": a13_normal,
    "A14": a14_lobes, "A15": a15_collapse, "A16": a16_splash, "A17": a17_decal,
    "B1": b1_what, "B2": b2_flow, "B3": b3_log, "B4": b4_authority,
    "B5": b5_quantize, "B6": b6_mismatch, "B7": b7_locks, "B8": b8_multicast,
}

EDGE = Path("C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe")


def rasterize(svg, png):
    subprocess.run([
        str(EDGE), "--headless", "--disable-gpu", "--hide-scrollbars",
        f"--screenshot={Path(png).resolve()}", f"--window-size={W},{H - TOP}", svg.resolve().as_uri(),
    ], check=True, capture_output=True, timeout=60)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("out")
    parser.add_argument("--stills", default=None, help="the shape stage stills, for A6")
    parser.add_argument("--png", action="store_true")
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    stills = Path(args.stills) if args.stills else out.parent / "stills" / "stills_shape_stages"
    made = []
    for name, make in FIGURES.items():
        path = out / f"{name}.svg"
        make(path)
        made.append(path)
    a6 = out / "A6.svg"
    a6_sdf(a6, stills)
    made.append(a6)
    if args.png:
        for svg in made:
            rasterize(svg, svg.with_suffix(".png"))
    print(f"{len(made)} figures in {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
