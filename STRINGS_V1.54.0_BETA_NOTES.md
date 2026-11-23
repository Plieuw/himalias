# Himalia Strings v1.54.0-beta

Fix attempt for "no sound" since v1.43 (v1.41 plays, v1.43 is silent):
the 1V/OCT calibration (v1.43) read a fixed flash address meant for a
1 MB chip at power-up. It now uses the last 8 KB block of the flash the
chip reports (NVMCTRL PARAM), and skips calibration when that is unknown.
Includes v1.53 (audio interrupt from RAM, -Os, safer CPU guard) and the
v1.52 ADC settings (the v1.41 knob glitches on DIVISI / MODEL came from the
12 MHz + averaging ADC setup).
