/* ALPHA 003 */
/*********************************************************************
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/

/*********************************************************************
Keeping my clock in sync with a battery backup.
rob@openpart.co.uk 14/01/2016

Hacked up version of the original available via:
https://www.dropbox.com/s/qbo8fqkjkvm6wu2/OpenSourceNixieTubeShieldFirmware.ino

Modified for DS3231 (Precision Real Time Clock Module / Memory Module /
battery backup). It may also work with a DS1307.
Tested on an Uno let me know if it works for you.
Connect VCC to 5V and GND to GND.
Connect SDA (or data) and SCL (or clock). On a Pro-Mini, Arduino Uno or
compatible boards, these pins are A4 - data and A5 - clock. If you’re
using an Arduino Mega the pins are D20 - data and D21 - clock.

Jo Havik ported the original code to Mega and Leonardo.
I'm not sure if it will work with the Leonardo???
*********************************************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include "Wire.h"
#define ACP_DELAY 25
#define DS3231_I2C_ADDRESS 0x68

// Convert normal decimal numbers to binary coded decimal
byte decToBcd(byte val)
{
  return( (val/10*16) + (val%10) );
}
// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val)
{
  return( (val/16*10) + (val%16) );
}

/* global variables */
volatile unsigned char INDEX = 1;   /* 1 to 6 */
volatile unsigned char HOUR = 12;   /* 1 to 12 */ /* 01 to 12 actually! */
volatile unsigned char MINUTE = 00;  /* 0 to 59 */
volatile unsigned char SECOND = 00;  /* 0 to 59 */

volatile boolean TOP_OF_THE_MINUTE = false;

/* timer1 used for timekeeping */
ISR(TIMER1_COMPA_vect)
{
	/* increment and check seconds */
	if (++SECOND >= 60)
	{
		SECOND = 0;
		TOP_OF_THE_MINUTE = true;

		/* increment and check minutes */
		if (++MINUTE >= 60)
		{
			MINUTE = 0;

			/* increment and check hours */
			if (++HOUR >= 13)
			{
				HOUR = 1;
			}
		}
	}
}

/* Nixie Tube multiplexing. Uses timer3 on Leonardo, timer2 on everything else */
#if defined(__AVR_ATmega32U4__)
	ISR(TIMER3_COMPA_vect)
#else
	ISR(TIMER2_COMPA_vect)
#endif
{
	blankCathodes();
	blankAnodes();
	setFirstLed((HOUR / 10) || TOP_OF_THE_MINUTE); /* turn first led on if HOUR = 10, 11, or 12 or top of minute */
   
	switch(INDEX++)
	{
		/* HOUR tens place */
		case 1:
			/* set cathode */
			setCathodes((HOUR / 10)); 

			/* only turn anode on if one */
			if (HOUR / 10)
			{
				setAnodes(0x04);
			}
		break;

		/* HOUR ones place */
		case 2:
			/* set cathode */
			setCathodes((HOUR % 10)); 
			
			/* turn on anode */
			setAnodes(0x08);
		break;

		/* MINUTE tens place */
		case 3:
			/* set cathode */
			setCathodes((MINUTE / 10)); 
			
			/* turn on anode */
			setAnodes(0x10);
		break;

		/* MINUTE ones place */
		case 4:  
			/* set cathode */
			setCathodes((MINUTE % 10));
			
			/* turn on anode */
			setAnodes(0x20); 
		break;

		/* SECOND tens place */
		case 5:
			/* set cathode */
			setCathodes((SECOND / 10)); 
			
			/* turn on anode */
			setAnodes(0x40); 
		break;

		/* SECOND ones place */
		case 6:
			/* set cathode */
			setCathodes((SECOND % 10)); 
			
			/* turn on anode */
			setAnodes(0x80); 
			
			/* reset index */
			INDEX = 1;
		break;
	}    
}

