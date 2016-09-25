# solarclock

The aim of this project is to develop an LED alarm clock (DCF77 synchronized),

powered by a solarcell and rechargeable batteries.

A prototype testing for various hardware and a software with most planned
features have been completed.

With Linux, the GUI of the clock can be simulated on PC without building any
hardware. Every menu item can be changed as on the real clock, however there
is no functionality behind the GUI on the PC. Control the simulation with the four
cursor keys.

![alt text](images/linux-gui.png "Linux GUI simulation")

The prototype clock:

![alt text](images/minimatrix-wall.jpg "Real clock")

If connected to an external RS232 level converter (19200 baud, no parity,
no extra stop bits, no flowcontrol), and enabled RS232 in the debug
menu, the real clock can be controlled with the w-a-s-d keys over the serial port too.

