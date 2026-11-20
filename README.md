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
| SAMPLE TUNE knob + CV       | pitch (about 16 Hz ... 8 kHz)              |
| SAMPLE SELECT knob + CV     | scan through the 64 waves of a table       |
| RATCHET knob + CV           | wavetable select (8 tables)                |
| A/B button (sample side)    | wavefolder on / off                        |
| TRIGGER in / button         | phase reset (sync)                         |
| BIT switch                  | 8 bit bytebeat mode (A/B = bytebeat bank)  |

Put Serum/Vital style wavetables (.wav, single-cycle frames, frame size read
from the Serum `clm` chunk, otherwise 2048) in `wavetables/`. They are used in
file name order, so prefix them `01_`, `02_` ... Default 8 tables of 64 waves
(packed 12 bit, 768 KB); other layouts with `--tables` / `--waves`, e.g.
16 x 32. Free slots get the built-in "basic" (sine > triangle > saw > square >
pulse) and "formant" tables. Frames are picked evenly from each file. Then in docker:
```
pip3 install numpy               # once, makes the converter fast (optional)
. /PRJ/build_wavetables.sh
. /PRJ/build.sh
```
The table order is listed at the top of `IOCore/HimaliaSketch/wavetables.h`.
Every wave is stored band limited in 4 versions so high notes do not alias.

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

### here is the uf2 bootloader for the samd51
https://github.com/noscene/uf2-samdx1
