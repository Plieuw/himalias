//
//  Himalia QSPI flash test
//
//  Checks whether the external QSPI flash chip (S25FL127S, 16 MB on the
//  schematic) is there, whether it can be written, and how fast random
//  reads are while the module would be playing.
//
//  Open the USB serial port at 115200 baud (e.g. the Google serial terminal)
//  and read the report. It repeats every 10 seconds.
//  LED (sample change): fast blinking = all tests passed, slow = a test failed.
//
//  The test only writes to one 64 KB block at 8 MB; nothing else is touched.
//
#include "Adafruit_SPIFlashBase.h"

Adafruit_FlashTransport_QSPI flashTransport;
Adafruit_SPIFlashBase flash(&flashTransport);

// Cypress / Infineon S25FL127S: 16 MB, JEDEC 01 20 18
static const SPIFlash_Device_t S25FL127S_DEV = {
  .total_size = (1UL << 24), .start_up_time_us = 300, .manufacturer_id = 0x01,
  .memory_type = 0x20, .capacity = 0x18, .max_clock_speed_mhz = 50,
  .quad_enable_bit_mask = 0x02, .has_sector_protection = false,
  .supports_fast_read = true, .supports_qspi = true,
  .supports_qspi_writes = true, .write_status_register_split = false,
  .single_status_byte = false, .is_fram = false,
};

static const uint32_t TEST_ADDR = 8UL * 1024 * 1024;     // 8 MB
static uint8_t buf[4096];
static bool all_ok = false;

