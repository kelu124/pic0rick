# ADC-Pulse High-Speed Data Acquisition System

## Overview

This project implements a high-speed data acquisition and processing pipeline for the Raspberry Pi Pico 2 (RP2350), featuring:

1. **SDIO SD Card Interface** - Up to 12MB/s write speeds with 32kB chunks (optimized for RP2350 memory)
2. **Signal Processing & Decimation** - Envelope detection and filtering for RF signals
3. **Parallel Pipeline** - Continuous 30μs acquisition cycles with real-time processing
4. **Multi-core Architecture** - Core 0 for acquisition/DSP, Core 1 for SD writing

## Hardware Requirements

- **Raspberry Pi Pico 2 (RP2350)** - Main processing unit
- **pic0rick Board** - Compatible with J6 connector for SDIO
- **Adafruit SDIO MicroSD Breakout** - For high-speed storage
- **SD Card** - Class 10 or better recommended

### Pin Assignments (pic0rick Compatible)

| Function | GPIO Pins | Notes |
|----------|-----------|-------|
| **ADC Data** | 0-10 | 11-bit ADC input from pic0rick |
| **Pulser** | 11, 16 | Pulse generator outputs |
| **MUX (MAX14866)** | 12, 13, 14, 15, 17 | Analog multiplexer control |
| **SDIO (J6)** | 18, 19, 20, 21, 22, 26 | High-speed SD via J6 connector |

**SDIO J6 Connector Mapping:**
- **CLK**: GPIO 18
- **CMD**: GPIO 19  
- **DAT0**: GPIO 20
- **DAT1**: GPIO 21
- **DAT2**: GPIO 22
- **DAT3**: GPIO 26
- **GPIO 27**: Available for expansion

## Features Implemented

### 1. SDIO SD Card Interface (`sdio/`)
- **High-speed 4-bit SDIO protocol**
- **50MHz clock for data transfer (increased for RP2350 performance)** 
- **32kB chunk writes** (64 x 512-byte blocks, optimized for RP2350 RAM limits)
- **Stress testing capabilities**
- **Commands:**
  - `sdio init` - Initialize SD card
  - `sdio status` - Show SDIO status
  - `sdio test [chunks]` - Stress test with N chunks

### 2. Signal Processing (`dsp/`)
- **Envelope detection** for RF signals
- **Decimation** (2x, 4x, 8x, 16x)
- **Moving average and IIR filters**
- **Real-time processing** optimized for streaming
- **Commands:**
  - `dsp init` - Initialize DSP engine
  - `dsp test` - Test with current ADC buffer
  - `dsp status` - Show DSP configuration

### 3. Parallel Pipeline (`pipeline/`)
- **30μs acquisition cycles** (configurable)
- **Multi-buffer system** (ping-pong buffering)
- **Dual-core operation** (Core 0: acquisition, Core 1: SD writing)
- **Real-time statistics** and monitoring
- **Commands:**
  - `pipeline init` - Initialize pipeline
  - `pipeline start [iterations]` - Start acquisition (0=continuous)
  - `pipeline stop` - Stop pipeline
  - `pipeline status` - Show detailed status
  - `pipeline test [seconds]` - Run stress test

## Usage Examples

### Basic Testing Sequence

1. **Initialize all systems:**
   ```
   run> sdio init
   run> dsp init  
   run> pipeline init
   ```

2. **Test individual components:**
   ```
   run> start acq
   run> read
   run> dsp test
   run> sdio test 1
   ```

3. **Run full pipeline:**
   ```
   run> pipeline start 1000     # 1000 iterations
   run> pipeline status         # Monitor progress
   run> pipeline stop          # Stop if needed
   ```

### Continuous High-Speed Acquisition

```
run> pipeline start 0          # Continuous mode
run> pipeline status           # Check performance
run> pipeline stop             # Stop when done
```

### Stress Testing

```
run> pipeline test 30          # 30-second stress test
run> sdio test 100            # Test 100x 128kB writes
```

## Performance Targets

- **Acquisition Rate:** 30μs cycles = ~33.3 kHz acquisition rate
- **ADC Sampling:** 120 MHz, 3600 samples per 30μs window
- **Decimation:** 4x reduction → ~900 samples output per cycle
- **Output Rate:** ~30 Msps input → ~7.5 Msps output → ~12 MB/s with compression
- **SD Write Speed:** Up to 12 MB/s sustained