void setup()
{
	/* configure pins */
	pinMode(2, OUTPUT);
	pinMode(3, OUTPUT);
	pinMode(4, OUTPUT);
	pinMode(5, OUTPUT);
	pinMode(6, OUTPUT);
	pinMode(7, OUTPUT);
	pinMode(8, OUTPUT);
	pinMode(9, OUTPUT);
	pinMode(10, OUTPUT);
	pinMode(11, OUTPUT);
	pinMode(12, OUTPUT);


	/* disable global interrupts */
	cli();

	/* timer1 1Hz interrupt for timekeeping function*/
	TCCR1A = TCCR1B = TCNT1 = 0;
	OCR1A = 0x3D08;
	TCCR1B |= (1 << WGM12) | (1 << CS12) | (1 << CS10);
	TIMSK1 |= (1 << OCIE1A);

	/* 1kHz interrupt for multiplexing function. Use timer3 for Leonardo, timer2 for others */
	#if defined(__AVR_ATmega32U4__)       
		TCCR3A = TCCR3B = TCNT3 = 0;
		OCR3A = 0xF;
		TCCR3B |= (1 << WGM32 ) | (1 << CS32) | (1<<CS30);
		TIMSK3 |= (1 << OCIE3A);
	#else
		TCCR2A = TCCR2B = TCNT2 = 0;
		OCR2A = 0xF;
		TCCR2B |= (1 << WGM22 ) | (1 << CS22) | (1<<CS20);
		TIMSK2 |= (1 << OCIE2A);
	#endif
	
	/* enable global interrupts */
	sei();

  
  Wire.begin();
  // set the time here
  // DS3231 seconds, minutes, hours, day, date, month, year
  // setDS3231time(00,00,21,5,14,01,16);

	#ifdef DEBUG
		Serial.begin(115200);
		Serial.println("Open Source Nixie Tube Shield v10");
		Serial.println("     Switchmode Design, Inc      ");
	#endif

}

void loop()
{
	/* cycle digits at top of minute */
	if (TOP_OF_THE_MINUTE)
	{
		anti_cathode_poisoning();
		TOP_OF_THE_MINUTE = false;
	}
  displayTime(); // display the real-time clock data on the Serial Monitor,
}

void anti_cathode_poisoning()
{
	/* save current time */
	unsigned char hour = HOUR;
	unsigned char minute = MINUTE;
	unsigned char second = SECOND;

	/* cycle the digits */
	HOUR = MINUTE = SECOND = 11;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 22;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 77;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 88;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 00;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 66;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 33;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 99;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 44;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 55;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 44;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 99;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 33;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 66;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 00;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 88;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 77;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 22;
	delay(ACP_DELAY);
	HOUR = MINUTE = SECOND = 11;
	delay(ACP_DELAY);

	/* restore current time */
	HOUR = hour;
	MINUTE = minute;
	SECOND = second;
}


/* Utility functions for handling pin differences on Uno, Leonardo and Mega */
/* These functions are inlined when compiled */

/* Pin mappings for Mega: http://arduino.cc/en/Hacking/PinMapping2560 */
/* Pin mappings for Leonardo: http://arduino.cc/en/Hacking/PinMapping32u4 */

/* Sets all anodes (pin 2-7) low */
inline void blankAnodes()
{
	#if defined(__AVR_ATmega32U4__)
		PORTD &= 0b01101100;   // pin 2,3,4,6 off
		PORTC &= 0b10111111;   // pin 5 off
		PORTE &= 0b10111111;   // port 7 off
	#endif
	#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		PORTE &= 0b11000111;  // pin 2,3,5 off
		PORTG &= 0b11011111;  // pin 4 off
		PORTH &= 0b11100111;  // pin 6,7 off
	#endif
	#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
		PORTD &= 0b00000011;           // pin 2-7 off
	#endif
}

/* Sets selected anode pins high */
inline void setAnodes(byte bits)
{
	#if defined(__AVR_ATmega32U4__)
		PORTD |= ((bits >> 1) & 0b00000010) | ((bits >> 3) & 0b00000001) | (bits & 0b00010000) | ((bits << 1) & 0b10000000);   //  (pin 2) | (pin 3) | (pin4) | (pin6)
		PORTC |= ((bits << 1 ) & 0b01000000); // pin 5 
		PORTE |= ((bits >> 1 ) & 0b01000000); // pin7
	#endif
	#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)   
		PORTE |= ((bits << 2) & 0b00110000) | ((bits >> 2) & 0b00001000); // (pin 2,3) | (pin 5)
		PORTG |= ((bits << 1) & 0b00100000);   // pin 4
		PORTH |= ((bits >> 3) & 0b00011000);   // pin 6,7   
	#endif
	#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
		PORTD |= (bits & 0b11111100);      // pin 2 - 7
	#endif
}

