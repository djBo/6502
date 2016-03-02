/*
  Programmer v1.0

  - Teensy 3.1
  - dual MCP23017
  - bi-directional Logic Level Converter

  Used for a multitude of memory ic's:
  - Dallas DS1225AD-170 Non-volatile Nonvalotaile-SRAM 8Kx8
  - ST M48T08-150 TimeKeeper™ Nonvalotaile-SRAM 8Kx8 (special case)
  - Generic 8k to 32k SRAM validation
  - More to come

  Connections:
  - Teensy pin #0   -> Chip Enable (level converter)
  - Teensy pin #1   -> Output Enable (level converter)
  - Teensy pin #18  -> I2C Data (level converter)
  - Teensy pin #19  -> I2C Clock (level converter)
  - Teensy 3v3      -> level converter
  - MCP23017-0 GPA  -> A0..A7
  - MCP23017-0 GPB  -> A8..A12
  - MCP23017-0 GPB7 -> Write Enable (eg. GPB7)
  - MCP23017-1 GPB  -> D0..D7
  - MCP23017-1 GPA  -> Reserved (for A16..A20)

*/

#include "Arduino.h"
#include <Wire.h>
#include <math.h>
#include "Adafruit_MCP23017.h"
#include "Blink.h"

// Teensy 3.1 restart
#define RESTART_ADDR		0xE000ED0C
#define READ_RESTART()		(*(volatile uint32_t *)RESTART_ADDR)
#define WRITE_RESTART(val)	((*(volatile uint32_t *)RESTART_ADDR) = (val))

#define MAX_BUF_SIZE		32768
#define LEN			32	// Number of bytes per line

enum State {
	NONE, // No state (the default)
	HELP, // Shows command info
	SIZE, // Shows buffer size
	BITS, // Set buffer bits 13-15
	LOAD, // Load data from UART into buffer
	READ, // Load data from IC into buffer
	SHOW, // List current buffer contents
	SAVE, // Send data from buffer to UART
	PROG, // Send data from buffer to IC
	NOPS, // Write 0xEA into buffer and set start address to 0xE000
	TEST, // Write 0xAA55 into buffer
	PEEK, // Read byte at location
	POKE, // Write byte at location
	ZERO  // Clear buffer

	//SET
	// CE2
	// 8k
	// 16k
	// 32k
	// Etc.
};
State state 		= NONE;

// This section alone is for the serial command line :(
static uint8_t uart_buffer[20];	
String uart_string;
String cmd_string;
bool uart_ready		= false;
uint8_t uart_count	= 0;

int CE			= 0;	// Chip Enable (pin 0) (any pin on your arduino)
int OE			= 1;	// Output Enable (pin 1) (any pin on your arduino)
int WR			= 15;	// Write Enable (15 on ext1)
int INT			= 14;	// Optional interrupt pin (14 on ext1)
int CE2			= 13;	// Optional enable #2 (13 on ext1)
int bits		= 13;	// Number of address lines connected (13 to 15, supporting 8k to 32k)
bool enable_int		= false;
bool enable_ce2		= false;
unsigned long max	= (unsigned long) (1UL << bits);
unsigned long count	= 0;
byte* buffer = 0;

Blink blink;

// This entire section belongs to the reader/writer code...
Adafruit_MCP23017 ext1;
Adafruit_MCP23017 ext2;

/*
	Basic IC pinout
	  ----u----
  1   A14   Vcc   28
  2   A12    WR   27
  3   A7    A13   26
  4   A6     A8   25
  5   A5     A9   24
  6   A4    A11   23
  7   A3     OE   22
  8   A2    A10   21
  9   A1     CE   20
 10   A0     D7   19
 11   D0     D6   18
 12   D1     D5   17
 13   D2     D4   16
 14   GND    D3   15
	  ---------
*/

/****************************************************************************/
void setup() {
	blink.set(250, 250);

	pinMode(CE, OUTPUT);
	pinMode(OE, OUTPUT);
	digitalWrite(CE, HIGH); // Disable chip
	digitalWrite(OE, HIGH); // Disable output

	Serial.begin(9600);
	while (!Serial) {
		blink.blink();
	}
	blink.set(50, 950);

	buffer = (byte*) malloc(MAX_BUF_SIZE * sizeof(byte)); // Just once!
	clearBuffer();

	Serial.println(F("Programmer v1.0 (") + getSize() + F(")"));
	Serial.println();

	ext1.begin(0);
	ext2.begin(1);

	for (int i = 0; i < 16; i++) {
		ext1.setPinMode(i, OUTPUT);
		if (i < 8) ext2.setPinMode(i, OUTPUT);
		else ext2.setPinMode(i, INPUT);
	}

	if (WR != -1 && bits <= WR) {
		ext1.writePin(WR, HIGH); // Disable write
	}
}

