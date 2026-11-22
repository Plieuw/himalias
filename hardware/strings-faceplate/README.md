# Himalia Strings faceplate (PCB panel, 16 HP)

A replacement front panel for the IO Instruments Himalia running the Strings
firmware. **Every hole is where the original panel has it** (measured from the
vector drawing in the Himalia manual: 80.96 x 128.5 mm, 1.862 pt/mm), only the
artwork is new.

| file | what |
|---|---|
| `panel_layout.py` | the one table: positions, hole sizes, labels, knob scales |
| `make_panel.py` | builds `himalia_strings_faceplate.kicad_pcb` with KiCad 7's pcbnew |
| `himalia_strings_faceplate.kicad_pcb` | the board (open / edit in KiCad 7+) |
| `fab/` | Gerbers + NPTH drill (Excellon), ready to upload to a PCB fab |

## Before you order -- check these

1. Print `Himalia_Strings_faceplate_1to1_testprint.pdf` at **100 %**, check the
   50 mm bar, and lay the module's PCB / old panel on it: every crosshair must
   sit on a jack, pot shaft, LED and button.
2. Hole sizes are standard parts, change them in `panel_layout.py` (`HOLE`) if
   yours differ, then run `python3 make_panel.py` and re-export:
   - jacks 6.0 mm (Thonkiconn), pots 7.2 mm (9 mm Alpha, M7 bushing),
     LEDs 3.2 mm (3 mm LED), mounting slots 3.2 x 6.0 mm;
   - **buttons 8.9 mm / 8.4 mm (manual GATE)**: taken from the button caps on
     the drawing + clearance -- measure your buttons, this is the least certain.
3. The rails hide roughly the top and bottom 6 mm; nothing important is there.

## Fab settings

- 2 layers (no copper is used), 1.6 mm FR4, **black solder mask, white
  silkscreen** (the preview); any colour works.
- Upload the `fab/` folder (zip). Panel outline = `Edge_Cuts`, all holes are
  non-plated (`NPTH.drl`, includes the 4 slots as routed slots).

## Re-exporting

    python3 make_panel.py
    kicad-cli pcb export gerbers --layers F.Cu,B.Cu,F.Mask,B.Mask,F.SilkS,Edge.Cuts --no-protel-ext -o fab/ himalia_strings_faceplate.kicad_pcb
    kicad-cli pcb export drill --format excellon --excellon-separate-th --drill-origin absolute -o fab/ himalia_strings_faceplate.kicad_pcb

## The panel

- `~` marks the analog noise section (NOISE MIX, NOISE CV, NOISE TRIG, NOISE
  out, COMP): hardware that keeps working on its own; the Strings firmware
  does not read it.
- Outputs (DIV TRIG, DIVISI, STRINGS, 6x PLAYERS) have inverted labels.
- Knob scales: NOISE MIX (red / white / blue), BOW SPEED (9 / 25 / 70 cm/s),
  PITCH (low ... high of the instrument), ZONE (tasto / norm / ponticello),
  MODEL (1-18; 1 / 7 / 11 / 16 / 18 mark instruments / styles / film /
  experimental), SPREAD (8 layouts), ARTIC (11 articulations).
- The IO Instruments logo is left off; this is a DIY panel, not an official
  one.
