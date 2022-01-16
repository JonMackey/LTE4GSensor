/*
*	LTESensorConfig.h, Copyright Jonathan Mackey 2020
*
*	GNU license:
*	This program is free software: you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*	Please maintain this license information along with authorship and copyright
*	notices in any redistribution of this code.
*
*/
#ifndef LTESensorConfig_h
#define LTESensorConfig_h

#include <inttypes.h>

#define LTE_SENSOR_VER	11	// v1.1 (board version)

#define DEBOUNCE_DELAY		20	 // ms	Down time required for a button press.
#define DEEP_SLEEP_DELAY	2000 // ms	Down time required for a power button press.

//#define SUPPORT_PERIODIC_SLEEP	1
#ifdef SUPPORT_PERIODIC_SLEEP
#define CHECK_TEMP_TICKS	60*1000		// 1 minute in ms 
#define CHECK_FOR_SMS_TICKS	5*60*1000	// 5 minutes in ms
#endif
// Anything higher than 19200 on a 8MHz mcu will have problems when 2 UARTs are
// operating at the same time.
#define BAUD_RATE	19200
#define DISPLAY_ROTATION	1	// Rotate display and buttons:
								// 0 = 0, 1 = 90, 2 = 180, 3 = 270

namespace Config
{
	const int8_t	kOneWirePin			= 0;	// PB0
	const int8_t	kBacklightPin		= 1;	// PB1
	const uint8_t	kDCPin				= 2;	// PB2
	const int8_t	kCSPin				= 3;	// PB3	(Display select)
	const int8_t	kResetPin			= 4;	// PB4
	const uint8_t	kMOSI				= 5;	// PB5
	const uint8_t	kMISO				= 6;	// PB6
	const uint8_t	kSCK				= 7;	// PB7
	
	const uint8_t	kRxPin				= 8;	// PD0
	const uint8_t	kTxPin				= 9;	// PD1
	const uint8_t	kSIMRxPin			= 10;	// PD2
	const uint8_t	kSIMTxPin			= 11;	// PD3
	const uint8_t	kSIMRItPin			= 12;	// PD4	PCINT28
	const uint8_t	kSIMDTRPin			= 13;	// PD5	PCINT29
	const uint8_t	kUpBtnPin			= 14;	// PD6	PCINT30
	const uint8_t	kLeftBtnPin			= 15;	// PD7	PCINT31

	const uint8_t	kSCL				= 16;	// PC0
	const uint8_t	kSDA				= 17;	// PC1
	const uint8_t	kEnterBtnPin		= 18;	// PC2	PCINT18
	const uint8_t	kSIMPowerKeyPin		= 19;	// PC3	PCINT19
	const uint8_t	kSIMResetPin		= 20;	// PC4	PCINT20
	const int8_t	kPowerBtnPin		= 21;	// PC5	PCINT21
	const uint8_t	kTOSC1Pin			= 22;	// PC6
	const uint8_t	kTOSC2Pin			= 23;	// PC7
	
	const uint8_t	kRightBtnPin		= 24;	// PA0	PCINT0
	const uint8_t	kDownBtnPin			= 25;	// PA1	PCINT1
	const int8_t	kUnusedPinA2		= 26;	// PA2
	const int8_t	kUnusedPinA3		= 27;	// PA3
	const int8_t	kUnusedPinA4		= 28;	// PA4
	const int8_t	kUnusedPinA5		= 29;	// PA5
	const int8_t	kUnusedPinA6		= 30;	// PA6
	const int8_t	kUnusedPinA7		= 31;	// PA7

#if DISPLAY_ROTATION == 0
	const uint8_t	kRightBtn			= _BV(PIND7);
	const uint8_t	kDownBtn			= _BV(PIND6);
	const uint8_t	kUpBtn				= _BV(PINA1);
	const uint8_t	kLeftBtn			= _BV(PINA0);
#elif DISPLAY_ROTATION == 1
	const uint8_t	kRightBtn			= _BV(PIND6);
	const uint8_t	kDownBtn			= _BV(PINA0);
	const uint8_t	kUpBtn				= _BV(PIND7);
	const uint8_t	kLeftBtn			= _BV(PINA1);
#elif DISPLAY_ROTATION == 2
	const uint8_t	kRightBtn			= _BV(PINA0);
	const uint8_t	kDownBtn			= _BV(PINA1);
	const uint8_t	kUpBtn				= _BV(PIND6);
	const uint8_t	kLeftBtn			= _BV(PIND7);
#endif
	const uint8_t	kEnterBtn			= _BV(PINC2);
	const uint8_t	kPowerBtn			= _BV(PINC5);
	const uint8_t	kPINABtnMask		= (_BV(PINA0) | _BV(PINA1));
	const uint8_t	kPINCBtnMask		= (_BV(PINC2) | _BV(PINC5));
	const uint8_t	kPINDBtnMask		= (_BV(PIND6) | _BV(PIND7));

	/*
	*	EEPROM usage, 2K bytes
	*	Uninitialized values are FF
	*
	*	[0]		uint8_t		flags, bit 0 is for 24 hour clock format on all boards.
	*						0 = 24 hour, 1 = 12 hour (default)
	*						bit 1 is enable sleep, 1 = enable (default)
	*						bit 2 is temperature unit.  0 = Celsius, 1 = Fahrenheit (default)
	*						bit 3 is alarm off.  0 = on, 1 = off (default)
	*	[1 to 3] unused/available
	*	[4]		uint16_t	4 digit PIN
	*	[6]		TPAddress	Alarm Target Address (16 bytes max)
	*	[22 to 29] unused/available
	*	[30]	int16_t		Alarm High (C)
	*	[32]	int16_t		Alarm Low (C)
	*/
	const uint16_t	kFlagsAddr	= 0;
	const uint8_t	k12HourClockBit		= 0;
	const uint8_t	kEnableSleepBit		= 1;
	const uint8_t	kTempUnitBit		= 2;
	const uint8_t	kAlarmIsOffBit		= 3;	
	
	const uint16_t	kPINAddr			= 4;
	const uint16_t	kTargetAddr			= 6;
	const uint16_t	kAlarmHighAddr		= 30;
	const uint16_t	kAlarmLowAddr		= 32;

	const uint8_t	kTextInset			= 3; // Makes room for drawing the selection frame
	const uint8_t	kTextVOffset		= 6; // Makes room for drawing the selection frame
	// To make room for the selection frame the actual font height in the font
	// file is reduced.  The actual height is kFontHeight.
	const uint8_t	kFontHeight			= 43;
	const uint8_t	kDisplayWidth		= 240;
}

#endif // LTESensorConfig_h

