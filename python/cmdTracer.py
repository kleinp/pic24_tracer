import serial

ser = serial.Serial('COM6', 460800, timeout=15)

# a = [0x11,0x01]
# b = [0x11,0x01,0x11,0x02]
# c = [0x11,0x01,0x11,0x02,0x11,0x03]

# ser.write(a)
# ser.write(b)
# ser.write(c)
# ser.write(a)

cmd = [0xAA, 4, 0x64, 0x00, 0x00, 0x00]
ser.write(cmd)

cmd = [0xAA, 1, 0x00, 0x00, 0x00, 0x00]
ser.write(cmd)

reply = ser.read(10000)
print(reply)

ser.close()