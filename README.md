## pic24_tracer

This code was originally meant to help debug a PIC24 running freeRTOS using the trace macros. On the PIC side, it simply sent out 2 bytes of information on a UART, the first byte being the event type (freeRTOS has 30 something macros) and the second byte being an application tag given to each task and ISR. I wrote this code to save timer resources and bytes to transmit on the PIC side by timestamping incoming serial data on the stm microcontroller and then forwarding clumps of data with timestamp. The idea was that this information could be used to construct some figures showing CPU utilization. I decided that my work is just going to pony up for a license of https://percepio.com/tz/freertostrace/ rather than me spending several more weekends getting a rudimentary GUI working.

This code is meant to run on a NUCLEO-L432KC. The incoming serial data needs to be connected to CN3 pins 1 & 2. CN3 pin 12 needs to be jumpered to CN4 pin 8. The timestamp+data packets are sent out to the computer via the built in virtual COM port. 

Note that you have to send a command to the nucleo for it to start sending timestamp+data and that there are a few commands that let you configure baud rates, etc.

The code currently timestamps every 2 received bytes, but could be easily adapted to timestamp every byte. The timestamp timer is running at 40MHz, so resolution is pretty good.

There is more documentation on concept of operation (multiple timers are being used to do the timestamping using DMA) in `main.c`