static void dwtInit() {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

// Put the QSPI peripheral in "read memory" mode: plain reads from the
// memory window at 0x04000000 then read the flash directly (like the audio
// interrupt would do).
static void qspiMemoryMode() {
  QSPI->INSTRCTRL.bit.INSTR = 0x6B;   // quad output fast read
  QSPI->INSTRFRAME.reg = QSPI_INSTRFRAME_WIDTH_QUAD_OUTPUT | QSPI_INSTRFRAME_ADDRLEN_24BITS |
                         QSPI_INSTRFRAME_TFRTYPE_READMEMORY | QSPI_INSTRFRAME_INSTREN |
                         QSPI_INSTRFRAME_ADDREN | QSPI_INSTRFRAME_DATAEN |
                         QSPI_INSTRFRAME_DUMMYLEN(8);
  volatile uint32_t dummy = QSPI->INSTRFRAME.reg; (void)dummy;
}

static uint32_t rng = 0x1234567;
static uint32_t rnd() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static void report() {
  all_ok = true;
  Serial.println();
  Serial.println("==== Himalia QSPI flash test ====");

  uint32_t jedec = flash.getJEDECID();
  Serial.print("JEDEC ID: 0x"); Serial.println(jedec, HEX);
  Serial.print("Grootte volgens chip: "); Serial.print(flash.size() / 1024); Serial.println(" KB");
  if (jedec == 0 || jedec == 0xFFFFFF) {
    Serial.println("FOUT: geen flashchip gevonden (geen antwoord op QSPI).");
    all_ok = false;
    return;
  }
  if (jedec != 0x012018) {
    Serial.println("Let op: andere chip dan de S25FL127S uit het schema. Test gaat door.");
  }

  // ---- write test: erase one 64 KB block at 8 MB, write 4 KB, read back
  for (uint32_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)(i * 7 + 3);
  uint32_t t0 = millis();
  bool er = flash.eraseBlock(TEST_ADDR / (64UL * 1024));
  uint32_t t_erase = millis() - t0;
  t0 = millis();
  uint32_t w = flash.writeBuffer(TEST_ADDR, buf, sizeof(buf));
  uint32_t t_write = millis() - t0;
  memset(buf, 0, sizeof(buf));
  flash.readBuffer(TEST_ADDR, buf, sizeof(buf));
  uint32_t bad = 0;
  for (uint32_t i = 0; i < sizeof(buf); i++) if (buf[i] != (uint8_t)(i * 7 + 3)) bad++;
  Serial.print("Wissen 64 KB: "); Serial.print(er ? "ok" : "FOUT"); Serial.print(" ("); Serial.print(t_erase); Serial.println(" ms)");
  Serial.print("Schrijven 4 KB: "); Serial.print(w == sizeof(buf) ? "ok" : "FOUT"); Serial.print(" ("); Serial.print(t_write); Serial.println(" ms)");
  Serial.print("Teruglezen: "); Serial.print(bad == 0 ? "ok" : "FOUT"); Serial.print(" ("); Serial.print(bad); Serial.println(" bytes fout)");
  if (!er || w != sizeof(buf) || bad) all_ok = false;

  // ---- read speed through the memory window (what the audio code would do)
  qspiMemoryMode();
  volatile const uint8_t *mem = (volatile const uint8_t *)QSPI_AHB;

  // check the memory window sees the same data
  uint32_t bad2 = 0;
  for (uint32_t i = 0; i < 256; i++) if (mem[TEST_ADDR + i] != (uint8_t)(i * 7 + 3)) bad2++;
  Serial.print("Lezen via geheugenvenster: "); Serial.println(bad2 == 0 ? "ok" : "FOUT");
  if (bad2) all_ok = false;

  const uint32_t N = 20000;
  uint32_t sum = 0;
  // random reads all over 12 MB: worst case, nothing in a cache
  noInterrupts();
  uint32_t c0 = DWT->CYCCNT;
  for (uint32_t i = 0; i < N; i++) sum += mem[rnd() % (12UL * 1024 * 1024)];
  uint32_t c_rand = DWT->CYCCNT - c0;
  // "audio pattern": 4 reads close together (two neighbouring waves, two points)
  c0 = DWT->CYCCNT;
  for (uint32_t i = 0; i < N / 4; i++) {
    uint32_t base = rnd() % (12UL * 1024 * 1024 - 1024);
    sum += mem[base] + mem[base + 1] + mem[base + 384] + mem[base + 385];
  }
  uint32_t c_audio = DWT->CYCCNT - c0;
  interrupts();

  float us_rand = (float)c_rand / (float)N / (F_CPU / 1000000.0f);
  float us_tick = (float)c_audio / (float)(N / 4) / (F_CPU / 1000000.0f);
  const float budget_us = 1000000.0f / 47619.0f;
  Serial.print("Willekeurige leesactie: "); Serial.print(us_rand, 2); Serial.println(" us");
  Serial.print("Leeswerk per audiosample (4 waarden): "); Serial.print(us_tick, 2);
  Serial.print(" us van de "); Serial.print(budget_us, 1); Serial.println(" us per sample");
  Serial.print("QSPI klok: "); Serial.print(F_CPU / (QSPI->BAUD.bit.BAUD + 1) / 1000000); Serial.println(" MHz");
  if (us_tick > budget_us * 0.35f) {
    Serial.println("Te traag om tijdens het spelen rechtstreeks te lezen (meer dan 35% van de tijd).");
    all_ok = false;
  }
  (void)sum;

  Serial.println(all_ok ? "RESULTAAT: GESLAAGD - de 16 MB route kan." : "RESULTAAT: NIET GESLAAGD - zie hierboven.");
}

void setup() {
  pinMode(PB31, OUTPUT);
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && millis() - t < 15000) {          // wait up to 15 s for the terminal
    digitalWrite(PB31, (millis() / 100) & 1);
  }
  dwtInit();
  bool ok = flash.begin(&S25FL127S_DEV, 1);
  if (!ok) Serial.println("flash.begin: chip niet herkend als S25FL127S (ID volgt hieronder).");
  // start at 50 MHz, the library may have set a lower clock
  flashTransport.setClockSpeed(50000000UL, 0);
  report();
}

void loop() {
  static uint32_t last = 0;
  digitalWrite(PB31, (millis() / (all_ok ? 100 : 700)) & 1);
  if (millis() - last > 10000) {
    last = millis();
    report();
  }
}
