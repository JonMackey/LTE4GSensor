/*
*	DS18B20Multidrop.h, Copyright Jonathan Mackey 2021
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
#ifndef DS18B20Multidrop_h
#define DS18B20Multidrop_h

#include <inttypes.h>
#include "MSPeriod.h"

class OneWire;

//#define DUMP_TO_SERIAL 1

#define MAX_DS18B20_COUNT	4	// (if more than 8 then update NewAlarms())

typedef uint8_t OneWireDevAddr[8];
typedef uint8_t DS18B20ScratchPad[9];

union DSScratchPad
{
	struct
	{
		int16_t	temp;	// Fixed-point with 1/16 scaling factor (as per doc)
		int8_t	alarmHigh;
		int8_t	alarmLow;
		uint8_t	config;
		uint8_t	reserved[3];
		uint8_t	crc;
	} f;
	uint8_t	data[9];
	
	
};

struct SDS18B20
{
	uint8_t	address[8];
	int16_t	temp;
	bool	changed;
	bool	alarm;
};

class DS18B20Multidrop
{
public:
	enum EResolution
	{
		e9BitResolution,	// 0.5 C, 93.75 ms
		e10BitResolution,	// 0.25 C, 187.5 ms
		e11BitResolution,	// 0.125 C, 375 ms
		e12BitResolution	// 0.0625 C, 750 ms
	};
	/*
	*	The alarm high/low values are fixed-point with a 1/16 scale. (low 4 bits
	*	used for fraction.)
	*/
							DS18B20Multidrop(
								OneWire&				inOneWire,
								uint32_t				inUpdatePeriod,
								int16_t					inAlarmHigh = 38*16,// 38C, ~100F
								int16_t					inAlarmLow = 0,		// 0C, 32F
								EResolution				inResolution = e9BitResolution);
	/*
	*	Should be called on startup and whenever a thermometer is added or
	*	removed from the 1-Wire bus.
	*/
	void					begin(void);
	
	bool					Update(
								bool					inResetAlarms);
	bool					DataUpdated(
								bool					inResetAlarms);
	bool					BeginDataUpdate(void);
	
							// Returns true if any of the temperatures have changed.
	bool					TemperatureChanged(void) const
								{return(mTemperatureChanged);}
							// Returns true if the thermometer at inIndex has changed.
	bool					TemperatureChanged(
								uint8_t					inIndex) const
								{return(mThermometer[inIndex].changed);}
	void					ResetTemperatureChanged(void);
	bool					DataIsValid(void) const
								{return(mDataIsValid);}
	bool					Alarm(void) const
								{return(mAlarm);}
							// Returns true if the alarm at inIndex is active.
	bool					Alarm(
								uint8_t					inIndex) const
								{return(mThermometer[inIndex].alarm);}
	uint8_t					NewAlarms(
								uint8_t					inIgnoreMask);
	void					ResetAlarm(void);
							// Initiate readings of all thermometers (async)
	inline bool				ConversionDone(void)
								{return(mConversionPeriod.Passed());}
	inline const SDS18B20*	GetThermometers(void) const
								{return(mThermometer);}
	inline const uint8_t	GetCount(void) const
								{return(mCount);}
	void					SetUpdatePeriod(
								uint32_t				inPeriod)
								{mUpdatePeriod.Set(inPeriod);
								mUpdatePeriod.Start();}
	/*
	*	SetAlarmHighLow is only an accessor.  If you need to set the value on
	*	the thermometers then you should call begin() after calling this routine.
	*	Note that this class doesn't use the high low value stored on the
	*	thermometer itself but rather uses the high low members of this class.
	*	Therefore there shouldn't be any reason to update the high low values
	*	on the thermometers unless this class is modified or subclassed to
	*	support individual termometer alarms.  (See DS18B20Multidrop::Update())
	*/							
	void					SetAlarmHigh(
								int16_t					inAlarmHigh)
							{
								mAlarmHigh = inAlarmHigh;
							}
	void					SetAlarmLow(
								int16_t					inAlarmLow)
							{
								mAlarmLow = inAlarmLow;
							}
	int16_t					GetAlarmHigh(void) const
								{return(mAlarmHigh);}
	int16_t					GetAlarmLow(void) const
								{return(mAlarmLow);}
								
	uint8_t					CreateTempStr(
								uint8_t					inIndex,
								bool					inCelsius,
								bool					inAppendUnitSuffix,
								bool					inUse7Bit,
								char*					outTempStr);
	static uint8_t			CreateTempStr(
								int16_t					inTemperatureC,
								bool					inCelsius,
								bool					inAppendUnitSuffix,
								bool					inUse7Bit,
								char*					outTempStr);
	// Fixed-point conversion to F uses unscaled multipiers and divisors so the
	// result is the same 1/16 scale, then 32 is added scaled to 1/16.
	inline static int16_t	CToF(
								int16_t					inTempC)
								{return(((inTempC * 9) / 5) + (32*16));}
	inline static int16_t	FToC(
								int16_t					inTempF)
								{return(((inTempF - (32*16)) * 5) / 9);}
								
	static const char kDegCelsiusStr[];
	static const char kDegFahrenheitStr[];
protected:
	OneWire&	mOneWire;
	MSPeriod	mConversionPeriod;
	MSPeriod	mUpdatePeriod;
	SDS18B20	mThermometer[MAX_DS18B20_COUNT];
	int16_t		mAlarmHigh;
	int16_t		mAlarmLow;
	bool		mAlarm;
	bool		mTemperatureChanged;
	bool		mDataIsValid;
	uint8_t		mCount;
	uint8_t		mResolution;

	enum ECommandSet
	{
		eConvertTemperature	= 0x44,	// Initiate a temperature conversion
		eReadScratchPad		= 0xBE,	// Read entire scratch pad (9 bytes)
		eWriteScratchPad	= 0x4E,	// Write 3 bytes to scratch pad from offset 2.
		eCopyScratchPad		= 0x48,	// Save scratch pad to EEPROM
		eRecallEEPROM		= 0xB8,	// Read alarm trigger values
		eReadPowerSupply	= 0xB4	// Read whether parasitic
	};
	
	bool					ReadScratchPad(
								uint8_t					inIndex,
								DSScratchPad&			outScratchPad);
	bool					WriteScratchPad(
								uint8_t					inIndex,
								DSScratchPad&			inScratchPad,
								bool					inSaveToEEPROM);
};
#endif // DS18B20Multidrop_h
