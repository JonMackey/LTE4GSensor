/*
*	DS18B20Multidrop.cpp, Copyright Jonathan Mackey 2021
*	Class to manage a multidrop group of 1-Wire DS18B20 thermometers.
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
#include "DS18B20Multidrop.h"
#include "Arduino.h"
#include "OneWire.h"
#include "StringUtils.h"

const char DS18B20Multidrop::kDegCelsiusStr[] PROGMEM = "°C";
const char DS18B20Multidrop::kDegFahrenheitStr[] PROGMEM = "°F";

/****************************** DS18B20Multidrop ******************************/
DS18B20Multidrop::DS18B20Multidrop(
	OneWire&	inOneWire,
	uint32_t	inUpdatePeriod,
	int16_t		inAlarmHigh,
	int16_t		inAlarmLow,
	EResolution	inResolution)
	: mOneWire(inOneWire), mResolution(inResolution), mCount(0), 
		mAlarmHigh(inAlarmHigh), mAlarmLow(inAlarmLow),
		mTemperatureChanged(false), mUpdatePeriod(inUpdatePeriod)
{
}

/*********************************** begin ************************************/
void DS18B20Multidrop::begin(void)
{
	/*
	*	Begin a targeted search for DS18B20 thermometers only.  Even if all of
	*	the 1-Wire devices are DS18B20, a targeted search is faster.
	*/
	mOneWire.target_search(0x28);
	uint8_t tIndex = 0;
	for(; tIndex < MAX_DS18B20_COUNT && mOneWire.search(mThermometer[tIndex].address);
			tIndex++)
	{
		if (OneWire::crc8(mThermometer[tIndex].address, 7) == mThermometer[tIndex].address[7])
		{
		#ifdef DUMP_TO_SERIAL
			Serial.print(F("ROM"));
			for (uint8_t k = 0; k < 8; k++)
			{
				Serial.print(' ');
				Serial.print(mThermometer[tIndex].address[k], HEX);
			}
			Serial.println();
		#endif
			continue;
		}
		break;	// Error
	}
	mCount = tIndex;
	
	if (mCount)
	{
		DSScratchPad	scratchPad;
		scratchPad.f.alarmHigh = mAlarmHigh/16;
		scratchPad.f.alarmLow = mAlarmLow/16;
		scratchPad.f.config = (mResolution << 5) + 0x1F;
		// Note that the 1 meter probes I purchased on AliExpress have turned out to
		// be clones, not original Dallas/Maxim chips.
		// See https://github.com/cpetrich/counterfeit_DS18B20
		// According to the notes, this is counterfeit family "C".
		// This family doesn't support setting the resolution.
		// ROM 28 FF 64 1D 96 48 C1 63
		// ROM 28 FF 64 1D 96 5C 95 0E
		// ROM 28 FF 64 1D 96 7A 9D A7
		//
		//
		// The 3 meter probes are probably clones also.  They support setting
		// the resolution.
		// ROM 28 E8 0A 69 2D 19 01 AC
		// ROM 28 A3 81 72 2D 19 01 36
		// ROM 28 1F 66 51 2D 19 01 3F

	#ifdef DUMP_TO_SERIAL
		Serial.print(F("In 0x"));
		Serial.print(scratchPad.data[2], HEX);
		Serial.print(' ');
		Serial.print(scratchPad.data[3], HEX);
		Serial.print(' ');
		Serial.println(scratchPad.data[4], HEX);
	#endif
		/*
		*	EEPROM values are copied to the scratch pad SRAM when power is
		*	applied.  The only reason you would rely on the EEPROM values being
		*	copied is if you wanted to set the alarm and config values once, or
		*	very infrequently.  The problem is the EEPROM values on newly added
		*	thermometers would have to be initialized somehow. Rather than deal
		*	with that this code just loads the SRAM values on startup
		*	overwriting whatever config values were loaded from EEPROM.
		*/
		mOneWire.reset();
		mOneWire.skip();	// Broadcast to all thermometers
		mOneWire.write(eWriteScratchPad);
		mOneWire.write_bytes(&scratchPad.data[2], 3);
		mOneWire.reset();
		
	#ifdef DUMP_TO_SERIAL
		scratchPad.f.alarmHigh = 9;
		scratchPad.f.alarmLow = 9;
		scratchPad.f.config = 9;
		ReadScratchPad(0, scratchPad);
		Serial.print(F("Out 0x"));
		Serial.print(scratchPad.f.alarmHigh, HEX);
		Serial.print(' ');
		Serial.print(scratchPad.f.alarmLow, HEX);
		Serial.print(' ');
		Serial.println(scratchPad.f.config, HEX);
	#endif
		/*
		*	initialize any garbage values in the thermometer array.
		*/
		for (uint8_t i = 0; i < mCount; i++)
		{
			mThermometer[i].temp = 0;
			mThermometer[i].changed = false;
			mThermometer[i].alarm = false;
		}
		mUpdatePeriod.Start();
		mDataIsValid = false;
	}
}

