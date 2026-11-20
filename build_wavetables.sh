#!/bin/bash
# Build IOCore/HimaliaSketch/wavetables.h
#   put your Serum/Vital style wavetables in /PRJ/wavetables/ (01_x.wav, 02_y.wav ...)
#   up to 16 tables; free slots use the built-in tables.
#   . /PRJ/build_wavetables.sh            -> frames of 2048
#   . /PRJ/build_wavetables.sh 256        -> frames of 256
python3 /PRJ/IOCore/tools/make_wavetables.py --frame ${1:-2048}
