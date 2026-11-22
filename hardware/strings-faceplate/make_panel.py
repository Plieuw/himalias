#!/usr/bin/env python3
"""Build the Himalia Strings faceplate (KiCad 7) from panel_layout.py.

    python3 make_panel.py            -> himalia_strings_faceplate.kicad_pcb
Then (KiCad 7):
    kicad-cli pcb export gerbers ... / kicad-cli pcb export drill ...
(make_fab.sh does both). Black solder mask + white silkscreen is the intended
look; any colour works.
"""
import math, sys, os
import pcbnew
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panel_layout import *

OX, OY = 100.0, 50.0                      # panel origin on the KiCad sheet
def P(x, y): return pcbnew.VECTOR2I(pcbnew.FromMM(OX + x), pcbnew.FromMM(OY + y))
board = pcbnew.BOARD()
board.SetCopperLayerCount(2)
SILK, EDGE = pcbnew.F_SilkS, pcbnew.Edge_Cuts

def seg(x0, y0, x1, y1, w=0.2, layer=SILK):
    s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(P(x0, y0)); s.SetEnd(P(x1, y1)); s.SetWidth(pcbnew.FromMM(w)); s.SetLayer(layer); board.Add(s)

def circle(x, y, r, w=0.2, fill=False):
    s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_CIRCLE)
    s.SetCenter(P(x, y)); s.SetEnd(P(x + r, y)); s.SetWidth(pcbnew.FromMM(w)); s.SetFilled(fill)
    s.SetLayer(SILK); board.Add(s)

def rect(x0, y0, x1, y1, fill=True, w=0.15):
    s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_RECT)
    s.SetStart(P(x0, y0)); s.SetEnd(P(x1, y1)); s.SetFilled(fill); s.SetWidth(pcbnew.FromMM(w))
    s.SetLayer(SILK); board.Add(s)

def text(t, x, y, h=1.3, th=None, just=0, knockout=False, bold=False):
    if not t: return
    tx = pcbnew.PCB_TEXT(board); tx.SetText(t); tx.SetPosition(P(x, y)); tx.SetLayer(SILK)
    tx.SetTextSize(pcbnew.VECTOR2I(pcbnew.FromMM(h), pcbnew.FromMM(h)))
    tx.SetTextThickness(pcbnew.FromMM(th if th else max(0.15, h * 0.15)))
    tx.SetHorizJustify({-1: pcbnew.GR_TEXT_H_ALIGN_LEFT, 0: pcbnew.GR_TEXT_H_ALIGN_CENTER, 1: pcbnew.GR_TEXT_H_ALIGN_RIGHT}[just])
    tx.SetVertJustify(pcbnew.GR_TEXT_V_ALIGN_CENTER)
    if knockout: tx.SetIsKnockout(True)
    board.Add(tx)

# ---- outline
for (x0, y0, x1, y1) in ((0, 0, PANEL_W, 0), (PANEL_W, 0, PANEL_W, PANEL_H), (PANEL_W, PANEL_H, 0, PANEL_H), (0, PANEL_H, 0, 0)):
    seg(x0, y0, x1, y1, 0.1, EDGE)

# ---- holes: one footprint per NPTH hole, placed at the hole
_n = [0]
def hole(x, y, d, slot=None):
    _n[0] += 1
    fp = pcbnew.FOOTPRINT(board)
    fp.SetReference("H%d" % _n[0]); fp.Reference().SetVisible(False); fp.Value().SetVisible(False)
    board.Add(fp)
    pad = pcbnew.PAD(fp); pad.SetAttribute(pcbnew.PAD_ATTRIB_NPTH)
    pad.SetLayerSet(pad.UnplatedHoleMask())
    if slot:
        w, l = slot
        pad.SetShape(pcbnew.PAD_SHAPE_OVAL); pad.SetDrillShape(pcbnew.PAD_DRILL_SHAPE_OBLONG)
        pad.SetSize(pcbnew.VECTOR2I(pcbnew.FromMM(l), pcbnew.FromMM(w)))
        pad.SetDrillSize(pcbnew.VECTOR2I(pcbnew.FromMM(l), pcbnew.FromMM(w)))
    else:
        pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
        pad.SetSize(pcbnew.VECTOR2I(pcbnew.FromMM(d), pcbnew.FromMM(d)))
        pad.SetDrillSize(pcbnew.VECTOR2I(pcbnew.FromMM(d), pcbnew.FromMM(d)))
    fp.Add(pad)
    fp.SetPosition(P(x, y))                                   # moves the pad with it
for (x, y) in MOUNT: hole(x, y, 0, slot=MOUNT_SLOT)

