# Project

The resources the program uses are as follows
* CPU -> 1 Core
* RAM -> Total memory usage: 225,180 bytes (225KB)
* PIO -> 2 PIOs --> PIO0 (1 State Machine) -> to get data from ADC and PIO1 (3 State Machines)-> to draw the graph on the VGA
* PINS -> 10 pins for ADC 3 PINS for DAC and 3 PINS for VGA

## Design 

* Tool: KICAD
* Dimension of the board: standard credit card
* Layers: up to four
* Passives: using 1206 as much as possible, spaced layout where feasible, clear silkscreen.

## Organisation

![](pic0rick.jpg)

## Components

* Central, pico, to solder on the board
* Acquisition - 14 IOs used.
  * MD0100: AFE protection
  * AD8331: Variable gain (3 SPI IO)
  * (DAC to control AD8331 gain)
  * Acquisition: ADC10065, 10bits ADC (10IO + 1clk)
  
* Other side - exposes remaining 12 IOs.
  * PMOD extension with free IOs from PICO - reference as PMOD1A/PMOD1B (in [this picture](https://www.crowdsupply.com/img/26cc/b3ff769f-8195-40e5-88b3-47b8051c26cc/icebreaker-v1-0b-legend.jpg)), with 5V and GND in between. Contains 8 IOs each.
  * [PMODs specification sheet](https://digilent.com/reference/_media/reference/pmod/pmod-interface-specification-1_2_0.pdf) as a PDF
  * In this case, providing 1 PMOD with 4 IO, 1 with 8 IOs
  * 4 IO: design looks like in the spec sheet above, ref J2 (labeled C in red) in fig 3
  * 8 IO PMODs: design looks like in the spec sheet above, J2 (labeled D in red) in fig 4

# Previous designs to reuse:

* Front end from [this design](https://github.com/kelu124/un0rick/blob/master/hardware/MATTY-V11.pdf) (PDF only)
* 