void loop() {
	if (Serial) {
		switch (state) {
			case HELP:
				stateHelp();
				break;
			case LOAD:
				stateLoad();
				break;
			case READ:
				stateRead();
				break;
			case SHOW:
				stateShow();
				break;
			case SAVE:
				stateSave();
				break;
			case PROG:
				stateProg();
				break;
			case NOPS:
				stateNops();
				break;
			case TEST:
				stateTest();
				break;
			case ZERO:
				stateZero();
				break;
			case PEEK:
				statePeek();
				break;
			case POKE:
				statePoke();
				break;
			case SIZE:
				stateSize();
				break;
			case BITS:
				stateBits();
				break;
			default:
				stateNone();
		}
	} else {
		WRITE_RESTART(0x5FA0004);
	}
}
/****************************************************************************/
void serialHandler(String s) {
	if (state == NONE) {
		if (s == "HELP") state = HELP;
		if (s == "LOAD") state = LOAD;
		if (s == "READ") state = READ;
		if (s == "SHOW") state = SHOW;
		if (s == "SAVE") state = SAVE;
		if (s == "SIZE") state = SIZE;
		if (s == "PROG") state = PROG;
		if (s == "NOPS") state = NOPS;
		if (s == "TEST") state = TEST;
		if (s == "ZERO") state = ZERO;
		if (s.startsWith("BITS")) state = BITS;
		if (s.startsWith("PEEK")) state = PEEK;
		if (s.startsWith("POKE")) state = POKE;

		if (state == NONE) Serial.println(F("? ") + s);
		else {
			cmd_string = s;
			if (state == LOAD) Serial.print(F("LOAD "));
			else Serial.println();
		}
	}
}

void serialEvent() {
	while (Serial.available() > 0) {
		// get the new byte:
		uint8_t uart_char = (uint8_t)Serial.read();
		if (state == LOAD) {
			int len = max >> 11; // >>6 & >>5
			buffer[count] = uart_char; 
			count++;

			// This division need to come down to 64 items per line!
			// Whether it be bytes, blocks or whatever, using max.
			if (count % (LEN*len) == 0) {
				// At the end of LEN*4 bytes, we append a dot
				Serial.print(".");
			}

			if (count == max) {
				Serial.println(F(" DONE"));
				Serial.println();
				state = NONE;
				break;
			}
		} else {
			if (!uart_ready) {
				if (uart_char == '\n') {
					// if the incoming character is a newline, set a flag
					// so the main loop can do something about it
					uart_count--;
					uart_ready = true;
				} else {
					if (uart_count > 19) {
						uart_count--;
						uart_ready = true;
					} else {
						// add it to the uart_buffer
						uart_buffer[uart_count] = uart_char;
						uart_count++;
					}
				}
			}
		}
	}
}

void serialReady() {
	uint8_t uart_buffer_len = uart_count + 1;

	uart_string = "";
	for (int i=0; i < uart_buffer_len; i++) {
		if (i < uart_buffer_len)
			uart_string += (char)uart_buffer[i];
		uart_buffer[i] = ' ';
	}

	// reset the flag and the index in order to receive more data
	uart_count    = 0;
	uart_ready = false;

	serialHandler(uart_string);
}
/****************************************************************************/
void clearBuffer() {
	for (int i = 0; i < MAX_BUF_SIZE; i++) {
		buffer[i] = 0;
	}	
}
/****************************************************************************/
void stateNone() {
	// Check for completed UART data
	if (uart_ready)  {
		serialReady();
	}
	
	// Check for incoming UART data
	if (Serial.available()) {
		serialEvent();
	}
	
	blink.blink();
}

void stateHelp() {
	//Serial.println(F(""));
	Serial.println(F("HELP	Displays this message"));
	Serial.println(F("SIZE	Show buffer size"));
	Serial.println(F("BITS	Set buffer bits (13-15)"));
	Serial.println(F("READ	Load data from IC into buffer"));
	Serial.println(F("PROG	Send data from buffer to IC"));
	Serial.println(F("LOAD	Load data from UART into buffer (*)"));
	Serial.println(F("SAVE	Send data from buffer to UART (*)"));
	Serial.println(F("PEEK	Get byte at location (**)"));
	Serial.println(F("POKE	Set byte at location (**)"));
	Serial.println(F("NOPS	Fill buffer with 0xEA, and set RST to 0xE000"));
	Serial.println(F("TEST	Fill buffer with 0xAA55"));
	Serial.println(F("ZERO	Clear buffer"));
	Serial.println();
	Serial.println(F("(*) :	Requires Programmer.jar"));
	Serial.println(F("(**):	Performed on IC directly"));
	Serial.println();
	state = NONE;
}

