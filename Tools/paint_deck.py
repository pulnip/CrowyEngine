"""Builds the MintChoco paint deck (.pptx) from Tools/paint_deck.json.

    python Tools/paint_deck.py captures/paintlab/run1

Clips, stills and figures come from the run directory (paint_record.py and
paint_figures.py fill it). The build fails on a missing asset, a clip outside
4-6 s, or a banned word anywhere in the deck's text, notes or figures.
Needs python-pptx and Pillow (PYTHONPATH=captures/paintlab/pydeps).
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

from lxml import etree
from PIL import Image
from pptx import Presentation
from pptx.dml.color import RGBColor
from pptx.enum.text import MSO_ANCHOR, PP_ALIGN
from pptx.opc.constants import RELATIONSHIP_TYPE as RT
from pptx.opc.package import Part
from pptx.opc.packuri import PackURI
from pptx.oxml.ns import qn
from pptx.util import Emu, Inches, Pt

import paint_shots

ROOT = Path(__file__).resolve().parent.parent
FONT = "Malgun Gothic"
MONO = "Consolas"
INK = RGBColor(0x2B, 0x1B, 0x12)
BODY = RGBColor(0x2A, 0x22, 0x1D)
MUTED = RGBColor(0x6E, 0x65, 0x5F)
MINT = RGBColor(0x1F, 0xA8, 0x8E)
MINT_DARK = RGBColor(0x13, 0x7A, 0x67)
MINT_LIGHT = RGBColor(0xE2, 0xF4, 0xEF)
CHOCO = RGBColor(0x6B, 0x44, 0x30)
CHOCO_LIGHT = RGBColor(0xF3, 0xEC, 0xE6)
COVER_TAG = RGBColor(0x9F, 0xE3, 0xD3)
WHITE = RGBColor(0xFF, 0xFF, 0xFF)
# stems, so every inflection is caught
BANNED = ["read-back", "readback", "read back", "read_back", "리드백", "튕", "bounc", "바운스"]
SVG_EXT = "{96DAC541-7B7A-43D3-8B79-37D633B846F1}"
SVG_NS = "http://schemas.microsoft.com/office/drawing/2016/SVG/main"


def set_font(run, size, color=BODY, bold=False, name=FONT):
    run.font.size = Pt(size)
    run.font.bold = bold
    run.font.color.rgb = color
    run.font.name = name
    rpr = run._r.get_or_add_rPr()
    for tag in ("a:ea", "a:cs"):
        el = rpr.find(qn(tag))
        if el is None:
            el = etree.SubElement(rpr, qn(tag))
        el.set("typeface", name)


def text_box(slide, x, y, w, h, lines, size=13, color=BODY, bold=False, align=PP_ALIGN.LEFT,
             anchor=MSO_ANCHOR.TOP, name=FONT, spacing=None):
    box = slide.shapes.add_textbox(Emu(x), Emu(y), Emu(w), Emu(h))
    frame = box.text_frame
    frame.word_wrap = True
    frame.vertical_anchor = anchor
    frame.margin_left = frame.margin_right = 0
    frame.margin_top = frame.margin_bottom = 0
    if isinstance(lines, str):
        lines = lines.split("\n")
    for i, line in enumerate(lines):
        p = frame.paragraphs[0] if i == 0 else frame.add_paragraph()
        p.alignment = align
        if spacing:
            p.space_after = Pt(spacing)
        run = p.add_run()
        run.text = line
        set_font(run, size, color, bold, name)
    return box


def fill_rect(slide, x, y, w, h, color, line=None):
    from pptx.enum.shapes import MSO_SHAPE
    shape = slide.shapes.add_shape(MSO_SHAPE.RECTANGLE, Emu(x), Emu(y), Emu(w), Emu(h))
    shape.fill.solid()
    shape.fill.fore_color.rgb = color
    if line is None:
        shape.line.fill.background()
    else:
        shape.line.color.rgb = line
    shape.shadow.inherit = False
    return shape


def round_rect(slide, x, y, w, h, color):
    from pptx.enum.shapes import MSO_SHAPE
    shape = slide.shapes.add_shape(MSO_SHAPE.ROUNDED_RECTANGLE, Emu(x), Emu(y), Emu(w), Emu(h))
    shape.adjustments[0] = 0.12
    shape.fill.solid()
    shape.fill.fore_color.rgb = color
    shape.line.fill.background()
    shape.shadow.inherit = False
    return shape


def clip_seconds(path):
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)],
        capture_output=True, text=True, check=True,
    ).stdout
    return float(out.strip())


class Deck:
    def __init__(self, run_dir, spec):
        self.run = Path(run_dir).resolve()
        self.spec = spec
        self.prs = Presentation()
        self.prs.slide_width = Emu(9144000)
        self.prs.slide_height = Emu(5143500)
        self.blank = self.prs.slide_layouts[6]
        self.page = 0
        self.errors = []
        self.texts = []
        self.svg_count = 0
        self.video_count = 0

    # ---------------------------------------------------------------- assets

    def asset(self, ref):
        kind, _, name = ref.partition(":")
        if kind == "clip":
            path = self.run / "clips" / f"{name}.mp4"
            poster = self.run / "clips" / f"{name}.png"
            return path, poster
        if kind == "figure":
            return self.run / "figures" / f"{name}.svg", self.run / "figures" / f"{name}.png"
        if kind == "still":
            hits = list((self.run / "stills").rglob(f"{name}.png"))
            return (hits[0] if hits else self.run / "stills" / f"{name}.png"), None
        if kind == "file":
            return ROOT / name, None
        raise ValueError(f"unknown asset {ref}")

    def check_asset(self, ref):
        path, poster = self.asset(ref)
        if not path.exists():
            self.errors.append(f"missing {ref}: {path}")
            return False
        if poster is not None and not poster.exists():
            self.errors.append(f"missing poster of {ref}: {poster}")
            return False
        if ref.startswith("clip:"):
            seconds = clip_seconds(path)
            if not 4.0 <= seconds <= 6.0:
                self.errors.append(f"{ref} runs {seconds:.2f} s, outside 4-6 s")
        if ref.startswith("figure:"):
            svg = path.read_text(encoding="utf-8")
            self.texts.append((ref, " ".join(re.findall(r">([^<]+)</(?:text|title)>", svg))))
        return True

    def aspect(self, ref):
        path, poster = self.asset(ref)
        image = poster if poster is not None else path
        with Image.open(image) as im:
            return im.width / im.height

    # ---------------------------------------------------------------- media

    def place_media(self, slide, ref, x, y, w, h, caption=None):
        if not self.check_asset(ref):
            fill_rect(slide, x, y, w, h, CHOCO_LIGHT)
            return
        a = self.aspect(ref)
        if w / h > a:
            mw, mh = int(h * a), h
        else:
            mw, mh = w, int(w / a)
        mx, my = x + (w - mw) // 2, y + (h - mh) // 2
        path, poster = self.asset(ref)
        if ref.startswith("clip:"):
            slide.shapes.add_movie(str(path), Emu(mx), Emu(my), Emu(mw), Emu(mh),
                                   poster_frame_image=str(poster), mime_type="video/mp4")
            self.video_count += 1
        elif ref.startswith("figure:"):
            self.add_svg(slide, path, poster, mx, my, mw, mh)
        else:
            slide.shapes.add_picture(str(path), Emu(mx), Emu(my), Emu(mw), Emu(mh))
        if caption:
            self.texts.append((ref, caption))
            text_box(slide, x, my + mh + Emu(Inches(0.05)), w, Emu(Inches(0.3)), caption, size=10,
                     color=MUTED, align=PP_ALIGN.CENTER)

    def add_svg(self, slide, svg, png, x, y, w, h):
        """the PNG as the picture, the SVG beside it in the blip's extension list"""
        picture = slide.shapes.add_picture(str(png), Emu(x), Emu(y), Emu(w), Emu(h))
        self.svg_count += 1
        part = Part(PackURI(f"/ppt/media/paintfig{self.svg_count}.svg"), "image/svg+xml",
                    slide.part.package, svg.read_bytes())
        rid = slide.part.relate_to(part, RT.IMAGE)
        blip = picture._element.find(".//" + qn("a:blip"))
        ext_list = etree.SubElement(blip, qn("a:extLst"))
        ext = etree.SubElement(ext_list, qn("a:ext"))
        ext.set("uri", SVG_EXT)
        svg_blip = etree.SubElement(ext, f"{{{SVG_NS}}}svgBlip", nsmap={"asvg": SVG_NS})
        svg_blip.set(qn("r:embed"), rid)

    # ---------------------------------------------------------------- frames

    def new_slide(self, dark=False):
        slide = self.prs.slides.add_slide(self.blank)
        if dark:
            fill_rect(slide, 0, 0, self.prs.slide_width, self.prs.slide_height, INK)
        return slide

    def header(self, slide, kicker, title):
        self.texts.append(("title", f"{kicker} {title}"))
        text_box(slide, 457200, 256032, 6858000, 228600, kicker.upper(), size=10, color=MINT, bold=True)
        text_box(slide, 457200, 475488, 8229600, 548640, title, size=24, color=INK, bold=True)

    def footer(self, slide):
        self.page += 1
        text_box(slide, 8229600, 4754880, 457200, 182880, str(len(self.prs.slides._sldIdLst)), size=9,
                 color=MUTED, align=PP_ALIGN.RIGHT)

    def notes(self, slide, text):
        if text:
            self.texts.append(("notes", text))
            slide.notes_slide.notes_text_frame.text = text

    # ---------------------------------------------------------------- kinds

    def cover(self, s):
        slide = self.new_slide(dark=True)
        text_box(slide, 548640, 1280160, 8000000, 914400, s["title"], size=48, color=WHITE, bold=True)
        text_box(slide, 548640, 2331720, 8000000, 615206, s["subtitle"], size=18, color=WHITE)
        text_box(slide, 548640, 4114800, 8000000, 274320, s.get("tag", ""), size=12, color=COVER_TAG)
        self.texts.append(("cover", f"{s['title']} {s['subtitle']} {s.get('tag', '')}"))
        self.notes(slide, s.get("notes"))

    def section(self, s):
        slide = self.new_slide(dark=True)
        text_box(slide, 548640, 1600000, 8000000, 300000, s["kicker"].upper(), size=12, color=COVER_TAG, bold=True)
        text_box(slide, 548640, 1950000, 8000000, 800000, s["title"], size=36, color=WHITE, bold=True)
        if s.get("subtitle"):
            text_box(slide, 548640, 2900000, 8000000, 800000, s["subtitle"], size=15, color=WHITE)
        self.texts.append(("section", f"{s['title']} {s.get('subtitle', '')}"))
        self.notes(slide, s.get("notes"))

    def media(self, s):
        slide = self.new_slide()
        self.header(slide, s["kicker"], s["title"])
        items = s["media"]
        top, bottom = 1100000, 4650000
        left, right = 457200, 9144000 - 457200
        bullets = s.get("bullets")
        if bullets:
            split = int(left + (right - left) * s.get("split", 0.62))
            self.bullet_list(slide, bullets, split + 228600, top + 100000, right - split - 228600)
            right = split
        caption_room = 280000 if any(i.get("caption") for i in items) else 0
        gap = 200000
        w = (right - left - gap * (len(items) - 1)) // len(items)
        for k, item in enumerate(items):
            self.place_media(slide, item["ref"], left + k * (w + gap), top, w, bottom - top - caption_room,
                             item.get("caption"))
        self.footer(slide)
        self.notes(slide, s.get("notes"))

    def bullet_list(self, slide, bullets, x, y, w):
        for i, b in enumerate(bullets):
            head, _, rest = b.partition("|")
            self.texts.append(("bullet", b))
            text_box(slide, x, y + i * 560000, w, 260000, head.strip(), size=14, color=INK, bold=True)
            if rest.strip():
                text_box(slide, x, y + i * 560000 + 250000, w, 300000, rest.strip(), size=11, color=MUTED)

    def points(self, s):
        """the portfolio's principle rows: a colored chip, a head line, a muted line"""
        slide = self.new_slide()
        self.header(slide, s["kicker"], s["title"])
        rows = s["rows"]
        step = min(1143000, 3500000 // max(len(rows), 1))
        for i, row in enumerate(rows):
            y = 1200000 + i * step
            color = MINT if i % 2 == 0 else CHOCO
            chip = round_rect(slide, 457200, y, 777240, int(step * 0.8), color)
            text_box(slide, 457200, y, 777240, int(step * 0.8), row["chip"], size=12, color=WHITE, bold=True,
                     align=PP_ALIGN.CENTER, anchor=MSO_ANCHOR.MIDDLE)
            text_box(slide, 1463040, y + 20000, 7223760, 292608, row["head"], size=15, color=INK, bold=True)
            text_box(slide, 1463040, y + 340000, 7223760, 400000, row.get("text", ""), size=11.5, color=MUTED)
            self.texts.append(("row", f"{row['chip']} {row['head']} {row.get('text', '')}"))
        self.footer(slide)
        self.notes(slide, s.get("notes"))

    def tiles(self, s):
        slide = self.new_slide()
        self.header(slide, s["kicker"], s["title"])
        items = s["tiles"]
        per_row = s.get("per_row", 4)
        tw = (8229600 - (per_row - 1) * 150000) // per_row
        th = 1500000
        for i, t in enumerate(items):
            col, row = i % per_row, i // per_row
            x = 457200 + col * (tw + 150000)
            y = 1150000 + row * (th + 150000)
            round_rect(slide, x, y, tw, th, MINT_LIGHT if i % 2 == 0 else CHOCO_LIGHT)
            text_box(slide, x + 120000, y + 110000, tw - 240000, 300000, t["head"], size=13,
                     color=MINT_DARK if i % 2 == 0 else CHOCO, bold=True)
            text_box(slide, x + 120000, y + 470000, tw - 240000, th - 560000, t["text"], size=10.5, color=BODY)
            self.texts.append(("tile", f"{t['head']} {t['text']}"))
        self.footer(slide)
        self.notes(slide, s.get("notes"))

    def table(self, s):
        slide = self.new_slide()
        self.header(slide, s["kicker"], s["title"])
        rows = s["rows"]
        cols = len(rows[0])
        shape = slide.shapes.add_table(len(rows), cols, Emu(457200), Emu(1150000), Emu(8229600),
                                       Emu(300000 * len(rows)))
        table = shape.table
        widths = s.get("widths")
        if widths:
            for c, frac in enumerate(widths):
                table.columns[c].width = Emu(int(8229600 * frac))
        for r, row in enumerate(rows):
            for c, value in enumerate(row):
                cell = table.cell(r, c)
                cell.text = ""
                run = cell.text_frame.paragraphs[0].add_run()
                run.text = str(value)
                head = r == 0
                set_font(run, 10.5, WHITE if head else BODY, head, MONO if (c == 0 and not head and s.get("mono_first")) else FONT)
                cell.fill.solid()
                cell.fill.fore_color.rgb = CHOCO if head else (CHOCO_LIGHT if r % 2 == 0 else WHITE)
                self.texts.append(("cell", str(value)))
        self.footer(slide)
        self.notes(slide, s.get("notes"))

    # ---------------------------------------------------------------- build

    def build(self, out, draft=False):
        for s in self.spec["slides"]:
            getattr(self, s["kind"])(s)
        # the captions paint_record burns into the clips are deck text too
        for shot in paint_shots.all_shots():
            for _, caption in shot.get("captions", []):
                self.texts.append(("clip caption", caption))
        for where, text in self.texts:
            low = text.lower()
            for word in BANNED:
                if word in low:
                    self.errors.append(f"banned word '{word}' in {where}: {text[:80]}")
        if self.errors:
            for e in self.errors:
                print("error:", e)
            if not draft:
                return False
        out.parent.mkdir(parents=True, exist_ok=True)
        self.prs.save(out)
        sections = [s for s in self.spec["slides"] if s["kind"] == "section"]
        print(f"{out}: {len(self.prs.slides._sldIdLst)} slides, {self.video_count} videos, "
              f"{self.svg_count} SVG figures, {len(sections)} sections")
        return True


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("run", help="the recording run directory")
    parser.add_argument("--spec", default=str(ROOT / "Tools" / "paint_deck.json"))
    parser.add_argument("--out", default=None)
    parser.add_argument("--draft", action="store_true", help="save despite errors, for a layout check")
    args = parser.parse_args()
    spec = json.loads(Path(args.spec).read_text(encoding="utf-8"))
    out = Path(args.out) if args.out else Path(args.run) / "deck" / "MintChoco_Paint.pptx"
    return 0 if Deck(args.run, spec).build(out.resolve(), args.draft) else 1


if __name__ == "__main__":
    sys.exit(main())
