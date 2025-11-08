# PIO SDIO for Raspberry Pi PICO example

This is an standalone example of the [bench](https://github.com/greiman/SdFat/tree/master/examples/bench) code from [SdFat](https://github.com/greiman/SdFat) using an SDIO PIO implementation from [ZuluSCSI](https://github.com/ZuluSCSI/ZuluSCSI-firmware)

This fork added some changes to improve large write performance.

Current numbers with a supposedly good SD Card and clock speed 250MHz:

```
Type is exFAT
Card size: 128.178 GB (GB = 1E9 bytes)

Manufacturer ID: 0X1B
OEM ID: SM
Product: ED2S5
Revision: 3.0
Serial number: 0XB210678A
Manufacturing date: 10/2022

FILE_SIZE_MB = 50
BUF_SIZE = 32768 bytes
Starting write test, please wait.

write speed and latency
speed,max,min,avg
KB/Sec,usec,usec,usec
26058.1,24395,1216,1254
26027.0,16960,1216,1256

Starting read test, please wait.

read speed and latency
speed,max,min,avg
KB/Sec,usec,usec,usec
24702.6,1395,1313,1326
24702.6,1392,1313,1326

Done
```

## Build
```
<BUILD_DIR> $ PICO_SDK_PATH=<PATH_TO_SDK> cmake <PATH_TO_SOURCE>
```

Target code will output bench results via UART and save results in `bench.dat` file on SD card root.