void stateSize() {
	Serial.println(F("Size: ") + getSize());
	Serial.println();
	state = NONE;
}

void stateBits() {
	int p = cmd_string.indexOf(" ");
	if (p != -1) {
		String s = cmd_string.substring(p+1);
		unsigned int val = strtoul(s.c_str(), 0, 10);
		if ((val >= 13) && (val <= 15)) {
			Serial.print(F("Bits: "));
			if (bits != val) {
				Serial.println(String(val, DEC) + F(" OK"));
				clearBuffer();
				bits = val; // Number of address lines connected (13 to 15, supporting 8k to 32k)
				max = (unsigned long) (1UL << bits);
				enable_int = false;
				enable_ce2 = false;
			} else {
				Serial.println(String(val, DEC));
			}
		} else {
			Serial.println(F("Invalid bits ") + String(val, DEC));
		}
	} else {
		Serial.println(F("BITS missing value"));
	}
	Serial.println();
	state = NONE;
}

void stateLoad() {
	if (Serial.available()) {
		serialEvent();
	}
	blink.blink();
}

void stateRead() {
	readCycle();
	state = NONE;
}

void stateShow() {
	showCycle();
	state = NONE;
}

void stateSave() {
	state = NONE;
}

void stateProg() {
	progCycle();
	state = NONE;
}

void statePeek() {
	int p = cmd_string.indexOf(" ");
	if (p != -1) {
		String s = F("0x") + cmd_string.substring(p+1).toUpperCase();
		unsigned long addr = strtoul(s.c_str(), 0, 16);
		if (addr >= max) Serial.println(F("Invalid location: ") + s);
		else {
			setPins(INPUT);
			Serial.print(s + F(": 0x"));
			buffer[addr] = readByte(getLowerAddress(addr));
			Serial.println(_hex(buffer[addr], 8));
		}
	} else {
		Serial.println(F("PEEK missing location"));
	}
	Serial.println();
	state = NONE;
}

void statePoke() {
	int p = cmd_string.indexOf(" ");
	if (p != -1) {
		String s = cmd_string.substring(p+1);
		p = s.indexOf(" ");
		if (p != -1) {
			String s1 = F("0x") + s.substring(0, p).toUpperCase();
			String s2 = F("0x") + s.substring(p+1).toUpperCase();
			unsigned long addr = strtoul(s1.c_str(), 0, 16);
			if (addr >= max) Serial.println(F("Invalid location: ") + s1);
			else {
				long unsigned int val = strtoul(s2.c_str(), 0, 16);
				if (val > 255) Serial.println(F("Invalid value: ") + s2);
				else {
					setPins(OUTPUT);
					Serial.print(s1 + F(": ") + s2 + F(" "));
					writeByte(getLowerAddress(addr), byte(val));
					buffer[addr] = val;
					Serial.println(F("OK"));
					Serial.println();
				}
			}
		} else {
			Serial.println(F("POKE missing value"));
		}
	} else {
		Serial.println(F("POKE missing location"));
	}
	state = NONE;
}

void stateNops() {
	for (unsigned long addr = 0; addr < max; addr++) {
		if (addr == (max - 4)) buffer[addr] = 0x00;
		else if (addr == (max - 3)) buffer[addr] = 0xE0;
		else buffer[addr] = 0xEA;
	}
	state = NONE;
}

void stateTest() {
	for (unsigned long addr = 0; addr < max; addr++) {
		if (addr % 2 == 0) buffer[addr] = 0xAA;
		else buffer[addr] = 0x55;
	}
	state = NONE;
}

void stateZero() {
	clearBuffer();
	state = NONE;
}
/****************************************************************************/
void progCycle() {
	int len = LEN * 4;
	setPins(OUTPUT);

	for (unsigned long addr = 0; addr < max; addr++) {
		unsigned long loc = addr;

		if (addr == 0) {
			// On multiples of LEN*4, we prepend the current address
			Serial.print(_hex(addr, bits));
			Serial.print(F(" "));
			Serial.flush();
		}

		writeByte(getLowerAddress(loc), buffer[addr]);

		if (addr % len == (len-1)) {
			// At the end of LEN*4 bytes, we append a dot
			Serial.print(F("."));
			Serial.flush();
			delay(100);
			blink.once();
		}

		if (addr == (max-1)) {
			// At the end of max bytes, we append the current address
			Serial.print(F(" "));
			Serial.println(_hex(addr, bits));
			Serial.println();
			Serial.flush();
		}
	}
}

