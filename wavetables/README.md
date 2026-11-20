Put your Serum / Vital style wavetables (.wav) here.

- used in file name order: name them `01_name.wav`, `02_name.wav`, ...
- 8 files by default (`--tables`); the RATCHET knob picks the table, SAMPLE SELECT scans
- free slots are filled with the built-in "basic" and "formant" tables

Then inside docker: `. /PRJ/build_wavetables.sh` and `. /PRJ/build.sh`
