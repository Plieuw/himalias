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

### resonator voice (replaces the sample player)
SAMPLE OUT is a physical-modelling resonator with five models
(`IOCore/HimaliaSketch/resonators.h`).

| control                     | function                                   |
|-----------------------------|--------------------------------------------|
| RATCHET knob + CV           | model: string, bar/marimba, bell/glass, drum, flute/brass |
| SAMPLE TUNE knob + CV       | pitch: knob 7 octaves from 27.5 Hz, CV -10 ... +10 V at 1 V/oct |
| SAMPLE SELECT knob + CV     | timbre, per model (see below)              |
| TRIGGER in / button         | strike; the wind model blows while the gate is held |
| A/B button (sample side)    | decay short / long                         |
| BIT switch                  | bow / blow: excite continuously while the gate is held |
| CV IN RATE jack             | external audio excites the resonator (like Rings) |
| CLOCK RATE knob             | wavefolder after the resonator: clean ... 6 folds |
| rate LED                    | on while external audio comes in           |

| model        | timbre (SELECT)                                  |
|--------------|--------------------------------------------------|
| string       | dark nylon ... bright steel                      |
| bar          | soft ... hard mallet                             |
| bell / glass | church bell ... wine glass / plate (inharmonic morph) |
| drum         | soft ... hard mallet (pitch drops after the strike) |
| flute/brass  | airy flute ... bright brass                      |

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
| resonators                    | ~32 KB        |

### here is the uf2 bootloader for the samd51
https://github.com/noscene/uf2-samdx1