void readCycle() {
	int len = LEN * 4;
	clearBuffer();
	setPins(INPUT);

	for (unsigned long addr = 0; addr < max; addr++) {
		unsigned long loc = addr;

		if (addr == 0) {
			// On multiples of LEN*4, we prepend the current address
			Serial.print(_hex(addr, bits));
			Serial.print(F(" "));
			Serial.flush();
		}

		buffer[addr] = readByte(getLowerAddress(loc));

		if (addr % len == (len-1)) {
			// At the end of LEN*4 bytes, we append a dot
			Serial.print(F("."));
			Serial.flush();
			delay(100);
			blink.once();
		}

		if (addr == (max-1)) {
			// At the end of max bytes, we append the current address
			Serial.print(F(" "));
			Serial.println(_hex(addr, bits));
			Serial.println();
			Serial.flush();
		}
	}
}

void showCycle() {
	for (unsigned long addr = 0; addr < max; addr++) {
		unsigned long loc = addr;

		//TODO:Make this optional!
		if (addr % LEN == 0) {
			// On multiples of LEN, we prepend the current address
			Serial.print(_hex(addr, bits));
			Serial.print(F(" "));
		}

		Serial.print(_hex(buffer[addr], 8));

		//TODO:Make this optional!
		if (addr % LEN == (LEN-1)) {
			// At the end of len bytes, we append the current address
			Serial.print(F(" "));
			Serial.println(_hex(addr, bits));
			Serial.flush();
		}
	}
	Serial.println();
}
/****************************************************************************/
unsigned int getLowerAddress(unsigned long addr) {
	return addr & 0xFFFF;
}

unsigned int getUpperAddress(unsigned long addr) {
	return addr >> 16;
}

bool addressCheck(unsigned long addr) {
	addr |= 0x8000; // Set WR to HIGH always
	if (bits == 13) {
		if (enable_int) {
			// 0x4000;
			// Need it as an INPUT, with INTERNAL PULLUP (if possible)
		}
		if (enable_ce2) {
			addr |= 0x2000;
		}
	}
	return addr;
}

byte readByte(unsigned int addr) {
	byte b;

	addr = addressCheck(addr);

	ext1.writeGPIOAB(addr);

	// Step 3: Enable the chip and output
	digitalWrite(CE, LOW);
	digitalWrite(OE, LOW);

	// Step 4: Read the output from ext2.GPIOB
	b = ext2.readGPIOB();

	// Step 5: Turn off output and chip
	digitalWrite(OE, HIGH);
	digitalWrite(CE, HIGH);

	return b;
}

void writeByte(unsigned long addr, byte b) {
	//if (WR != -1 && bits <= WR) loc = loc + (0x1 << WR);
	addr = addressCheck(addr);

	ext1.writeGPIOAB(addr);

	// Step 2: Write the data byte to ext2.GPIOB
	ext2.writeGPIOB(b);

	// Step 3: Enable the chip and input
	digitalWrite(CE, LOW);
	ext1.writePin(WR, LOW);

	// Step 4: Sleep for 1ms.
	//delay(1);

	// Step 5: Disable the input and chip
	ext1.writePin(WR, HIGH);
	digitalWrite(CE, HIGH);
}

void setPins(int mode) {
	for (int i = 8; i < 16; i++) {
		ext2.setPinMode(i, mode);
	}	
}
/****************************************************************************/
String _hex(unsigned long v, int num_places) {
	int mask=0, n, num_nibbles, digit;

	for (n=1; n<=num_places; n++) {
		mask = (mask << 1) | 0x0001;
	}
	v = v & mask; // truncate v to specified number of places

	num_nibbles = num_places / 4;
	if ((num_places % 4) != 0) {
		++num_nibbles;
	}

	String s = "";
	do {
		digit = ((v >> (num_nibbles-1) * 4)) & 0x0f;
		s += String(digit, HEX);
	} while (--num_nibbles);
	s.toUpperCase();
	return s;
}

String getSize() {
	return F("2^") + String(bits, DEC) + F(" = ") + String(max, DEC) + F(" bytes");
}
/****************************************************************************/