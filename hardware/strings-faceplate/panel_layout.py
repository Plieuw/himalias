"""Himalia Strings faceplate: one table for the PCB panel and the guide.

Positions (mm, from the panel's top-left corner) are measured from the vector
drawing in IO Instruments' Himalia manual (16 HP, 80.96 x 128.5 mm); every
hole stays where the original panel has it. Hole sizes are the usual parts
(Thonkiconn jack, 9 mm Alpha pot, 3 mm LED); CHECK THEM against your module
(calipers) before ordering -- the button holes especially.
"""
PANEL_W, PANEL_H = 80.96, 128.5          # 16 HP Eurorack
MOUNT = [(7.5, 3.0), (73.54, 3.0), (7.5, 125.5), (73.54, 125.5)]
MOUNT_SLOT = (3.2, 6.0)                  # slot width, length (horizontal)

HOLE = {"jack": 6.0, "pot": 7.2, "button": 8.9, "button_s": 8.4, "led": 3.2}
CAP = {"pot": 12.8, "pot_s": 9.1}        # knob diameters on the original (for the scales)

# kind, x, y, short panel label, function (guide, Dutch), group
# group: in = CV / gate input, out = output, ctl = control, noise = analog noise (hardware)
ELEMENTS = [
  # left column: jacks
  ("jack", 8.39, 19.71, "NOISE CV",   "CV filter analoge ruis (hardware, ongewijzigd)", "noise"),
  ("jack", 8.39, 32.93, "VELOCITY",   "Dynamische gate 0-10 V: niveau bij de aanzet = hoe hard; tijdens de noot = aftertouch (aandrukken); ook gate en tempo", "in"),
  ("jack", 8.39, 46.15, "NOISE TRIG", "Trigger digitale ruis (hardware, niet gelezen door de firmware)", "noise"),
  ("jack", 8.39, 59.36, "GATE",       "Gate: streek / pluk / slag; gatelengte = articulatie (AUTO), het ritme geeft frase, tel en tempo", "in"),
  ("jack", 8.39, 72.58, "1V/OCT",     "Toonhoogte 1 V/oct, continu (geen quantizer)", "in"),
  ("jack", 8.39, 85.79, "ZONE",       "CV speelzone (1 V = 10 %)", "in"),
  ("jack", 8.39, 99.02, "ARTIC",      "CV articulatie: 1 V = 1 stap (gepatcht = precies die, geen gatelengte); bij vaste articulatie: gebaar (1 V = 10 %)", "in"),
  ("jack", 8.39, 112.23, "SPREAD",    "CV spread: layout / expressie / breedte", "in"),
  # right column
  ("jack",   72.69, 19.70, "NOISE",      "Analoge ruis uit (hardware, ongewijzigd)", "noise"),
  ("button", 72.69, 32.94, "COMP",       "Comparator op de analoge ruis (hardware)", "noise"),
  ("jack",   72.69, 46.15, "DIV TRIG",   "Trigger (5 ms) als de divisi wisselt", "out"),
  ("jack",   72.69, 59.35, "DIVISI",     "Divisi-spelers 2-6 als strijkers (diagnose: CPU 1 V = 20 %)", "out"),
  ("button", 72.69, 72.60, "TREM",       "Tremolo aan / uit", "ctl"),
  ("jack",   72.69, 85.78, "STRINGS",    "Strijkers: hele ensemble, max +-4,98 V", "out"),
  ("button", 72.69, 99.04, "VIB",        "Vibrato aan / senza vibrato", "ctl"),
  ("jack",   72.69, 112.22, "6x PLAYERS","Elke speler als blokgolf op zijn eigen noot", "out"),
  # centre
  ("pot",    29.42, 19.76, "NOISE MIX",  "Rood-wit-blauw ruismix (hardware, ongewijzigd)", "noise"),
  ("pot",    53.56, 32.73, "BOW SPEED",  "Strijksnelheid, log: ~9 ... 25 ... 70 cm/s", "ctl"),
  ("led",    57.98, 22.77, "BEAT",       "Knippert op de gevonden tel (diagnose: overbelast)", "ctl"),
  ("button_s", 25.94, 42.57, "GATE",     "Handmatige gate (vasthouden bij opstarten: diagnose)", "ctl"),
  ("led",    32.81, 38.54, "",           "Gate-LED", "ctl"),
  ("pot",    29.39, 66.04, "PITCH",      "Toonhoogte over het echte bereik van het instrument", "ctl"),
  ("pot_s",  53.55, 66.15, "ZONE",       "Speelzone: toets (zacht, donker) ... kam (luid, helder)", "ctl"),
  ("pot_s",  33.74, 86.76, "MODEL",      "Model 1-18: instrumenten, stijlen, filmgebaren, experimenteel", "ctl"),
  ("button", 57.20, 86.82, "MUTE",       "Con sordino (demper)", "ctl"),
  ("pot",    29.53, 112.12, "SPREAD",    "Layout; bij solo / vaste layout: expressie; zwerm- / clusterbreedte", "ctl"),
  ("pot",    53.62, 112.11, "ARTIC",     "Articulatie (12): AUTO = gatelengte beslist, dan lang -> kort -> klankkleur; bij vaste articulatie: gebaar van het model", "ctl"),
]

# scales around knobs: (index in ELEMENTS, labels from 7 o'clock to 5 o'clock)
SCALES = {
  "PITCH":     ["LOW", "", "", "", "HIGH"],
  "BOW SPEED": ["9", "", "25", "", "70"],
  "ZONE":      ["TASTO", "", "NORM", "", "PONT"],
  "SPREAD":    ["UNI", "OCT", "5TH", "MAJ", "MIN", "DIV+", "DIV-", "CLU"],
  "ARTIC":     ["AUTO", "POR", "LEG", "DET", "MAR", "STA", "SPI", "PIZ", "FLA", "TAS", "PON", "COL"],
  "MODEL":     ["1", "", "", "", "", "", "7", "", "", "", "11", "", "", "", "", "16", "", "18"],
  "NOISE MIX": ["RED", "", "WHT", "", "BLU"],
}
MODEL_GROUPS = [(1, 7, "INSTR"), (8, 11, "STYLE"), (12, 16, "FILM"), (17, 18, "EXP")]
