# Setup

## Physical assembly

### Board once assembled with headers

![](/documentation/images/v2/20250323_114717.jpg)

### Checking the pulser pins alignments


![](/documentation/images/v2/20250323_114809.jpg)

### Stacking the high voltage module on the pulser

#### From the top

![](/documentation/images/v2/20250323_115024.jpg)

#### From the bottom


![](/documentation/images/v2/20250323_115016.jpg)

### And final connection to a piezo

![](/documentation/images/v2/20250323_114927.jpg)


## Poweruse 

* Around 90 mA 


## Programming

### Using the code as is (simpler)

You will surely know how to program a pico, or pico2. Just keep the RESET button held down when plugging it in, then copy the uf2 file into the mass storage that will appear. It will reboot automatically once programmed.  

This is the [uf2 file for rp2040](rpXXXX-common_rp2040.uf2) and the [uf2 file for rp2350](rpXXXX-common_rp2350.uf2)  that you will use (these versions alleviate a former stalling issue).


### Using the source files

The code lives in:
* [software/rp2350/rp2350_simple](software/rp2350_simple) for the pico2
* [software/rp2040_shell](software/rp2040_shell) for the pico1 
* Playing with rp2040 (pico1) and the VGA PMOD is : [code is there](software/rp2040_vga)

Options to compile:

* See [this page](https://learn.adafruit.com/rp2040-arduino-with-the-earlephilhower-core/connecting-your-rp2040) for more information on how to use the Arduino IDE.
* Work in progress for VSCode

## Using it (and resukts)

The code today allows you to connect to the board via a COM port, that you can see for example when typing "dmesg" in linux after plugging the board in (after you flashed it)

### Video

[![Video of pic0rick](https://img.youtube.com/vi/2a3_D-hZEio/0.jpg)](https://www.youtube.com/watch?v=2a3_D-hZEio)


### Using python

[See this notebook as an example](rp2350_simple/Readme.ipynb): python runs along the lines "just" a wrapper around the CLI interface

It will give you data like below:

![](/software/imgs/rp2350/pic0gain_at_2.jpg)

### Using the CLI

Connect to the shell via for example screen /dev/ttyACM0 (if your pico shows on ttyACMO).

Commands available through the CLI are

* start acq X1 X2 X3 -------> X1 is PP in ns, X2 is PN in ns, X3 is PD in ns this starts the acquisitions syncing with the pulse.
* write dac X -------> X is the dac number I forgot the min and the max but it may be from 0 to 400.
* read -------> read the obtained acquisitions (8000kpts)

And, if you have a MUX (and the good firmware):

* write mux XXXX ---> where XXXX is the word to be sent to the MAX14866.
* clear mux -- opens every switch
* set mux -- closes every switch
