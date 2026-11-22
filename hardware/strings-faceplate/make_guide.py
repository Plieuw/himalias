# Panel guide (Dutch) in playing order: the preview + the controls in 3 steps.
# python3 make_guide.py  ->  Himalia_Strings_panel_guide.png
from PIL import Image, ImageDraw, ImageFont
import textwrap
F = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"; FB = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
f_t, f_h, f_l, f_x = ImageFont.truetype(FB, 46), ImageFont.truetype(FB, 32), ImageFont.truetype(FB, 24), ImageFont.truetype(F, 23)
panel = Image.open("Himalia_Strings_faceplate_preview.png").convert("RGB")
ph = 1700; panel = panel.resize((int(panel.width * ph / panel.height), ph))
STEPS = [
  ("1  WAT speelt er", (52, 120, 200), [
    ("MODEL", "7 instrumenten x 3 speelkarakters: VL solo viool, VC solo cello, VL+ violen, VA+ altviolen, VC+ celli, LOW celli+bassen, TUT tutti. Per instrument: klassiek (strak), romantisch (zingend, meer vibrato en glijders), film (breed, zwellende lange noten, zware tellen)."),
    ("PITCH + 1V/OCT", "Toonhoogte over het echte bereik van het instrument; CV 1 V/oct erbovenop."),
    ("SPREAD + CV", "Layout: unisono, octaven, kwinten, groot, klein, divisi groot/klein, cluster. Solo of vaste layout: expressiediepte (links neutraal, midden natuurlijk, rechts heel expressief)."),
  ]),
  ("2  HOE het gespeeld wordt", (40, 150, 90), [
    ("ARTIC + CV", "AUTO (gatelengte beslist) -> portamento, legato, detache, martele, staccato, spiccato, pizzicato -> klankkleur zacht naar hard: flageolet, sul tasto, ponticello, col legno. Detache t/m spiccato volgen ook de gatelengte. CV gepatcht = precies die articulatie."),
    ("ZONE + CV", "Contactpunt + kracht: toets (zacht, donker) ... normaal ... kam (luid, helder)."),
    ("BOW SPEED", "Strijksnelheid, log ~9 ... 25 ... 70 cm/s."),
  ]),
  ("3  De UITVOERING", (200, 110, 40), [
    ("GATE / knop", "Streek, pluk of slag. Gatelengte = informatie: lang/gebonden = legato, normaal = detache, kort = staccato, heel kort = spiccato."),
    ("VELOCITY (0-5 / 0-10 V)", "Met GATE: alleen velocity, 0 V = pp, nooit stil (bereik past zich aan). Zonder GATE: dynamische gate. Niveau bij de aanzet = hoe hard. Tijdens de noot = aftertouch: verder aandrukken geeft meer strijkgewicht, volume en vibrato; loslaten maakt lichter."),
    ("VIB / TREM / MUTE", "Vibrato aan/uit, tremolo, con sordino."),
    ("Automatisch", "Fraseboog + lange boog over 8 frases, zware tellen op afstreek, streekverdeling op de verwachte nootlengte, sectie volgt de concertmeester, vibrato reageert op tempo en sprongen."),
  ]),
  ("Uitgangen", (120, 120, 120), [
    ("STRINGS", "Hele ensemble, max +-4,98 V.   DIVISI: spelers 2-6.   DIV TRIG: 5 ms bij divisi-wissel.   6x PLAYERS: blokgolf per speler."),
    ("~ (ruissectie)", "Analoge ruis, NOISE TRIG en COMP zijn hardware: de firmware leest ze niet."),
  ]),
]
W = panel.width + 1250; H = ph + 140
im = Image.new("RGB", (W, H), (248, 248, 246)); d = ImageDraw.Draw(im)
d.text((40, 30), "Himalia Strings v1.35 - bedienen in speelvolgorde", font=f_t, fill=(20, 20, 20))
im.paste(panel, (40, 110))
x0 = panel.width + 90; y = 120
for title, col, items in STEPS:
    d.rounded_rectangle([x0 - 16, y - 6, W - 40, y + 44], radius=10, fill=col)
    d.text((x0, y), title, font=f_h, fill="white"); y += 64
    for lab, txt in items:
        d.text((x0, y), lab, font=f_l, fill=col); y += 32
        for line in textwrap.wrap(txt, 82):
            d.text((x0 + 14, y), line, font=f_x, fill=(30, 30, 30)); y += 29
        y += 10
    y += 14
im.save("Himalia_Strings_panel_guide.png"); print("guide height used", y, "of", H)
