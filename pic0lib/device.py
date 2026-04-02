# Main libs
import os
import sys
import glob
import time
import struct
# Third party
import serial
import numpy as np


def _find_port():
    if sys.platform.startswith("win"):
        ports = glob.glob("COM[0-9]*")
    elif sys.platform.startswith("linux"):
        ports = glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*")
    elif sys.platform.startswith("darwin"):  # macOS
        ports = glob.glob("/dev/tty.usbmodem*") + glob.glob("/dev/tty.usbserial*")
    else:
        raise OSError(f"Unsupported platform: {sys.platform}")

    if not ports:
        raise OSError("No serial device found")
    return ports[0]


def pprint(ans):
    return "".join(b.decode("utf-8") for b in ans)


class Pic0rick:

    def sread(self):
        done = False
        ans = []
        while not done:
            res = self.ser.readline()
            ans.append(res)
            if res == b"":
                done = True
        if self.verbose:
            pprint(ans)
        if self.log:
            with open(self.log_file, "a") as f:
                for line in ans:
                    if line and line != b"":
                        f.write(line.decode("utf-8", errors="replace"))
        return ans

    def __init__(self, port=None, verbose=True, logging=False, log_file=".log"):
        self.verbose = verbose
        self.log = logging
        self.log_file = log_file

        # Detect the port
        port_device = port if port is not None else _find_port()
        print("Device on", port_device)

        self.ser = serial.Serial(port_device, 115200, timeout=0.2)
        self.ser.baudrate = 115200
        time.sleep(1)  # wait for the serial connection to initialize
        self.sread()


    def dac(self, N):
        """Write a value to the 10-bit DAC MCP4812 (write dac).

        Args:
            N: DAC value (10-bit).
        """
        self.ser.write(bytearray("write dac " + str(N) + "\n", "ascii"))
        ans = self.sread()
        return ans
    
    def read(self):
        self.ser.write(bytearray("read\n", "ascii"))
        ans = self.sread()
        return ans
    
    def pulse_adc_trigger(self, pon: int=200,poff:int=200,damp:int=2000):
        self.ser.write(bytearray("start acq "+str(pon)+" "+str(poff)+" "+str(damp)+"\n",'ascii'))
        ans = self.sread()
        return ans