/*********************************** Update ***********************************/
/*
*	This performs a periodic update of the thermometer data stored in
*	mThermometer.  This is called on a regular basis when the MCU is not
*	not sleeping.  The MCU must be awake otherwise the timers used to determine
*	when a conversion has completed and when a new conversion/update should take
*	place would be inaccurate.
*/
bool DS18B20Multidrop::Update(
	bool	inResetAlarms)
{
	bool dataUpdated = DataUpdated(inResetAlarms);
	/*
	*	The mConversionPeriod must be less than mUpdatePeriod otherwise the
	*	data update would never finish.
	*/
	if (mUpdatePeriod.Passed())
	{
		mUpdatePeriod.Start();
		BeginDataUpdate();
	}
	return(dataUpdated);
}

/****************************** BeginDataUpdate *******************************/
/*
*	This starts a read conversion of all thermometers on the OneWire bus.
*	Update() or DataUpdated() should be called till the data has been updated.
*	The MCU must remain awake until the data update completes.
*/
bool DS18B20Multidrop::BeginDataUpdate(void)
{
	uint8_t	success = mOneWire.reset();
	if (success)
	{
		mOneWire.skip();	// Broadcast to all thermometers
		mOneWire.write(eConvertTemperature);
		success = mOneWire.reset();
		mConversionPeriod.Set((1<<mResolution) * 94);// Roughly 94, 188, 376, 752 ms
		mConversionPeriod.Start();
	}
	return(success);
}

/******************************** DataUpdated *********************************/
/*
*	This is repeatedly called after a read/conversion has started via a call to
*	BeginDataUpdate().  BeginDataUpdate() is called by Update as well as
*	directly by the host when periodically waking the MCU from sleep.
*
*	Returns true if the thermometer data has been updated.  This doesn't mean
*	the data has changed in any way, just that the converted data has been read.
*
*	For periodic updates when the MCU is waking from sleep, true
*	would signal it's time to go back to sleep.  For updates when the MCU is
*	fully awake, true would cause
*
*	inResetAlarms true means reset the alarm state of each individual
*	thermometer and the overall alarm state before checking for alarms.
*/
bool DS18B20Multidrop::DataUpdated(
	bool	inResetAlarms)
{
	bool dataUpdated = mConversionPeriod.Passed();
	if (dataUpdated)
	{
		mDataIsValid = true;
		if (inResetAlarms)
		{
			ResetAlarm();
		}

		mConversionPeriod.Set(0);
		DSScratchPad	scratchPad;
	
		for (uint8_t i = 0; i < mCount; i++)
		{
			if (ReadScratchPad(i, scratchPad))
			{
				if (scratchPad.f.temp != mThermometer[i].temp)
				{
					mTemperatureChanged = true;
					mThermometer[i].changed = true;
					mThermometer[i].temp = scratchPad.f.temp;

					// This could be done with an alarm search command, but since
					// we're getting the temperture anyway, it's faster to check it
					// here.
					if (!mThermometer[i].alarm &&
						(scratchPad.f.temp >= mAlarmHigh ||
							scratchPad.f.temp <= mAlarmLow))
					{
						mAlarm = true;
						mThermometer[i].alarm = true;
					}
				}
			}
		}
	}
	return(dataUpdated);
}

