/*

  IO Device Emulator for 65C02 cpu

  CLK -> OUTPUT (9/PC6)
  IRQ -> OUTPUT (12/PD7)
  RDY -> OUTPUT (22/PD4)
  RES -> OUTPUT (23/PD5)
  CS  -> INPUT TRIGGER (24/PE6/INT6)
  R/W -> INPUT (10/PC7)
  A0  -> INPUT (19/PF4)
  A1  -> INPUT (18/PF5)
  A2  -> INPUT (17/PF6)
  A3  -> INPUT (16/PF7)
  A11 -> INPUT (21/PF0)
  A12 -> INPUT (20/PF1)
  D0  -> I/O (0/PB0)
  D1  -> I/O (1/PB1)
  D2  -> I/O (2/PB2)
  D3  -> I/O (3/PB3)
  D4  -> I/O (13/PB4)
  D5  -> I/O (14/PB5)
  D6  -> I/O (15/PB6)
  D7  -> I/O (4/PB7)
  
  This project was created using a Teensy 2.0 and a breadboarded 65c02 with 32k RAM and 8k ROM.
  A 74HC139/74HC04 combo take up address decoding. Teensy is at 0x8000, onboard LCD at 0xA000.
  ROM is decoded at 0xE000. This leaves 0xC000 open for either ROM or more devices.

  The Teensy is mostly responsible for generating the processor clock using Timer3 on pin 9.
  This is mostly due to lack of a 1MHZ crystal. As other parts are also missing (VIA, ACIA, CRT),
  it is up to the Teensy to provide more.

  In the current setup, it should be able to generate interrupts, control the ready/reset signal,
  and respond to read/write commands with full 8bit IO. This does take up a lot of pins of the
  Teensy, but it leaves pins 5 to 8 and the LED pin 11 free for use. Last, but not least, it
  provides a USB connection to a computer with integral serial io for terminal use.

*/
#include <avr/io.h>
#include <avr/interrupt.h>
#include "TimerThree.h"

int ledState = LOW;
long previousMillis = 0;
long interval = 500;

#define CLK 9	// PC6/OC3A
#define IRQ 10	// PC7/OC4A
#define RDY 22	// PD4
#define RES 23	// PD5
#define CS 24	// PE6/INT6
#define RW 12	// PD7
#define A0 19	// PF4
#define A1 18	// PF5
#define A2 17	// PF6
#define A3 16	// PF7
#define A11 21	// PF0
#define A12 20	// PF1

// Memory Map
// 0x8000 - 0x800F PIA
// 0x8800 - 0x8803 ACIA
// 0x8800          ACIA_DATA
// 0x8801          ACIA_STATUS
// 0x8002          ACIA_COMMAND
// 0x8003          ACIA_CONTROL
// 0x9000 - 0x9001 CRTC

volatile byte rw;
volatile byte data[][8] = {
	{0, 0, 0, 0, 0, 0, 0, 0},	// 0x8000 - 0x800F
	{0, 0, 0, 0, 0, 0, 0, 0},	// 0x8800 - 0x880F
	{0, 0, 0, 0, 0, 0, 0, 0},	// 0x9000 - 0x900F
	{0, 0, 0, 0, 0, 0, 0, 0},	// 0x9800 - 0x900F
};
volatile int addr;
volatile bool interrupted = false;

void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
	pinMode(IRQ, OUTPUT);
	pinMode(RDY, OUTPUT);
	pinMode(RES, OUTPUT);
	pinMode(CS, INPUT);
	pinMode(RW, INPUT);
	pinMode(A0, INPUT);
	pinMode(A1, INPUT);
	pinMode(A2, INPUT);
	pinMode(A3, INPUT);
	pinMode(A11, INPUT);
	pinMode(A12, INPUT);
	DDRB = 0; // All as INPUT

	digitalWrite(IRQ, HIGH);
	digitalWrite(RDY, HIGH);
	digitalWrite(RES, LOW);

	//init();

	Serial.begin(9600);

	digitalWrite(LED_BUILTIN, HIGH);
	delay(250);
	digitalWrite(LED_BUILTIN, LOW);
	delay(5000);

	Timer3.initialize(10000); // Highest usable value: 1000000
	Timer3.pwm(CLK, 512); // 50% duty cycle

	delay(2000);

//	EICRB |= (1<<ISC61)|(1<<ISC60); // Rising Edge (CS Inverted)
	EICRB |= (1<<ISC61); // Falling Edge (CS Direct)
	EIMSK |= (1<<INT6);

	digitalWrite(RES, HIGH);
}

void init() {
	digitalWrite(LED_BUILTIN, HIGH);
	delay(250);
	digitalWrite(LED_BUILTIN, LOW);
	for (int i = 0; i < 2; i++) {
		DDRB = 0xFF;
		PORTB = 0xA5;
		delay(1000);
		PORTB = 0x0;
		DDRB = 0x0;
		delay(1000);
	}
	digitalWrite(LED_BUILTIN, HIGH);
	delay(250);
	digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis > interval) {
		previousMillis = currentMillis;
		ledState = !ledState;
		digitalWrite(LED_BUILTIN, ledState);
	}
	
	if (interrupted) {
		interrupted = false;
		handleInterrupt();
		//EIMSK |= (1<<INT6);
		sei();
	}
}

ISR(INT6_vect) {
	//EIMSK &= 0xBF; // Clear INT6 ??
	cli();
	//digitalWrite(RDY, LOW);
	interrupted = true;
	rw = digitalRead(RW);
	addr = PINF; //0x8000 | ((PINF & 0x3) << 11) | ((PINF & 0xF0) >> 4);
	if (rw == 0) {
		data[addr & 0x3][(addr & 0xF0) >> 4] = PINB;
	} else {
		DDRB = 255;
		PORTB = data[addr & 0x3][(addr & 0xF0) >> 4];
		Timer3.attachInterrupt(timer_overflow);
		sei();
	}
	// Catch 22!
	// We dont really want to handle reads :( Mostly because at the
	// end of round #2, the pins of PORTB need to be reset to INPUT
	// mode. Next to that, if the Teensy is too slow, additional
	// wait-states would have to be generated, and this pretty much
	// requires an interrupt right BEFORE the end of round #2!
	// Having some cycle dependent delay could save the day ?!?
	//digitalWrite(RDY, HIGH);
}

void timer_overflow(void) {
	Timer3.detachInterrupt();
	if (rw == 1) {
		DDRB = 0;
		digitalWrite(RDY, HIGH);
	}
}

void handleInterrupt() {
	Serial.print(rw);
	Serial.print(",");
	print_hex((0x8000 | ((addr & 0x3) << 11) | ((addr & 0xF0) >> 4)), 16);
	Serial.print(",");
	print_hex(data[addr & 0x3][(addr & 0xF0) >> 4], 8);
	Serial.println();
	delay(100);
}

void print_hex(int v, int num_places) {
	int mask=0, n, num_nibbles, digit;

	for (n=1; n<=num_places; n++) {
		mask = (mask << 1) | 0x0001;
	}
	v = v & mask; // truncate v to specified number of places

	num_nibbles = num_places / 4;
	if ((num_places % 4) != 0) {
		++num_nibbles;
	}

	do {
		digit = ((v >> (num_nibbles-1) * 4)) & 0x0f;
		Serial.print(digit, HEX);
	} while (--num_nibbles);
}
