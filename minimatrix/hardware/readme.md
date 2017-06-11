# About the hardware
Version 1.0 is the prototype PCB actually produced.

However it contains a LOT of bugs, so before making your own one, I recommend reading through the bug list and make a new version, starting from the current state of version 1.2.

Please note the new designed ir-sensor board which fits in connector SV2. The ones attached on the v1.0 board simply do not work.

The yellow wires in version 1.1 and 1.2 have been added to the PCB of version 1.0 by wireing on the patchfield.

Genreally, I had two PCBs of version 1.0 produced. The first one is soldered as in schematic version 1.1 and the second one as in schematic 1.2.

Version 1.2 has been improved a bit from version 1.1 and some parts soldered in version 1.1 (but not used there or considered bad) have been removed in version 1.2.

## Userinput

There are two input methods available. Touch and infrared proximity. The software will detect which type is used on startup automatically. Each has its strength and weaknesses. The software could be changed to use classic buttons as third input method with little effort.

## So for the overview of the files:
The brd and sch files are created with Eagle version 5.11 light. The pdf files have been exported from these for easy viewing without the software.

minimatrixV1 brd/sch/pdf: Originally produced PCB, later manually patched to meet the schematics of version 1.1 and 1.2

minimtrixV1-1 brd/sch/pdf: First clock build (the green one). Fixes have been added into the schematic, but not in the board. So you might modify the board before production.

minimtrixV1-2 brd/sch/pdf: Second clock build (the red one). Fixes have been added into the schematic, but not in the board. So you might modify the board before production.

ir-sensorboard brd/sch/pdf: Touchless keys working by infrared reflection sensors. Used on the first clock. Good for use in a kitchen or some other place where you dont want to touch anything. Do not work in bright light, and might get activated by bright flashes by accident. In the daylight they cant be used.

touch-sensorboardW brd/sch/pdf: Use the body capacity as input key. Used on the second clock. Hopefully the ESD protection is sufficient.

solarvoltagelimit brd/sch/pdf: Two schematics and PCBs used for overvoltage and overcurrent protection. The circuit for overcurrent protection is only required for solarcells who can deliver more than 200mA peak. Instead of producing a special PCB, they can be soldered on common perfboard too. Note that the PCBs in these files have never been produced and therefore never tested.

solarclock-touch-voltagelimit brd/pdf: A board from touch-sensorboardW and the voltage limit PCB of solarvoltagelimit combined with predefined breaking points for cheap production as single board.

## License

All my work is published under Attribution-NonCommercial-ShareAlike 4.0 International, however please note that many symbols are work from the authors of the part libraries.

Many symbols are from the Eagle authors.

avr-7.lbr from https://cadsoft.io/resources/libraries/1751/ has been used

And the RFM12 device is from https://www.mikrocontroller.net/topic/90021