/************************** ResetTemperatureChanged ***************************/
void DS18B20Multidrop::ResetTemperatureChanged(void)
{
	mTemperatureChanged = false;
	for (uint8_t i = 0; i < mCount; i++)
	{
		mThermometer[i].changed = false;
	}
}

/********************************* ResetAlarm *********************************/
/*
*	The alarm will immediately retrigger if the alarm condition hasn't changed.
*/
void DS18B20Multidrop::ResetAlarm(void)
{
	mAlarm = false;
	for (uint8_t i = 0; i < mCount; i++)
	{
		mThermometer[i].alarm = false;
	}
}

/********************************* NewAlarms **********************************/
/*
*	Returns the mask of current alarms minus inIgnoreMask (pass 0 to get all.)
*	Each set bit of the returned mask represents a thermometer with its
*	alarm flag set.
*	bit 0 = thermometer index 0, bit 1 = index 1, etc..
*/
uint8_t DS18B20Multidrop::NewAlarms(
	uint8_t	inIgnoreMask)
{
	uint8_t	newAlarms = 0;
	if (mAlarm)
	{
		uint8_t	numThermometers = mCount;
		for (uint8_t i = 0; i < numThermometers; i++)
		{
			if (mThermometer[i].alarm &&
				(inIgnoreMask & _BV(i)) == 0)
			{
				newAlarms |= _BV(i);
			}
		}
	}
	return(newAlarms);
}

/******************************* ReadScratchPad *******************************/
bool DS18B20Multidrop::ReadScratchPad(
	uint8_t			inIndex,
	DSScratchPad&	outScratchPad)
{
	uint8_t	success = mOneWire.reset();
	if (success)
	{
		mOneWire.select(mThermometer[inIndex].address);
		mOneWire.write(eReadScratchPad);
		mOneWire.read_bytes(outScratchPad.data, 9);
		success = mOneWire.reset();
	}
	return(success);
}

/******************************* WriteScratchPad *******************************/
bool DS18B20Multidrop::WriteScratchPad(
	uint8_t			inIndex,
	DSScratchPad&	inScratchPad,
	bool			inSaveToEEPROM)
{
	uint8_t	success = mOneWire.reset();
	if (success)
	{
		mOneWire.select(mThermometer[inIndex].address);
		mOneWire.write(eWriteScratchPad);
		mOneWire.write_bytes(&inScratchPad.data[2], 3);
		success = mOneWire.reset();
		if (success && inSaveToEEPROM)
		{
			mOneWire.select(mThermometer[inIndex].address);
			mOneWire.write(eCopyScratchPad);
			// Per doc, don't reset till the EEPROM write completes (Max 10ms)
			delay(10);
			mOneWire.reset();
		}
	}
	return(success);
}

/******************************* CreateTempStr ********************************/
uint8_t DS18B20Multidrop::CreateTempStr(
	uint8_t	inIndex,
	bool	inCelsius,
	bool	inAppendUnitSuffix,
	bool	inUse7Bit,
	char*	outTempStr)
{
	uint8_t	charsBeforeDec = CreateTempStr(mThermometer[inIndex].temp, inCelsius, inAppendUnitSuffix, inUse7Bit, outTempStr);
	return(charsBeforeDec);
}

/******************************* CreateTempStr ********************************/
uint8_t DS18B20Multidrop::CreateTempStr(
	int16_t	inTemperatureC,
	bool	inCelsius,
	bool	inAppendUnitSuffix,
	bool	inUse7Bit,
	char*	outTempStr)
{
	uint8_t	charsBeforeDec = StringUtils::Fixed16ToDec10Str(inCelsius ?
							inTemperatureC : CToF(inTemperatureC), outTempStr);
	if (inAppendUnitSuffix)
	{
		outTempStr += (charsBeforeDec+2);
		if (inUse7Bit)
		{
			outTempStr[0] = inCelsius ? 'C':'F';
			outTempStr[1] = 0;
		} else
		{
			// Both strings are 3 bytes UTF8: C2 B0 43 or C2 B0 47
			strcpy_P(outTempStr, inCelsius ? kDegCelsiusStr : kDegFahrenheitStr);
		}
	}
	return(charsBeforeDec);
}