# ---- title (where the original has HIMA / LIA)
text("HIMALIA", 13.2, 4.2, h=3.0, th=0.5, just=-1)
text("STRINGS", 13.4, 8.2, h=1.8, th=0.3, just=-1)
text("bowed-string physical model", 40.0, 8.2, h=0.9, th=0.14, just=-1)

# ---- elements
LABEL_POS = {   # label placement per element (dx, dy from the centre), default: above
  "NOISE MIX": (0, 11.4), "BOW SPEED": (0, 11.0), "BEAT": (2.6, 0, -1), "PITCH": (0, -10.6),
  "ZONE": (0, 10.4), "MODEL": (0, 10.0), "MUTE": (0, 6.2), "SPREAD": (0, 11.6), "ARTIC": (0, 11.6),
}
def scale(cx, cy, r_tick, labels, r_text=None):
    n = len(labels)
    for k, lab in enumerate(labels):
        a = math.radians(-150 + 300 * k / (n - 1))           # 0 = up, clockwise
        sx, sy = math.sin(a), -math.cos(a)
        long = bool(lab)
        seg(cx + sx * r_tick, cy + sy * r_tick, cx + sx * (r_tick + (1.1 if long else 0.6)),
            cy + sy * (r_tick + (1.1 if long else 0.6)), 0.18)
        if lab:
            h = 0.95; wdt = 0.85 * h * len(lab)                # stroke font: ~0.85 h per character
            rr = r_tick + 1.5 + 0.55 * h + abs(sx) * wdt / 2    # push wide labels out at the sides
            text(lab, cx + sx * rr, cy + sy * rr, h=h, th=0.15)

for kind, x, y, label, func, grp in ELEMENTS:
    if kind == "jack":
        hole(x, y, HOLE["jack"])
        circle(x, y, 4.3, 0.18)                               # nut ring
        lx, ly = x, y - 5.65
        lab = ("~" + label) if grp == "noise" else label
        if grp == "out":
            text(lab, lx, ly, h=1.2, knockout=True)
        else:
            text(lab, lx, ly, h=1.25)
        # in / out arrow, like the original panel
        ax = x - 5.6 if x < PANEL_W / 2 else x + 5.6
        d = 1 if (grp in ("in", "noise") and x < PANEL_W / 2) else -1
        if grp == "out" or (grp == "noise" and x > PANEL_W / 2): d = 1 if x > PANEL_W / 2 else -1
        seg(ax - 0.9 * d, y, ax + 0.9 * d, y, 0.2)
        seg(ax + 0.9 * d, y, ax + 0.3 * d, y - 0.6, 0.2); seg(ax + 0.9 * d, y, ax + 0.3 * d, y + 0.6, 0.2)
    elif kind in ("button", "button_s"):
        hole(x, y, HOLE[kind])
        circle(x, y, HOLE[kind] / 2 + 0.9, 0.18)
        lab = ("~" + label) if grp == "noise" else label
        dx, dy = LABEL_POS.get(label, (0, -(HOLE[kind] / 2 + 1.9)))[:2]
        text(lab, x + dx, y + dy, h=1.25)
    elif kind == "led":
        hole(x, y, HOLE["led"])
        circle(x, y, HOLE["led"] / 2 + 0.6, 0.15)
        if label:
            pos = LABEL_POS.get(label, (0, -3.2))
            text(label, x + pos[0], y + pos[1], h=1.0, just=pos[2] if len(pos) > 2 else 0)
    elif kind in ("pot", "pot_s"):
        hole(x, y, HOLE["pot"])
        cap = CAP[kind]
        r_tick = cap / 2 + 0.7
        labs = SCALES.get(label, [])
        if labs: scale(x, y, r_tick, labs, r_tick + (2.2 if kind == "pot" else 2.0))
        lab = ("~" + label) if grp == "noise" else label
        dx, dy = LABEL_POS.get(label, (0, -(cap / 2 + 4.0)))[:2]
        text(lab, x + dx, y + dy, h=1.45, th=0.24)

# MODEL: the scale numbers 1 / 7 / 11 / 16 / 18 mark the groups
# (instruments 1-7, playing styles 8-11, film gestures 12-16, experimental 17-18)

# small legend at the bottom: what the marks mean
text("~ analog noise (hardware)", 40.0, 10.3, h=0.85, th=0.13, just=-1)
text("OUT", 62.2, 10.3, h=0.85, th=0.13, knockout=True)
text("= output", 64.9, 10.3, h=0.85, th=0.13, just=-1)

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "himalia_strings_faceplate.kicad_pcb")
board.Save(out)
print("wrote", out)
