# Himalia Strings v1.55.0-beta

Fixed: no sound and "USB device not recognized" since v1.43.

Cause (found with test builds on the module): at power-up the firmware read
the 1V/OCT calibration from the last 8 KB block of the flash. On this module
that area cannot be read (most likely reserved as SmartEEPROM by the
bootloader's fuses); the read faulted before anything else ran. v1.54 without
that read works.

- The calibration now lives in an 8 KB block reserved inside the firmware
  itself. A firmware update erases it: calibrate again after an update
  (without a calibration the standard values are used, as before).
- Keeps v1.53 / v1.54: audio interrupt from RAM, -Os, safer CPU guard, the
  v1.52 ADC settings (no knob glitches).
