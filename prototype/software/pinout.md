# Pinout

ADC : GPIO0 is clock, GPIO1 to GPIO10 are outputs

MCP4812 pins are :
- MOSI : GPIO15
- CS   : GPIO11
- CLK  : GPIO14


SDCards:
- CMD: GPIO26
- CLK: GPIO22
- DAT: GPIO16 (DAT0) to GPIO19 (DAT3)

Pulsers: P+ GPIO27, Pdamp GPIO28


If running remotely

```
 rm app.uf2
 curl -L -o app.uf2 "https://drive.google.com/uc?export=download&id=XXXX"
 picotool load -v -f app.uf2 
 sudo screen /dev/ttyACM0
```