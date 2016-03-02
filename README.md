## 6502
This project contains the sources created while bread-boarding a 65C02 CPU from an old Apple IIc.

### Hardware
The breadboard contains the following items:

* [65C02](https://en.wikipedia.org/wiki/WDC_65C02)
* [Teensy 2.0](https://www.pjrc.com/store/teensy.html)
* 32kx8 SRAM
* 8kx8 ROM
* 16x2 LCD Display
* 74HC139, 74HC04 and 74LS03

Optional components are required when building your own programmer :)

![alt text](https://github.com/djBo/6502/raw/master/65C02_Breadboard.jpg "65C02 on a breadboard")

### Memory Map
The bottom half of RAM is occupied by a single 32kx8 SRAM ic. The top half is decoded using a 74139 2-to-4 decoder. The Teensy 2.0 micro-controller sits at `0x8000`, a 16x2 LCD sits at `0xA000`, and the 8kx8 *ROM* occupies `0xE000` to the end.

### Operation
The Teensy provides `CLK`, `RDY`, `RES` and `IRQ` signals. It also acts as a device on the bus and provides a remote terminal.

### Code
* bios.asm
  * The main assembly file to be burned on the 8kb ROM module
  * Assembles using [dev65](http://www.obelisk.me.uk/dev65/)
* IO65.ino
  * The main program to be used on the Teensy 2.0
* Programmer.ino
  * The main program to be used on a Teensy 3.1
* Programmer.java
  * To be used to `LOAD` data from the PC to the Teensy 3.1
  * `SAVE` not implemented
* Hex2bin,java
  * Simple program to convert HEX data into binary