# Himalia - IO Instruments

Himalia is a eurorack module with an samd51 cortex m4 mcu. The firmeware can build complete
in a docker enviroment, so it has no library conflicts on build machine.
The firmware is written in arduino style so it is easy to use and modify.

## Docker build chain based on arduino cli
the commands are used on macos in terminal. if using windows or linux may need
some changes in syntax or directorys.

### build docker image
just clone repo and create the docker images. it contains all arduino stuff, board package and convert tool for own samples
just start the terminal and:
```
cd ~/Documents/GitHub
git clone https://github.com/noscene/himalia
cd ~/Documents/GitHub/Himalia
docker build -t iocore IOCore/
```

### start docker instance and mount USB,Arduino BoardPackage and himalia src files
when docker image is created, it can run to compile it.
if using mac you can mount /Volumes dir into docker container to copy image after build direct by usb mass storage
you need connect the himalia module by micro usb and double tap the reset button to enter bootloader mode.
```
cd ~/Documents/GitHub/Himalia
export MOUNTPOINT=`pwd`
docker run -it -v $MOUNTPOINT:/PRJ -v /Volumes:/USB  -v $MOUNTPOINT/IOCore/variant_IOCore:/root/.arduino15/packages/dadamachines_doppler/hardware/samd/1.2.9/variants/IOCore  iocore bash
```

### inside docker: build all in one
double tap reset on module to go into bootloader mode. Then it works like a simple usb stick
and you only need the xxx.uf2 file into root for update firmware. Call the build.sh script
to compile all and copy firmware into module.
```
. /PRJ/build.sh 
```

### wavetable oscillator (replaces the sample player)
SAMPLE OUT is now a wavetable oscillator:

| control                     | function                                   |
|-----------------------------|--------------------------------------------|
| SAMPLE TUNE knob + CV       | pitch: knob 16 Hz ... 8 kHz, CV -10 ... +10 V at 1 V/oct |
| SAMPLE SELECT knob + CV     | scan through the 46 waves; CV -10 ... +10 V = whole table |
| RATCHET knob + CV           | wavetable select (10 tables)               |
| A/B button (sample side)    | LPG mode: short / long decay               |
| CLOCK RATE knob             | wavefolder after the LPG: clean ... 6 folds |
| TRIGGER in / button         | phase reset (sync); opens the LPG in LPG mode |
| BIT switch                  | LPG mode: vactrol-style low pass gate on trigger/gate (replaces bytebeat) |

Put Serum/Vital style wavetables (.wav, single-cycle frames, frame size read
from the Serum `clm` chunk, otherwise 2048) in `wavetables/`. They are used in
file name order, so prefix them `01_`, `02_` ... Default 10 tables of 46 waves
(packed 12 bit, 906 KB); other layouts with `--tables` / `--waves`, e.g.
8 x 57. The real limit is the bootloader's USB drive: the .uf2 (about twice
the firmware size) must be smaller than 1,961,472 bytes. Free slots get the built-in "basic" (sine > triangle > saw > square >
pulse) and "formant" tables. Frames are picked evenly from each file. Then in docker:
```
pip3 install numpy               # once, makes the converter fast (optional)
. /PRJ/build_wavetables.sh
. /PRJ/build.sh
```
The table order is listed at the top of `IOCore/HimaliaSketch/wavetables.h`.
The converter also levels each table for smooth scanning: (almost) silent
frames are skipped, sudden quiet frames are turned up and frames that are
upside down compared to their neighbour are flipped (`--no-level` turns this off).

CV calibration: open the USB serial port at 115200 baud; the module prints the
raw tune / scan CV readings twice a second. Set `CV_ZERO_ADC` and
`CV_COUNTS_PER_VOLT` in `HimaliaSketch.ino` from readings at 0 V and e.g. +5 V.
Every wave is stored band limited in 7 versions, one per octave, and the
firmware picks the one whose highest harmonic stays below 20 kHz. The low
versions are stored with many more points than harmonics and the firmware
reads them with cubic interpolation, so interpolation images stay low too
(measured: non-harmonic content at least 42 dB, mostly 50-70 dB below the
signal over the whole pitch range). Hard sync and the wavefolder add their
own aliasing on top; that is part of how they sound.

### generative sequencer (replaces the spread presets)
The 6 squares are a generative sequencer, Turing machine style: a 16 step
loop of bits that slowly (or quickly) mutates.

| control                     | function                                   |
|-----------------------------|--------------------------------------------|
| SAMPLE TRIGGER in / button  | clock: every trigger is one step (also strikes the LPG / syncs the wavetable) |
| SQUARES MAIN TUNE + CV      | root pitch, same 1 V/oct scale as the CV out: lowest note at 0 V = C2 (65 Hz) |
| SPREAD knob + CV            | change: left = loop locked, right = every step random |
| A/B button (squares side)   | scale: A = minor pentatonic, B = natural minor |
| noise side DAC out          | melody pitch CV, 1 V/oct, 0 ... +5 V: MAIN TUNE fully left = lowest note at 0 V, fully right = highest note at +5 V |
| digital noise out (PA22)    | 5 ms trigger whenever the melody note changes |

Voices: melody, sub octave, two harmonies, octave up and a drone that moves
now and then. Per step the register decides which voices sound, their pulse
width and which filter caps are switched in.

### real oscillator sync (replaces the digital noise / sample & hold)
Patch the waveform of another oscillator (the master) into CV IN RATE. Every
rising zero crossing restarts the wavetable (hard sync); the exact crossing
time between two samples is interpolated, so the sync is tight.

| control                     | function                                   |
|-----------------------------|--------------------------------------------|
| CV IN RATE jack             | hard sync input (master waveform, needs to swing through 0 V) |
| rate LED                    | on while a sync signal comes in            |

The jack is sampled every audio tick on ADC0; knob readings on ADC0 are
interleaved with it.

### flash size
The Himalia uses an ATSAMD51J20A with 1 MB flash. The old linker script only
allowed 512 KB (copied from the Doppler, which has a 512 KB chip), which made
the build fail with "too big" as soon as more samples were added. The linker
scripts in `IOCore/variant_IOCore/linker_scripts` now use the full 1 MB
(1008 KB for the firmware after the 16 KB bootloader).

| build                         | firmware size |
|-------------------------------|---------------|
| factory (16 bit samples)      | ~394 KB       |
| wavetables, 2 tables          | ~181 KB       |
| wavetables, 16 x 16 (16 bit)  | ~690 KB       |
| wavetables, 8 x 64 (12 bit)   | ~820 KB       |
| wavetables, 10 x 46, 7 levels | ~952 KB (.uf2 1.91 MB, max 1.96 MB) |

### here is the uf2 bootloader for the samd51
https://github.com/noscene/uf2-samdx1
