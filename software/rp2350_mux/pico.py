import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import serial, glob
import datetime


class Pic0rick():

    def __init__(self):
        PORT = glob.glob("/dev/ttyACM*")[0]
        self.ser=serial.Serial(PORT,115200,timeout=0.2)
        self.ser.baudrate=115200
        print("Device on",PORT)

    def pulse(self, pon: int=200,poff:int=200,damp:int=2000):
        self.ser.write(bytearray("start acq "+str(pon)+" "+str(poff)+" "+str(damp)+"\n",'ascii'))
        A = self.ser.readline()
        B = self.ser.readline()
        C = self.ser.readline()
        D = self.ser.readline()
        return D

    def dac(self, gain=200):
        self.ser.write(bytearray("write dac "+str(gain)+"\n",'ascii'))
        A = self.ser.readline()
        B = self.ser.readline()
        C = self.ser.readline()
        return B

    def read(self):
        self.ser.write(bytearray('read\n','ascii'))
        A = self.ser.readline()
        B = self.ser.readline()
        C = self.ser.readline()
        D = self.ser.readline()
        S = [x.replace("b'","") for x in str(C).split(",") if len(x)]
        print(len(S))
        signal = [(int(x,16)-512)/512.0 for x in S[:-1]]
        return signal
    
    def setmux(self, mux):
        wri = bytearray("write mux "+str(mux)+"\n",'ascii')
        #print("MUX: \t",wri)
        self.ser.write(wri)
        A = self.ser.readline()
        B = self.ser.readline()
        C = self.ser.readline()
        D = self.ser.readline()
        #print(A,B,C,D)

    def set(self):
        self.ser.write(bytearray("set mux\n",'ascii'))
        A = self.ser.readline()
        return A
    
    def clr(self):
        self.ser.write(bytearray("clear mux\n",'ascii'))
        A = self.ser.readline()
        return A


def pplot(signal, f=64.0, G=200):
    now = datetime.datetime.today().strftime('%Y%m%d%H%M%S')
    t = [x/f for x in range(len(signal))]
    f = [k*f/len(signal) for k in range(len(signal))]

    data = {"signal":signal,"t":t,"f":f,"gain":G,"timestamp":now}
    m=800
    FFT = np.abs(np.fft.fft(signal))
    plt.figure(figsize=(20,10))
    plt.subplot(2, 1, 1)

    plt.plot(t[10:],signal[10:],"b",label="Signal")
    plt.xlabel("us")
    plt.ylabel("Amplitude")
    plt.legend()
    title = "Acquisition at gain = " + str(G)

    plt.subplot(2, 2, 3)
    plt.plot(t[38*60:42*60],signal[38*60:42*60],alpha=0.3,label="Signal")
    plt.title('Pulse waveform')
    plt.ylabel('V')
    plt.ylabel('us')
    plt.legend()

    plt.subplot(2, 2, 4)
    plt.title('Spectrum composition')
    plt.plot(f[25:len(FFT)//2],FFT[25:len(FFT)//2])
    plt.xlabel('Freq (MHz)')
    plt.ylabel('Energy')

    plt.suptitle(title)
    plt.tight_layout()