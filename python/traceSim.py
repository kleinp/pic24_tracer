import serial
import time
import random

ser = serial.Serial('COM7', 921600, timeout=1)

a = [0x11,0x01]

for i in range(0,1000):

    if (random.random() > 0.5):
        a[0] = 0xAA
    else:
        a[0] = 0x11
    
    a[1] = random.randint(1,10)
    ser.write(a)

    time.sleep(random.uniform(0,0.1))

ser.close()