/* Sets all cathodes (pin 8-11) low */
inline void blankCathodes()
{
	#if defined(__AVR_ATmega32U4__)
		PORTB &= 0b00001111;   // pin 8,9,10,11 off
	#endif
	#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		PORTH &= 0b10011111; // pin 8,9 off
		PORTB &= 0b11001111; // pin 10,11 off
	#endif
	#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
		PORTB &= 0b11110000;        // pin 8-11 off 
	#endif  
}

/* Sets selected cathode pins high */
inline void setCathodes(byte bits)
{
	#if defined(__AVR_ATmega32U4__)
		PORTB |= (( bits << 4) & 0b11110000); // pin 8,9,10,11
	#endif
	#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		PORTH |= ((bits << 5 ) & 0b01100000); // pin 8,9 
		PORTB |= ((bits << 2 ) & 0b00110000); // pin 10,11
	#endif
	#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
		PORTB |= bits & 0b00001111;  // pin 8 - 11
	#endif
}

/* Sets led under first hour digit high or low */
inline void setFirstLed(bool state)
{
	#if defined(__AVR_ATmega32U4__)
		PORTD |= ( state << 6); // pin 12
	#endif
	#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
		PORTB |= ( state << 6); // pin 12
	#endif
	#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
		PORTB |= ( state << 4); // pin 12
	#endif
}

/* store the datetime */
void setDS3231time(byte DSsecond, byte DSminute, byte DShour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
{
  // sets time and date data to DS3231
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(DSsecond)); // set seconds
  Wire.write(decToBcd(DSminute)); // set minutes
  Wire.write(decToBcd(DShour)); // set hours
  Wire.write(decToBcd(dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(month)); // set month
  Wire.write(decToBcd(year)); // set year (0 to 99)
  Wire.endTransmission();
}
/* read the datetime */
void readDS3231time(byte *DSsecond, byte *DSminute, byte *DShour, byte *dayOfWeek, byte *dayOfMonth, byte *month, byte *year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();
  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  *DSsecond = bcdToDec(Wire.read() & 0x7f);
  *DSminute = bcdToDec(Wire.read());
  *DShour = bcdToDec(Wire.read() & 0x3f);
  *dayOfWeek = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month = bcdToDec(Wire.read());
  *year = bcdToDec(Wire.read());
}

/* display DS time on serial - mostly for debug */
void displayTime()
{
  byte DSsecond, DSminute, DShour, dayOfWeek, dayOfMonth, month, year;
  // retrieve data from DS3231
  // readDS3231time(&DSsecond, &DSminute, &DShour, &dayOfWeek, &dayOfMonth, &month, &year);

  #ifdef DEBUG
  // send it to the serial monitor
  Serial.print(DShour, DEC);
  // convert the byte variable to a decimal number when displayed
  Serial.print(":");
  if (minute<10)
  {
    Serial.print("0");
  }
  Serial.print(DSminute, DEC);
  Serial.print(":");
  if (second<10)
  {
    Serial.print("0");
  }
  Serial.print(DSsecond, DEC);
  Serial.print(" ");
  Serial.print(dayOfMonth, DEC);
  Serial.print("/");
  Serial.print(month, DEC);
  Serial.print("/");
  Serial.print(year, DEC);
  Serial.print(" Day of week: ");
  switch(dayOfWeek){
  case 1:
    Serial.println("Sunday");
    break;
  case 2:
    Serial.println("Monday");
    break;
  case 3:
    Serial.println("Tuesday");
    break;
  case 4:
    Serial.println("Wednesday");
    break;
  case 5:
    Serial.println("Thursday");
    break;
  case 6:
    Serial.println("Friday");
    break;
  case 7:
    Serial.println("Saturday");
    break;
  }
  #endif
}
