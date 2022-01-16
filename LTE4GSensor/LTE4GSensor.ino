/*
*	LTESensor.ino, Copyright Jonathan Mackey 2022
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
*/
#include <SPI.h>
#include <Wire.h>

#include "LTESensorConfig.h"
#include "TFT_ST7789.h"
#include "LTESensor.h"
#include "ATmega644RTC.h"
#include "OneWire.h"
#include "DS18B20Multidrop.h"


TFT_ST7789	display(Config::kDCPin, Config::kResetPin,
						Config::kCSPin, Config::kBacklightPin, 240, 240);
LTESensor	lteSensor;
// Define "xFont" to satisfy the auto-generated code with the font files
// This implementation uses 'lteSensor' as a subclass of xFont
#define xFont lteSensor
#include "MyriadPro-Regular_36_1b.h"
#include "MyriadPro-Regular_18.h"

OneWire  oneWire(Config::kOneWirePin);
DS18B20Multidrop	thermometers(oneWire, 10000, 27*16);	// 10000 = update every 10 seconds.  27 = alarm high 27C

/*********************************** setup ************************************/
void setup(void)
{
	ATmega644RTC::RTCInit();
	Serial.begin(BAUD_RATE);
	Serial.print(F("Starting...\n"));

	Wire.begin();
	SPI.begin();

	// Pull-up all unused pins
	pinMode(Config::kUnusedPinA2, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA3, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA4, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA5, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA6, INPUT_PULLUP);
	pinMode(Config::kUnusedPinA7, INPUT_PULLUP);

	pinMode(Config::kSIMDTRPin, OUTPUT);
	digitalWrite(Config::kSIMDTRPin, LOW);
	pinMode(Config::kSIMRItPin, INPUT);	
	

	UnixTime::ResetSleepTime();
	display.begin(DISPLAY_ROTATION); // Init TFT
	display.Fill();

	thermometers.begin();
	lteSensor.begin(&thermometers, &display, &MyriadPro_Regular_36_1b::font,
												&MyriadPro_Regular_18::font);
}

/************************************ loop ************************************/
void loop(void)
{
	lteSensor.GiveTime();	
}
