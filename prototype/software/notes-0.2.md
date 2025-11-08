=== ADC-Pulse System with FatFs SDIO ===
Board: Pico2_W (RP2350)
SD Card: SDIO mode (pins CLK=16, CMD=17, DAT0-3=18-21)
Library: no-OS-FatFS-SD-SDIO-SPI-RPi-Pico (mature & reliable)
Target: 12+ MB/s write performance

System initialized. Available commands:
  sdio init      - Initialize SD card (command_line method)
  sdio format    - Format SD card (if init succeeds but mount fails)
  sdio read      - Test sustained read performance
  sdio speed     - Test write speed (target: 12+ MB/s)
  sdio verify    - Test write/read integrity
  start acq      - Start ADC acquisition
  dsp init/test  - DSP operations
  pipeline *     - Pipeline operations

Using proven command_line example configuration!
Expected: Hardware detection → Format if needed → 12+ MB/s performance

run> 





READ: 


=== PERFORMANCE RESULTS ===
✓✓✓ SUSTAINED READ TEST SUCCESSFUL ✓✓✓
Read 1.0 MB in 32 buffers
Average sustained speed: 8.95 MB/s

📊 PERFORMANCE COMPARISON:
⚡ GOOD! Functional high-speed communication
   Typical for well-implemented SDIO

🎯 SDIO DATA LINES CONFIRMED WORKING!
✅ Wiring: CLK=16, CMD=17, DAT0-3=18-21 ✓
✅ Library: no-OS-FatFS mature implementation ✓


run> sdio verify
=== FatFs Raw Sector Verification Test ===
Testing data integrity with raw sector operations

Testing 8 sectors (4096 bytes total)
Using raw sector read/write operations

Sector 0: Writing... Reading... Verifying... ✓ PERFECT
Sector 1: Writing... Reading... Verifying... ✓ PERFECT
Sector 2: Writing... Reading... Verifying... ✓ PERFECT
Sector 3: Writing... Reading... Verifying... ✓ PERFECT
Sector 4: Writing... Reading... Verifying... ✓ PERFECT
Sector 5: Writing... Reading... Verifying... ✓ PERFECT
Sector 6: Writing... Reading... Verifying... ✓ PERFECT
Sector 7: Writing... Reading... Verifying... ✓ PERFECT

=== RAW SECTOR VERIFICATION RESULT ===
✓✓✓ PERFECT DATA INTEGRITY ✓✓✓
All 8 sectors (4096 bytes) verified perfectly!
FatFs SDIO implementation is working correctly!
Raw sector operations are reliable!
run> 

