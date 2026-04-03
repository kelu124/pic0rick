
#!/bin/bash
set -e

# Load .env file
source .env

mkdir -p build
cd build

cmake -DWIFI_SSID="$WIFI_SSID" -DWIFI_PASSWORD="$WIFI_PASSWORD" ..
make -j4

cp rp2350_daq.uf2 ../rp2350_daq.uf2
