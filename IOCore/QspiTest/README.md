# QSPI flash test

Checks the external QSPI flash (S25FL127S, 16 MB on the schematic): JEDEC ID,
erase / write / read back of one 64 KB block at 8 MB, and random read speed
through the memory window (what the audio interrupt would do).

Flash `firmware/Himalia_QSPI_test.uf2`, open the USB serial port at 115200
baud and read the report (repeats every 10 s). Flash the normal firmware again
afterwards.

Builds with the Adafruit SPIFlash library (`arduino-cli lib install "Adafruit SPIFlash"`).