## Signal Processing Pipeline

```
Raw ADC (16-bit) → Envelope Detection → Decimation → Compression → SD Storage
     ↓                    ↓                ↓             ↓            ↓
  120 Msps           Rectify+LPF        4x downsample   8-bit     32kB chunks
```

### Envelope Detection Algorithm

Based on your RF signal example:
1. **Rectification:** Take absolute value of signal
2. **Peak Detection:** Track signal envelope peaks  
3. **Exponential Decay:** Smooth envelope following
4. **Decimation:** Reduce sample rate while preserving envelope shape

## Multi-Core Architecture

### Core 0 (Primary)
- **ADC acquisition** via PIO state machines
- **Signal processing** (envelope detection, filtering)
- **Buffer management** (ping-pong buffers)
- **Command interface** and system control

### Core 1 (SD Writer)
- **Background SD writing** of completed buffers
- **Non-blocking operation** to avoid acquisition stalls
- **Error handling** and retry logic
- **Statistics tracking**

## PIO Resource Allocation

- **PIO0:** ADC + Pulse generators (3 SMs used)
- **PIO1:** MAX14866 + DAC (2 SMs used)
- **PIO2:** SDIO interface (3 SMs used)
- **Total:** 8/12 state machines used

## Expected Performance

With proper tuning, the system should achieve:
- **12 Msps continuous output** at 8-bit resolution
- **~12 MB/s SD write speed** (near theoretical SDIO limit)
- **<1% acquisition overruns** in continuous mode
- **Real-time envelope detection** with ~4x compression ratio

## Troubleshooting

### Common Issues

1. **SD Card not detected:**
   - Check SDIO wiring on J6 connector (pins 18-22, 26)
   - Verify CLK=18, CMD=19, DAT0-3=20,21,22,26
   - Try different SD card (Class 10+ recommended)
   - Run `sdio init` and check for errors

2. **Acquisition overruns:**
   - Reduce acquisition rate or increase decimation
   - Check if SD writing is keeping up
   - Monitor with `pipeline status`

3. **Low write speed:**
   - Verify SD card speed class (Class 10+ recommended)
   - Check for fragmentation  
   - Try different chunk sizes
   - Ensure 50MHz SDIO clock is supported by your SD card

4. **Memory allocation failures:**
   - "Pipeline: Failed to allocate buffer X" indicates insufficient RAM
   - RP2350 has ~520KB total RAM, buffers now optimized to 32KB each
   - If still failing, reduce PIPELINE_MAX_BUFFERS in pipeline.h
   - Monitor total memory usage during initialization

### Performance Monitoring

Use these commands for real-time monitoring:
```
run> pipeline status    # Overall system status
run> dsp status        # DSP performance metrics  
run> sdio status       # SD card interface status
```

## Next Steps

1. **Calibration:** Tune acquisition timing for your specific signals
2. **Optimization:** Adjust decimation factors for optimal compression
3. **Analysis:** Implement additional signal processing algorithms
4. **Storage:** Add filesystem support for easier data management

---

## pic0rick Board Compatibility

This implementation is fully compatible with the [pic0rick board](https://github.com/kelu124/pic0rick) design:
- **ADC inputs** use pic0rick's existing GPIO 0-10 connections
- **Pulser outputs** use pic0rick's GPIO 11, 16 assignments  
- **SDIO interface** uses pic0rick's J6 connector (pins 18-22, 26-27)
- **MUX control** relocated to avoid J6 conflicts

## Build Instructions

1. **Hardware Setup:**
   - Connect Adafruit SDIO breakout to pic0rick J6 connector
   - SDIO wiring: CLK→18, CMD→19, DAT0→20, DAT1→21, DAT2→22, DAT3→26
   - MUX control available on GPIO 12-15, 17 (if needed)
   - ADC and pulser connections remain as per pic0rick design

---

**Note:** This implementation provides the foundation for high-speed continuous data acquisition with pic0rick board compatibility. Performance will depend on SD card quality, signal characteristics, and system tuning.
