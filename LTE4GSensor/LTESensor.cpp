/*
*	LTESensor.cpp, Copyright Jonathan Mackey 2021
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
#include <Arduino.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <EEPROM.h>

#include "LTESensor.h"
#include "DS18B20Multidrop.h"
#include "SIM7000.h"
#include "SerialUtils.h"
#include "ATmega644RTC.h"

bool LTESensor::sButtonPressed;

/*
Alarm
	Alarm: On/Off
	#: 123 456 7890	-> Set via SMS
	H:100°F L:100°F	-> Set via SMS
	Send Test SMS
	
Display
	Alarm Settings
	PIN: xxxx
	Temp Unit: °F/°C
	Clock: 12H/24H
	Update Sensors
	//Sleep: Enabled/Disabled
	
*/
const char kAlarmStr[] PROGMEM = "Alarm: ";	// Enabled/Disabled
const char kClockStr[] PROGMEM = "Clock: ";	// 12H/24H
const char k12HStr[] PROGMEM = "12H";
const char k24HStr[] PROGMEM = "24H";
const char kTempUnitStr[] PROGMEM = "Temp Unit: ";	// °F/°C
const char kNoMessageStr[] PROGMEM = " ";
const char kErrorNumStr[] PROGMEM = "Error: ";
const char kPINStr[] PROGMEM = "PIN: ";
// SMS Commands
const char kSetupStr[] PROGMEM = "setup";
const char kOnStr[] PROGMEM = "on";
const char kOffStr[] PROGMEM = "off";
const char kQueryStr[] PROGMEM = "?";

const char* const kSMSCommands[] PROGMEM =
{
	kSetupStr,
	kOnStr,
	kOffStr,
	kQueryStr
};
enum ESMSCommand
{
	eCommandNotFound,
	eSetupCmd,
	eOnCmd,
	eOffCmd,
	eQueryCmd
};

const char kOKStr[] PROGMEM = "OK";
const char kSetViaSMSWarningStr[] PROGMEM = "Set via SMS";
const char kTestSMSSentStr[] PROGMEM = "Test SMS Sent";
const char kCantSendSMSStr[] PROGMEM = "Can't Send SMS";
const char kNoSignalStr[] PROGMEM = "No Signal";
const char kBusyStr[] PROGMEM = "Busy";
const char kNoSIMCardStr[] PROGMEM = "No SIM Card";

struct SStringDesc
{
	const char*	descStr;
	uint16_t	color;
};

const SStringDesc kTextDesc[] PROGMEM =
{
	{kNoMessageStr, XFont::eWhite},
	{kOKStr, XFont::eWhite},
	{kSetViaSMSWarningStr, XFont::eYellow},
	{kTestSMSSentStr, XFont::eGreen},
	{kCantSendSMSStr, XFont::eYellow},
	{kNoSignalStr, XFont::eRed},
	{kNoSIMCardStr, XFont::eRed},
	{kBusyStr, XFont::eYellow},
	{kErrorNumStr, XFont::eWhite}
};

/********************************* LTESensor **********************************/
LTESensor::LTESensor(void)
: SIM7000(Serial1, Config::kSIMRxPin, Config::kSIMTxPin,
					Config::kSIMPowerKeyPin, Config::kSIMResetPin),
	mDebouncePeriod(DEBOUNCE_DELAY), mSleepEnabled(true), mSMSReply(eNoReply)
{
}

/*********************************** begin ************************************/
void LTESensor::begin(
	DS18B20Multidrop*	inThermometers,
	DisplayController*	inDisplay,
	Font*				inNormalFont,
	Font*				inSmallFont)
{
	SIM7000::SetPassthrough(&Serial);
	SIM7000::begin();
	
	mThermometers = inThermometers;
	
	pinMode(Config::kPowerBtnPin, INPUT_PULLUP);
	pinMode(Config::kUpBtnPin, INPUT_PULLUP);
	pinMode(Config::kLeftBtnPin, INPUT_PULLUP);
	pinMode(Config::kEnterBtnPin, INPUT_PULLUP);
	pinMode(Config::kRightBtnPin, INPUT_PULLUP);
	pinMode(Config::kDownBtnPin, INPUT_PULLUP);

	cli();						// Disable interrupts
	ADCSRA &= ~_BV(ADEN);		// Turn off ADC to save power.
	PRR0 |= _BV(PRADC);
	
	/*
	*	Other power saving changes verified:
	*	- On-chip Debug System is disabled - OCDEN and JTAGEN H fuses
	*	- Watchdog timer always on is disabled - WDTON of H fuse
	*/
	
	/*
	*	To wake from sleep and to respond to button presses, setup pin change
	*	interrupts for the button pins. All of the pins aren't on the same port.
	*	PA0 & PA1 are on PCIE0, PC2 & PC5 are on PCIE2, and PD6 & PD7 are on PCIE3.
	*/
	PCMSK0 = _BV(PCINT0) | _BV(PCINT1);		// PA0, PA1
	PCMSK2 = _BV(PCINT18) | _BV(PCINT21);	// PC2, PC5
	PCMSK3 = _BV(PCINT30) | _BV(PCINT31);	// PD6, PD7
	PCICR = _BV(PCIE0) | _BV(PCIE2) | _BV(PCIE3);
	sei();					// Enable interrupts

	mNormalFont = inNormalFont;
	mSmallFont = inSmallFont;
	SetDisplay(inDisplay, inNormalFont);
	mPINEditor.Initialize(this);
	EEPROM.get(Config::kPINAddr, mPIN);
	if (mPIN > 9999)
	{
		mPIN = 0;
	}
	{
		uint8_t	flags;
		EEPROM.get(Config::kFlagsAddr, flags);
		//mSleepEnabled = (flags & _BV(Config::kEnableSleepBit)) != 0;
		UnixTime::SetFormat24Hour((flags &  _BV(Config::k12HourClockBit)) == 0);	// Default is 12 hour.
		mTempIsCelsius = (flags & _BV(Config::kTempUnitBit)) == 0;
		mAlarmIsOn = (flags & _BV(Config::kAlarmIsOffBit)) == 0;
	}
	{
		int16_t	alarmTemp;
		EEPROM.get(Config::kAlarmHighAddr, alarmTemp);
		mThermometers->SetAlarmHigh(alarmTemp);
		EEPROM.get(Config::kAlarmLowAddr, alarmTemp);
		mThermometers->SetAlarmLow(alarmTemp);
	}
#if 0
	{
		char tempStr[40];
		Serial.print(F("Count = "));
		Serial.print(mThermometers->GetCount());
		DS18B20Multidrop::CreateTempStr(mThermometers->GetAlarmHigh(), mTempIsCelsius, true, false, tempStr);
		Serial.print(F(", Alarm high = "));
		Serial.print(tempStr);
		DS18B20Multidrop::CreateTempStr(mThermometers->GetAlarmLow(), mTempIsCelsius, true, false, tempStr);
		Serial.print(F(", low = "));
		Serial.print(tempStr);
		Serial.print('\n');
	}
#endif
	eeprom_read_block(mTargetAddr, (void*)Config::kTargetAddr, sizeof(TPAddress));
	GoToInfoMode();
	mPrevMode = eForceRedraw;
	mSleepLevel = eLightSleep;	// For WakeUpDisplay
	WakeUpDisplay();
	/*
	*	mTextMessageProcessingEnabled is controlled via the USB serial interface
	*	when you need to test receiving SMS texts without actually processing them.
	*/
	mTextMessageProcessingEnabled = true;
}

/********************************** GiveTime **********************************/
/*
*	Called from loop()
*/
void LTESensor::GiveTime(void)
{
	bool	dataWasUpdated = mThermometers->Update(true);
	UpdateDisplay();
	UpdateActions();
	if (dataWasUpdated)
	{
		mThermometers->ResetTemperatureChanged();
	}
}

/******************************* UpdateDisplay ********************************/
/*
*	eWhite = static text
*	eLightBlue = pressing enter will bring up editor or perform action.
*	eMagenta = editable text.  Depending on context, direction buttons will
*			change values.
*/
void LTESensor::UpdateDisplay(void)
{
	if (mSleepLevel == eAwake)
	{
		bool updateAll = mMode != mPrevMode;
	
		if (updateAll)
		{
			mPrevMode = mMode;
			if (mMode != eSetPINMode)
			{
				mDisplay->Fill();
				InitializeSelectionRect();
			}
		}

		switch (mMode)
		{
			case eSettingsMode:
			{
				if (updateAll ||
					mPrevAlarmIsOn != mAlarmIsOn)
				{
					mPrevAlarmIsOn = mAlarmIsOn;
					DrawItemP(eAlarmStateItem, kAlarmStr, eWhite);
					DrawItemValueP(mAlarmIsOn ? PSTR("ON") : PSTR("OFF"), eMagenta);
				}
				if (updateAll ||
					mPrevPIN != mPIN)
				{
					mPrevPIN = mPIN;
					DrawItemP(ePINItem, kPINStr, eWhite);
					char pinStr[PINEditor::eNumPINFields+1];
					{
						uint16_t	pinValue = mPIN;
						for (uint8_t i = PINEditor::eNumPINFields; i;)
						{
							i--;
							pinStr[i] = (pinValue%10) + '0';
							pinValue /= 10;
						}
						pinStr[PINEditor::eNumPINFields] = 0;
					}
					SetTextColor(eLightBlue);
					DrawStr(pinStr, true);
				}
				if (updateAll)
				{
					DrawItemP(eAlarmSettingsItem, PSTR("Alarm Settings"), eLightBlue);
				}
				if (updateAll ||
					mPrevTempIsCelsius != mTempIsCelsius)
				{
					mPrevTempIsCelsius = mTempIsCelsius;
					DrawItemP(eTempFormatItem, kTempUnitStr, eWhite);
					DrawItemValueP(mTempIsCelsius ? DS18B20Multidrop::kDegCelsiusStr : DS18B20Multidrop::kDegFahrenheitStr, eMagenta);
				}
				if (updateAll ||
					mPrevFormat24Hour != UnixTime::Format24Hour())
				{
					mPrevFormat24Hour = UnixTime::Format24Hour();
					DrawItemP(eTimeFormatItem, kClockStr, eWhite);
					DrawItemValueP(UnixTime::Format24Hour() ? k24HStr : k12HStr, eMagenta);
				}
			#if 0
				if (updateAll ||
					mPrevSleepEnabled != mSleepEnabled)
				{
					mPrevSleepEnabled = mSleepEnabled;
					DrawItemP(eEnableSleepItem, PSTR("Sleep: "), eWhite);
					DrawItemValueP(mSleepEnabled ? PSTR("Enabled") : PSTR("Disabled"), eMagenta);
				}
			#endif
				break;
			}
			case eInfoMode:
			{
				if (mTimeIsValid)
				{
					if (UnixTime::TimeChanged())
					{
						UnixTime::ResetTimeChanged();
						char timeStr[32];
						bool isPM = UnixTime::CreateTimeStr(timeStr);

						DrawItem(eRSSITimeBatItem, timeStr, eCyan, 45);
						//DrawCenteredItem(eRSSITimeBatItem, timeStr, eCyan);
						/*
						*	If updating everything OR
						*	the AM/PM suffix state changed THEN
						*	draw or erase the suffix.
						*/
						if (updateAll ||
							mPrevIsPM != isPM ||
							!mPrevTimeIsValid)
						{
							mPrevIsPM = isPM;
							mPrevTimeIsValid = true;
							if (!UnixTime::Format24Hour())
							{
								SetFont(mSmallFont);
								DrawStr(isPM ? " PM":" AM");
								SetFont(mNormalFont);
								// The width of a P is slightly less than an A, so erase any
								// artifacts left over when going from A to P.
								// The width of an 18pt A - the width of a P = 1
								mDisplay->FillBlock(FontRows(), 1, eBlack);
							}
						}
					}
				} else if (updateAll ||
					mTimeIsValid != mPrevTimeIsValid)
				{
					mPrevTimeIsValid = false;
					DrawItemP(eRSSITimeBatItem, PSTR("__:__:__"), eGray, 45);
				}
				
				/*
				*	Update RSSI bars.
				*	White bars = connected
				*	Yellow bars = connected roaming
				*	Red bars = not able to connect (if any bars)
				*/
				{
					uint8_t	connectionStatus = SIM7000::ConnectionStatus();
					bool updateBars = updateAll || mPrevConnectionStatus != connectionStatus;
					mPrevConnectionStatus = connectionStatus;
					uint16_t	barColor = eWhite;
					if (connectionStatus != 1)
					{
						barColor = connectionStatus != 5 ? eRed : eYellow;
					}
					if (updateBars)
					{
						mDisplay->MoveTo(4*Config::kFontHeight+38, Config::kTextInset);
						for (uint8_t i = 0; i < 4; i++)
						{
							mDisplay->FillBlock(2, 6, barColor);
							mDisplay->MoveColumnBy(2);
						}
					}
					{
						uint8_t	bars = SIM7000::Bars()/10;
						if (updateBars ||
							mPrevBars != bars)
						{
							mPrevBars = bars;
							mDisplay->MoveTo(4*Config::kFontHeight+23, Config::kTextInset);
							uint8_t	barHt = 15;
							for (uint8_t i = 0; i < 4; i++)
							{
								mDisplay->FillBlock(barHt, 6, bars > i ? barColor : eBlack);
								uint8_t moveRowBy = i ? 7 : 6;
								mDisplay->MoveRowBy(-moveRowBy);
								barHt += moveRowBy;
								mDisplay->MoveColumnBy(2);
							}
						}
					}
				}
				
				/*
				*	Update the battery level.  The battery level comes from
				*	the SIM7000, so the SIM7000 needs to be awake for this to
				*	be accurate.
				*/
				{
					const uint16_t	kBatTop = 4*Config::kFontHeight+30;
					const uint16_t	kBatLeft = 200;
					const uint16_t	kBatIndWidth = 22;
					const uint16_t	kBatIndHeight = 8;
					if (updateAll)
					{
						mDisplay->FillRect(kBatLeft,kBatTop+5,
											4, 6, eWhite);
						mDisplay->DrawFrame(kBatLeft+4, kBatTop,
											kBatIndWidth+8, kBatIndHeight+8,
											eWhite);
					}
					if (updateAll ||
						mPrevBatteryLevel != mBatteryLevel)
					{
						uint16_t	batIndWidth = (kBatIndWidth * mBatteryLevel)/100;
						mPrevBatteryLevel = mBatteryLevel;
						if (batIndWidth)
						{
							mDisplay->FillRect(kBatLeft+8, kBatTop+4,
											batIndWidth, kBatIndHeight, eWhite);
						}
						if (batIndWidth < kBatIndWidth)
						{
							mDisplay->FillRect(batIndWidth+kBatLeft+8, kBatTop+4,
											kBatIndWidth-batIndWidth, kBatIndHeight, eBlack);
						}
					}
				}
				
				if (updateAll ||
					mThermometers->TemperatureChanged())
				{
					char tempStr[20];
					uint8_t	count = mThermometers->GetCount();
					for (uint8_t i = 0; i < count; i++)
					{
						if (updateAll ||
							mThermometers->TemperatureChanged(i))
						{
							if (mThermometers->DataIsValid())
							{
								CreateIndexedTempStr(i, true, false, tempStr);
								DrawItem(i, tempStr,
											mThermometers->Alarm(i) ? eRed : eGreen,
												Config::kTextInset, true);
							} else
							{
								DrawItemP(i, PSTR("-----"), eGray, Config::kTextInset+38, true);
							}
						}
					}
				}
				break;
			}
			case eMessageMode:
				if (updateAll)
				{
					DrawCenteredDescP(0, mMessageLine0);
					if (mMessageLine1 == eErrorNumDesc)
					{
						DrawDescP(1, mMessageLine1);
						char errorNumStr[15];
						UInt8ToDecStr(mError, errorNumStr);
						DrawStr(errorNumStr, true);
					} else
					{
						DrawCenteredDescP(1, mMessageLine1);
					}
					DrawCenteredDescP(2, eOKItemDesc);
					mCurrentFieldOrItem = eOKItemItem;
					mSelectionFieldOrItem = 0;	// Force the selection frame to update
				}
				break;
			case eSetPINMode:
				mPINEditor.Update();
				break;
			case eAlarmSettingsMode:
				if (updateAll)
				{
					DrawItemP(eTargetNumberItem, PSTR("#: "), eWhite);
					if (mTargetAddr[0] != 0xFF)
					{
						DrawStr(mTargetAddr, false);
					}
					char	tempStr[20];
					char*	tempPtr = strcpy_P(tempStr, PSTR("High: ")) + 6;
					DS18B20Multidrop::CreateTempStr(mThermometers->GetAlarmHigh(),
										mTempIsCelsius, true, false, tempPtr);
					DrawItem(eHighAlarmTempItem, tempStr, eWhite);
					tempPtr = strcpy_P(tempStr, PSTR("Low: ")) + 5;
					DS18B20Multidrop::CreateTempStr(mThermometers->GetAlarmLow(),
										mTempIsCelsius, true, false, tempPtr);
					DrawItem(eLowAlarmTempItem, tempStr, eWhite);
					DrawItemP(eSendTestSMSItem, PSTR("Send Test SMS"), eLightBlue);
				}
				break;
		}
		
		/*
		*	Set PIN mode has its own selection frame...
		*/
		if (mMode != eSetPINMode)
		{
			UpdateSelectionFrame();
		}
	}
}

/******************************* UpdateActions ********************************/
void LTESensor::UpdateActions(void)
{
	/*
	*	Sleep modes
	*
	*	In all sleep modes the power button is monitored to either wakeup or
	*	shutdown the board.
	*
	*	When eAwake:
	*		- the display is on
	*		- the USB serial connection is monitored.
	*		- the 5 UI buttons are monitored.
	*		- the SIM7000 is awake
	*		- the thermometers are checked.
	*		- when the power button is held for more than 2 seconds, the board
	*		  will enter deep sleep (eDeepSleep)
	*
	*	Light sleep is entered when there is no activity for N seconds as set
	*	by UnixTime::sSleepDelay (default) or by explicit override by calling
	*	UnixTime::SetSleepDelay().  The default is 90 seconds.
	*
	*	In all sleep modes other than eAwake, the display is off.
	*
	*	When in eLightSleep:
	*		- the USB serial connection is monitored.
	*		- pressing the power button or any of the 5 UI buttons will
	*		  transition to eAwake.
	*		- the SIM7000 is awake
	*		- the thermometers are checked.
	*
	*	When in eDeepSleep:
	*		- the USB serial connection is not monitored.
	*		- the SIM7000 is sleeping
	*		- the thermometers are not checked.
	*		- the cpu clock is stopped.
	*		- when the power button is held for more than 2 seconds, the board
	*		  will wake up (eAwake)
	*	
	*	How the alarm behaves.
	*
	*	When the mAlarmIsOn and any thermometer goes above or below the alarm
	*	thresholds, a single warning SMS is sent.  After confirmation that the
	*	message was successfully sent, the alarm is turned off.  After the alarm
	*	is off, it can only be turned on either by SMS, via the Alarm Settings
	*	panel, or resetting the mcu.  The alarm is only turned off in RAM not
	*	EEPROM.
	*
	*				>>>> ePeriodicSleep is NOT implemented <<<<
	*
	*	I was originally going to support ePeriodicSleep as described below but
	*	found that the power savings didn't justify the work required to
	*	implement it.  Nor did it justify the lack of responsiveness to incoming
	*	SMSs.  With a 3000mAh battery, the device should last at least 4 days in
	*	the idle state.
	*
	*	The issues that would need to be worked out are related to how to manage
	*	the SIM7000 startup, wait time for incoming SMSs and sleep along with
	*	monitoring the thermometers.  Based on few tests, an incoming SMS can
	*	take as much as 4 minutes to arrive after the SIM7000 connects to the
	*	tower.  There isn't much point in going to sleep after waiting 4 minutes
	*	of a 5 minute period between SMS checks.  On the otherhand, if there is
	*	a constant connection to the network, SMSs arrive quite quickly.
	*
	*	ePeriodicSleep would have been entered when the duration of eLightSleep
	*	exceeded UnixTime::sSleepDelay seconds (default 90 seconds). In order to
	*	go from eAwake to ePeriodicSleep there should be no activity for 3
	*	minutes.  There is no way via the UI to enter ePeriodicSleep.
	*
	*	When in ePeriodicSleep:
	*		- the USB serial connection is not monitored.
	*		- the SIM7000 is periodically woken to check for any incoming
	*		  SMS messages.	(approximately once every 5 minutes)
	*		- the thermometers are checked.  (approximately once every minute)
	*		- when the power button is pressed for more than 2 seconds the board
	*		  will exit ePeriodicSleep and transition to eAwake.
	*
	*	The thermometer and SIM7000 periodic wake periods are a multiple of the
	*	mcu watchdog tick (converted to ms) used in PeriodicSleep().  The
	*	inexact watchdog tick is used because the RTC isn't running durning
	*	sleep.  The elapsed wake time in ms is added to the wake period to
	*	account for the time the MCU isn't sleeping.
	*
	*	The thermometers were to be checked more often than the SIM7000 because
	*	it takes much less time to check the thermometers.
	*/

	/*
	*	If the MCU is awake...
	*/
	if (mSleepLevel <= eLightSleep)
	{
		/*
		*	If the thermometers are being monitored AND
		*	an alarm SMS hasn't already been queued to be sent AND
		*	any themometer is in the alarm state THEN
		*	queue an alarm SMS to be sent followed by turning off the alarm.
		*/
		if (mThermometers->DataIsValid() &&
			mAlarmIsOn &&
			!mWaitingToTurnAlarmOff &&
			mThermometers->Alarm())
		{
			/*
			*	After the alarm is successfully sent, the alarm is turned off
			*	to avoid multiple SMSs being sent.
			*
			*	mWaitingToTurnAlarmOff makes the assumption that there is only
			*	one SMS being sent at any time.
			*/
			mWaitingToTurnAlarmOff = QueueSMSReply(eQueryReply);
		} else if (SMSStatus() >= eSMSSent)
		{
			/*
			*	This doesn't handle the case where the send fails other than
			*	treating it the same as successfully sent. The SMS is only sent
			*	if there is an established connection to a tower.  A failure to
			*	send at this point in the code would be considered a fatal
			*	error, either a coding error or provisioning error. Either way I
			*	don't know how an error should be handled in this case. It's not
			*	like you can keep trying to send an SMS if you don't understand
			*	why it's failing.
			*/
			ResetSMSStatus(); // SMS result handled.  Allow new SMSs to be sent.
			if (mWaitingToTurnAlarmOff)
			{
				mWaitingToTurnAlarmOff = false;
				/*
				*	Turning the alarm off will require the user to turn it
				*	back on in order to continue monitoring the thermometers.
				*	The user can still manually send an SMS to get the status.
				*/
				SetAlarm(false);
			}
		}
	}
#ifdef SUPPORT_PERIODIC_SLEEP
// NOT fully implemented, see notes above
	else if (mSleepLevel == ePerformingTempCheck)
	{
		if (mAlarmIsOn &&
			!mWaitingToTurnAlarmOff &&
			mThermometers->DataUpdated())
			mThermometers->Alarm())
		{
			/*
			*	After the alarm is successfully sent, the alarm is turned off
			*	to avoid multiple SMSs being sent.
			*
			*	mWaitingToTurnAlarmOff makes the assumption that there is only
			*	one SMS being sent at any time.
			*/
			mWaitingToTurnAlarmOff = QueueSMSReply(eQueryReply);
		}
	} else if (sWatchdogTick)
	{
		sWatchdogTick = false;
		if (mCheckTempTime >= CHECK_TEMP_TICKS)
		{
			mCheckTempTime = 0;
			if (mAlarmIsOn &&
				mThermometers->Alarm())
			{
				WakeUpDisplay();
			} else
			{
				PeriodicSleep();
			}
		} else if (mCheckForSMSTime >= CHECK_FOR_SMS_TICKS)
		{
			WakeUpSIM7000();
		} else
		{
			PeriodicSleep();
		}
	}
#endif
	
	SIM7000::Update();
	
	/*
	*	If entering deep sleep AND
	*	the SIM7000 is finally asleep THEN
	*	put the MCU to sleep.
	*/
	if (mSleepLevel == eEnteringDeepSleep &&
		SIM7000::IsSleeping())
	{
		DeepSleep();
	}
	
#ifdef SUPPORT_PERIODIC_SLEEP
// NOT fully implemented, see notes above
	/*
	*	If entering periodic sleep AND
	*	the SIM7000 is finally asleep THEN
	*	put the MCU to sleep.
	*/
	if (mSleepLevel == eEnteringPeriodicSleep &&
		SIM7000::IsSleeping())
	{
		PeriodicSleep();
	}
#endif

	/*
	*	If awake OR in light sleep AND
	*	serial data was received THEN
	*	check for serial commands.
	*/
	if (mSleepLevel <= eLightSleep &&
		Serial.available())
	{
		bool hasTimeout = true;
		switch (Serial.read())
		{
			case '>':	// Set the time.  A hexadecimal ASCII UNIX time follows
				UnixTime::SetUnixTimeFromSerial();
				break;
			case '\x1B':
				mSerial.print('\x1B');	// Forward ESC
				break;
			case '\x1A':
				mSerial.print('\x1A');	// Forward CTRL-Z
				break;
			case 'w':
				SIM7000::WakeUp();
				break;
			case 's':
				SIM7000::ClearError();
				SIM7000::Sleep();
				break;
			case 'r':
				Serial.print(F("Resetting\n"));
				SIM7000::Reset();
				break;
			case 'a':
				hasTimeout = false;	// fall through
			case 'A':
			{
			/*
				Read and forward an AT command to the SIM7000.
				NOT for commands that the SIM7000's response doesn't end
				with a newline unless you account for this.
				
				ATE0			Turn off echo
				AT+CCLK?		Clock +CCLK: <time>
				AT+CLTS=?		Get Local Timestamp +CLTS: "yy/MM/dd,hh:mm:ss+/-zz"
				AT+CLTS=1		Allow tower to update the local RTC
				AT+CMEE=2		Use verbose error response
				AT+CGATT=1		Attach or Detach from GPRS Service
				AT+CREG?		Check your network connection
				AT+COPS=?		Report what carriers are seen by the module.
					+COPS: 
						(2,"Verizon Wireless","VzW","311480",7),  // 7 at the end means "LTE"
						(1,"311 588","311 588","311588",7),
						(1,"311 589","311 589","311589",7),
						(1,"AT&T","AT&T","310410",7),
						(1,"U.S.Cellular","USCC","311580",7),
						(3,"313 100","313 100","313100",7),,
						(0,1,2,3,4),
						(0,1,2)
				AT+COPS=4,0,"Verizon Wireless",7	Manually connect to Verizon using long format
				AT+COPS=4,1,"VzW",7	Manually connect to Verizon using short name
				AT+COPS=4,2,"311480",7	Manually connect to Verizon using carrier code
				AT+COPS=4,0,"AT&T"
				AT+COPS=2	Disconnect
				AT+CPSI?		Get UE system information
				AT+GSN			Get TA Serial Number Identification (IMEI)
				AT+GSV			Get SIM7000A info (version, etc.)
				AT+IPR?			Get baud rate (0 means autobaud)
				AT+IPR=9600		Set baud rate to 9600 (by default it's set to autobaud, which is a pain)
				AT+CSQ			Get RSSI (signal strength).  The 2nd value
								returned is the error rate, which is generally 99,
								meaning unknown or undetectable.
				AT+CNUM			Get the subscriber phone number (doesn't work for ThingSpace SIMs)
				AT+CBC			Get Battery Charge 0,95,4246 = not charging, 95%, 4.246 volts.
				AT+CIMI			Get SIM IMI
				AT+CCID			Get SIM CCID
				AT+CPIN?		Get PIN info (Response READY means no PIN needed or one was already entered)
				AT+CNMI=0		Buffer SMS messages received indications.
				AT+CMGR = <index>	Read SMS message referenced by index
				AT+CMGD= <index>	Delete SMS message at index
				AT+CNMI=2		Buffer and immediately send SMS received indications to mcu
				AT+CPMS?		SIM message usage and capacity. (Verizon SIM capacity is 15)
				AT+CSCA="+316540951000",145	Set SMSC number used in sending SMS texts (published Verizon)
				AT+CSCA="+19036384682",145	Set SMSC number used in sending SMS texts (unpublished Verizon)
				AT+CSCA?		Check SMSC number
				AT+CMGF?		Check SMS mode
				AT+CMGS="1508813xxxx"
				AT+CMGF=1;+CMGS="1508528xxxx"	Set SMS text mode, start SMS by setting phone number.
												The expected response is '> '.  You then enter the
												message string followed by control-Z (0x1A)
			*/
				char commandStr[128];
				commandStr[0] = 'A';
				if (SerialUtils::LoadLine(128, &commandStr[1], false))
				{
					SIM7000::SendCommand(commandStr, 0, hasTimeout ? 2000:0);	// 0 = no timeout
				}
				break;
			}
			case 'C':
			{
				CheckLevels();
				break;
			}
			case 'd':
				SetDeleteMessagesAfterRead(true);
				break;
			case 'D':
				SetDeleteMessagesAfterRead(false);
				break;
			case 'M':	// Turn on text message processing
				EnableTextMessageProcessing(true);
				Serial.print(F("Text processing on\n"));
				break;
			case 'm':	// Turn off text message processing
				EnableTextMessageProcessing(false);
				Serial.print(F("Text processing off\n"));
				break;
			case 'S':	// Return the SMS status
				Serial.print(F("SMS Status = "));
				Serial.print(mSMSStatus, DEC);
				Serial.print('\n');
				break;
		}
	}

	if (mSleepLevel <= eLightSleep)
	{
		if (sButtonPressed)
		{
			/*
			*	Wakeup the display when any key is pressed.
			*/
			WakeUpDisplay();
			/*
			*	If a debounce period has passed
			*/
			{
				uint8_t		pinsState = ((~PIND) & Config::kPINDBtnMask) +
											((~PINC) & Config::kPINCBtnMask) +
												((~PINA) & Config::kPINABtnMask);

				/*
				*	If debounced
				*/
				if (mStartPinState == pinsState)
				{
					if (mDebouncePeriod.Passed())
					{
						sButtonPressed = false;
						mStartPinState = 0xFF;
						if (!mIgnoreButtonPress)
						{
							switch (pinsState)
							{
								case Config::kUpBtn:	// Up button pressed
									UpDownButtonPressed(false);
									break;
								case Config::kEnterBtn:	// Enter button pressed
									EnterPressed();
									break;
								case Config::kLeftBtn:	// Left button pressed
									LeftRightButtonPressed(false);
									break;
								case Config::kDownBtn:	// Down button pressed
									UpDownButtonPressed(true);
									break;
								case Config::kRightBtn:	// Right button pressed
									LeftRightButtonPressed(true);
									break;
								case Config::kPowerBtn:	// Power button pressed
									if (mDebouncePeriod.Get() == DEBOUNCE_DELAY)
									{
										mDebouncePeriod.Set(DEEP_SLEEP_DELAY);
										mDebouncePeriod.Start();
										sButtonPressed = true;
										mStartPinState = Config::kPowerBtn;
									} else
									{
										GoToDeepSleep();
										mDebouncePeriod.Start();
									}
									break;
								default:
									mDebouncePeriod.Start();
									break;
							}
						} else
						{
							mIgnoreButtonPress = false;
						}
					}
				} else
				{
					mStartPinState = pinsState;
					/*
					*	The mDebouncePeriod is set to DEBOUNCE_DELAY in case it
					*	was set to DEEP_SLEEP_DELAY without actually going into
					*	deep sleep.  This can happen when the power button is
					*	released before the DEEP_SLEEP_DELAY period passed.
					*/
					mDebouncePeriod.Set(DEBOUNCE_DELAY);
					mDebouncePeriod.Start();
				}
			}
		} else if (UnixTime::TimeToSleep() &&
			mMode < eSetPINMode)	// Don't allow mode change when the mode is modal.
		{
			/*
			*	If sleep is enabled THEN
			*	put the display to sleep.
			*/
			if (mSleepEnabled)
			{
				PutDisplayToSleep();
			}
			GoToInfoMode();
		}
	/*
	*	Else if a button was pressed while sleeping THEN
	*	determine if it's a valid combination to wake the board up from sleep.
	*/
	} else if (sButtonPressed)
	{
		/*
		*	If the power button is pressed THEN
		*	this is the valid button to wakeup the board.
		*/
		if (((~PINC) & Config::kPINCBtnMask) == Config::kPowerBtn)
		{
			if (UnixTime::TimeToSleep())
			{
				UnixTime::ResetSleepTime();
				/*
				*	When pressed after eLightSleep the period is DEBOUNCE_DELAY.
				*	When pressed after eDeepSleep the period is DEEP_SLEEP_DELAY.
				*	This means that in order to wakeup from deep sleep the power
				*	button must be held down for DEEP_SLEEP_DELAY ms.
				*/
				mDebouncePeriod.Start();
			/*
			*	else if the debounce period has passed THEN
			*	wake the board up.
			*/
			} else if (mDebouncePeriod.Passed())
			{
				sButtonPressed = false;
				WakeUpDisplay();
			}
		/*
		*	Else something other than the power button was pressed
		*/
		} else
		{
			sButtonPressed = false;
			mDebouncePeriod.Start();
			if (mSleepLevel == eDeepSleep)
			{
				DeepSleep();
		#ifdef SUPPORT_PERIODIC_SLEEP
			} else if (mSleepLevel == ePeriodicSleep)
			{
				PeriodicSleep();
		#endif
			}
		}
#ifdef SUPPORT_PERIODIC_SLEEP
	} else if (mSleepLevel == ePeriodicSleep)
	{
		PeriodicSleep();
#endif
	} else if (mSleepLevel == eDeepSleep)
	{
		DeepSleep();
	}
}

/********************************** SetAlarm **********************************/
void LTESensor::SetAlarm(
	bool	inAlarmIsOn)
{
	if (mAlarmIsOn != inAlarmIsOn)
	{
		mAlarmIsOn = inAlarmIsOn;
		uint8_t	flags;
		EEPROM.get(Config::kFlagsAddr, flags);
		if (mAlarmIsOn)
		{
			flags &= ~_BV(Config::kAlarmIsOffBit);	// 0
		} else
		{
			flags |= _BV(Config::kAlarmIsOffBit);	// 1
		}
		EEPROM.put(Config::kFlagsAddr, flags);
	}
}

/**************************** UpDownButtonPressed *****************************/
void LTESensor::UpDownButtonPressed(
	bool	inIncrement)
{
	switch (mMode)
	{
		case eSettingsMode:
			if (inIncrement)
			{
				if (mCurrentFieldOrItem < eLastSettingsItem)
				{
					mCurrentFieldOrItem++;
				} else
				{
					mCurrentFieldOrItem = 0;
				}
			} else if (mCurrentFieldOrItem > 0)
			{
				mCurrentFieldOrItem--;
			} else
			{
				GoToInfoMode();
			}
			break;
		case eInfoMode:
			mMode = eSettingsMode;
			mCurrentFieldOrItem = eAlarmStateItem;
			ShowSelectionFrame();
			break;
		case eSetPINMode:
			mPINEditor.UpDownButtonPressed(!inIncrement);
			break;
		case eAlarmSettingsMode:
			if (inIncrement)
			{
				if (mCurrentFieldOrItem < eSendTestSMSItem)
				{
					mCurrentFieldOrItem++;
				} else
				{
					mCurrentFieldOrItem = 0;
				}
			} else if (mCurrentFieldOrItem > 0)
			{
				mCurrentFieldOrItem--;
			} else
			{
				mMode = eSettingsMode;
				mCurrentFieldOrItem = eAlarmSettingsItem;
			}
			break;
	}
}

/******************************** EnterPressed ********************************/
void LTESensor::EnterPressed(void)
{
	switch (mMode)
	{
		case eSettingsMode:
			switch (mCurrentFieldOrItem)
			{
				case eAlarmStateItem:
					LeftRightButtonPressed(true);
					break;
				case ePINItem:
					mMode = eSetPINMode;
					mPINEditor.SetPIN(mPIN);
					break;
				case eAlarmSettingsItem:
					mMode = eAlarmSettingsMode;
					mCurrentFieldOrItem = 0;
					break;
				case eTempFormatItem:
				case eTimeFormatItem:
				//case eEnableSleepItem:
					LeftRightButtonPressed(true);
					break;
			}
			break;
		case eSetPINMode:
			// If enter was pressed on SET or CANCEL
			if (mPINEditor.EnterPressed())
			{
				if (!mPINEditor.CancelIsSelected())
				{
					mPIN = mPINEditor.GetPIN();
					EEPROM.put(Config::kPINAddr, mPIN);
				}
				mMode = eSettingsMode;
				mCurrentFieldOrItem = ePINItem;
			}
			break;
		case eAlarmSettingsMode:
			switch(mCurrentFieldOrItem)
			{
				case eTargetNumberItem:
				case eHighAlarmTempItem:
				case eLowAlarmTempItem:
					QueueMessage(eSetViaSMSDesc,
						eNoMessage, eAlarmSettingsMode, mCurrentFieldOrItem);
					break;
				case eSendTestSMSItem:
					// If there's a command or SMS send in progress then this
					// will fail.
					if (ClearToSendSMS() &&
						QueueSMSReply(eQueryReply))
					{
						QueueMessage(eTestSMSSentDesc,
							eNoMessage, eAlarmSettingsMode, eSendTestSMSItem);
					} else
					{
						QueueMessage(eCantSendSMSDesc,
							ConnectionStatus() != 1 ? ekNoSignalDesc : ekBusyDesc,
								eAlarmSettingsMode, eSendTestSMSItem);
					}
					break;
			}
			break;
		case eMessageMode:
			if (mMessageReturnMode == eInfoMode)
			{
				GoToInfoMode();
			} else
			{
				mMode = mMessageReturnMode;
				mCurrentFieldOrItem = mMessageReturnItem;
			}
			break;
	}
	UnixTime::ResetSleepTime();
}

/******************************** QueueMessage ********************************/
// Not a queue.  Only one message at a time is supported.
void LTESensor::QueueMessage(
	uint8_t	inMessageLine0,
	uint8_t	inMessageLine1,
	uint8_t	inReturnMode,
	uint8_t	inReturnItem)
{
	mMessageLine0 = inMessageLine0;
	mMessageLine1 = inMessageLine1;
	mMode = eMessageMode;
	mMessageReturnMode = inReturnMode;
	mMessageReturnItem = inReturnItem;
}

/*************************** LeftRightButtonPressed ***************************/
void LTESensor::LeftRightButtonPressed(
	bool	inIncrement)
{
	switch (mMode)
	{
		case eInfoMode:
			/*if (inIncrement)
			{
				if (mBatteryLevel < 100)
				{
					mBatteryLevel++;
				}
			} else if (mBatteryLevel)
			{
				mBatteryLevel--;
			}
			Serial.print(F("BL = "));
			Serial.println(mBatteryLevel);*/
			break;
		case eSettingsMode:
			switch(mCurrentFieldOrItem)
			{
				case eAlarmStateItem:
				{
					SetAlarm(!mAlarmIsOn);
					break;
				}
			#if 0
				case eEnableSleepItem:
				{
					mSleepEnabled = !mSleepEnabled;
					uint8_t	flags;
					EEPROM.get(Config::kFlagsAddr, flags);
					if (mSleepEnabled)
					{
						flags |= _BV(Config::kEnableSleepBit);	// 1
					} else
					{
						flags &= ~_BV(Config::kEnableSleepBit);	// 0
					}
					EEPROM.put(Config::kFlagsAddr, flags);
					break;
				}
			#endif
				case eTimeFormatItem:
				{
					UnixTime::SetFormat24Hour(!UnixTime::Format24Hour());
					uint8_t	flags;
					EEPROM.get(Config::kFlagsAddr, flags);
					if (UnixTime::Format24Hour())
					{
						flags &= ~_BV(Config::k12HourClockBit);	// 0
					} else
					{
						flags |= _BV(Config::k12HourClockBit);	// 1
					}
					EEPROM.put(Config::kFlagsAddr, flags);
					break;
				}
				case eTempFormatItem:
				{
					mTempIsCelsius = !mTempIsCelsius;
					uint8_t	flags;
					EEPROM.get(Config::kFlagsAddr, flags);
					if (mTempIsCelsius)
					{
						flags &= ~_BV(Config::kTempUnitBit);	// 0
					} else
					{
						flags |= _BV(Config::kTempUnitBit);	// 1
					}
					EEPROM.put(Config::kFlagsAddr, flags);
					break;
				}
			}
			break;
		case eSetPINMode:
			mPINEditor.LeftRightButtonPressed(inIncrement);
			break;
		case eAlarmSettingsMode:
			switch (mCurrentFieldOrItem)
			{
				case eTargetNumberItem:
				case eHighAlarmTempItem:
				case eLowAlarmTempItem:
					EnterPressed();
					break;
			}
			break;
	}
}

/**************************** CreateIndexedTempStr ****************************/
/*
*	Returns a pointer to the nul terminator.
*/
char* LTESensor::CreateIndexedTempStr(
	uint8_t	inIndex,
	bool	inAppendAsteriskIfAlarm,
	bool	inUse7Bit,
	char*	outStr)
{
	// Note that a colon is used in place of square brackets to conform to the
	// single byte sms characters set (square brackets would be 2 bytes each.)
	// See TPDU::Pack7BitToPDU() for the list of limitations.
	outStr[0] = ' ';
	outStr[1] = inIndex + '0';	// Assumes inIndex 0 to 9
	outStr[2] = ':';
	outStr[3] = ' ';
	outStr+=4;
	outStr += mThermometers->CreateTempStr(inIndex, mTempIsCelsius, true, inUse7Bit, outStr);
	outStr += (inUse7Bit ? 3 : 5);
	if (inAppendAsteriskIfAlarm &&
		mThermometers->Alarm(inIndex))
	{
		outStr[0] = ' ';
		outStr[1] = '*';
		outStr += 2;
		*outStr = 0;
	}

	return(outStr);
}

/********************************* ClearLines *********************************/
void LTESensor::ClearLines(
	uint8_t	inFirstLine,
	uint8_t	inNumLines)
{
	mDisplay->MoveTo(inFirstLine*Config::kFontHeight, 0);
	mDisplay->FillBlock(inNumLines*Config::kFontHeight, Config::kDisplayWidth, eBlack);
}

/************************** InitializeSelectionRect ***************************/
void LTESensor::InitializeSelectionRect(void)
{
	mSelectionRect.x = mMode < eMessageMode ? 0 : 89;
	mSelectionRect.y = mCurrentFieldOrItem * Config::kFontHeight;
	mSelectionRect.width = mMode < eMessageMode ? Config::kDisplayWidth : 62;
	mSelectionRect.height = Config::kFontHeight;
	mSelectionFieldOrItem = mCurrentFieldOrItem;
	mSelectionIndex = 0;
}

/***************************** HideSelectionFrame *****************************/
void LTESensor::HideSelectionFrame(void)
{
	if (mSelectionPeriod.Get())
	{
		/*
		*	If the selection frame was last drawn as white THEN
		*	draw it as black to hide it.
		*/
		if (mSelectionIndex & 1)
		{
			mSelectionIndex = 0;
			mDisplay->DrawFrame8(&mSelectionRect, eBlack, 2);
		}
		mSelectionPeriod.Set(0);
	}
}

/***************************** ShowSelectionFrame *****************************/
void LTESensor::ShowSelectionFrame(void)
{
	mSelectionPeriod.Set(500);
	mSelectionPeriod.Start();
}

/**************************** UpdateSelectionFrame ****************************/
void LTESensor::UpdateSelectionFrame(void)
{
	if (mSelectionPeriod.Get())
	{
		if (mSelectionFieldOrItem != mCurrentFieldOrItem)
		{
			if (mSelectionIndex & 1)
			{
				mDisplay->DrawFrame8(&mSelectionRect, eBlack, 2);
			}
			InitializeSelectionRect();
		}
		if (mSelectionPeriod.Passed())
		{
			mSelectionPeriod.Start();
			mSelectionIndex++;
			uint16_t	selectionColor = (mSelectionIndex & 1) ? eWhite : eBlack;
			mDisplay->DrawFrame8(&mSelectionRect, selectionColor, 2);
		}
	}
}

/******************************** GoToInfoMode ********************************/
void LTESensor::GoToInfoMode(void)
{
	HideSelectionFrame();
	if (mMode != eInfoMode)
	{
		mMode = eInfoMode;
		mCurrentFieldOrItem = 0;
		InitializeSelectionRect();
	}
}

#if 0
/****************************** DrawCenteredList ******************************/
void LTESensor::DrawCenteredList(
	uint8_t		inLine,
	uint8_t		inTextEnum, ...)
{
	va_list arglist;
	va_start(arglist, inTextEnum);
	for (uint8_t textEnum = inTextEnum; textEnum; textEnum = va_arg(arglist, int))
	{
		DrawCenteredDescP(inLine, textEnum);
		inLine++;
	}
	va_end(arglist);
}
#endif
/***************************** DrawCenteredDescP ******************************/
void LTESensor::DrawCenteredDescP(
	uint8_t		inLine,
	uint8_t		inTextEnum)
{
	SStringDesc	textDesc;
	memcpy_P(&textDesc, &kTextDesc[inTextEnum-1], sizeof(SStringDesc));
	DrawCenteredItemP(inLine, textDesc.descStr, textDesc.color);
}

/********************************* DrawDescP **********************************/
void LTESensor::DrawDescP(
	uint8_t		inLine,
	uint8_t		inTextEnum,
	uint8_t		inColumn)
{
	SStringDesc	textDesc;
	memcpy_P(&textDesc, &kTextDesc[inTextEnum-1], sizeof(SStringDesc));
	DrawItemP(inLine, textDesc.descStr, textDesc.color, inColumn);
}

/***************************** DrawCenteredItemP ******************************/
void LTESensor::DrawCenteredItemP(
	uint8_t		inLine,
	const char*	inTextStrP,
	uint16_t	inColor)
{
	char			textStr[20];	// Assumed all strings are less than 20 bytes
	strcpy_P(textStr, inTextStrP);
	DrawCenteredItem(inLine, textStr, inColor);
}

/****************************** DrawCenteredItem ******************************/
void LTESensor::DrawCenteredItem(
	uint8_t		inLine,
	const char*	inTextStr,
	uint16_t	inColor)
{
	mDisplay->MoveToRow((inLine*Config::kFontHeight) + Config::kTextVOffset);
	SetTextColor(inColor);
	DrawCentered(inTextStr);
}

/********************************* DrawItemP **********************************/
void LTESensor::DrawItemP(
	uint8_t		inLine,
	const char*	inTextStrP,
	uint16_t	inColor,
	uint8_t		inColumn,
	bool		inClearTillEOL)
{
	char	textStr[20];	// Assumed all strings are less than 20 bytes
	strcpy_P(textStr, inTextStrP);
	DrawItem(inLine, textStr, inColor, inColumn, inClearTillEOL);
}

/********************************** DrawItem **********************************/
void LTESensor::DrawItem(
	uint8_t		inLine,
	const char*	inTextStr,
	uint16_t	inColor,
	uint8_t		inColumn,
	bool		inClearTillEOL)
{
	mDisplay->MoveTo((inLine*Config::kFontHeight) + Config::kTextVOffset, inColumn);
	SetTextColor(inColor);
	DrawStr(inTextStr, inClearTillEOL);
}

/******************************* DrawItemValueP *******************************/
/*
*	Draws from the current row and column, then erases till end of line.
*/
void LTESensor::DrawItemValueP(
	const char*	inTextStrP,
	uint16_t	inColor)
{
	char	textStr[20];	// Assumed all strings are less than 20 bytes
	strcpy_P(textStr, inTextStrP);
	SetTextColor(inColor);
	DrawStr(textStr, true);
}

/******************************* UInt8ToDecStr ********************************/
void LTESensor::UInt8ToDecStr(
	uint8_t	inNum,
	char*	inBuffer)
{
	for (uint8_t num = inNum; num/=10; inBuffer++);
	inBuffer[1] = 0;
	do
	{
		*(inBuffer--) = (inNum % 10) + '0';
		inNum /= 10;
	} while (inNum);
}

/******************************* WakeUpDisplay ********************************/
/*
*	Wakup the display from sleep and/or keep it awake if not sleeping.
*/
void LTESensor::WakeUpDisplay(void)
{
	if (mSleepLevel != eAwake)
	{
		// If a button press that caused the display to wake then ignore
		// the current button press (after it debounces)
		mIgnoreButtonPress = sButtonPressed;
		mDisplay->WakeUp();
		mPrevMode = eForceRedraw;
		SIM7000::SetCheckLevelsPeriod(10000);	// Every 10 seconds
		mThermometers->begin();	// Update sensor list
	}
	WakeUpSIM7000();
	mSleepLevel = eAwake;	// Overrides value set in WakeUpSIM7000()
}

/******************************* WakeUpSIM7000 ********************************/
/*
*	Wakup the SIM7000 from sleep.
*/
void LTESensor::WakeUpSIM7000(void)
{
	if (SIM7000::IsSleeping())
	{
		Serial.begin(BAUD_RATE);
		SIM7000::WakeUp();
		mSleepLevel = eLightSleep;
	}
	UnixTime::ResetSleepTime();
}

/***************************** PutDisplayToSleep ******************************/
/*
*	Puts the display to sleep.
*/
void LTESensor::PutDisplayToSleep(void)
{
	if (mSleepLevel == eAwake)
	{
		mDisplay->Fill();
		mDisplay->Sleep();
		mSleepLevel = eLightSleep;
		SIM7000::SetCheckLevelsPeriod(30000);	// every 30 seconds
	}
}

/******************************* GoToDeepSleep ********************************/
/*
*	Puts the [MCU,] SIM7000, and display to sleep.  In this mode nothing is
*	working including the RTC.  Deep sleep doesn't occur till the SIM7000 goes
*	to sleep (asynchronous.)
*/
void LTESensor::GoToDeepSleep(void)
{
	mPrevTimeIsValid = true;
	mTimeIsValid = false;
	PutDisplayToSleep();
	SIM7000::Sleep();
	mSleepLevel = eEnteringDeepSleep;
}

/********************************* DeepSleep **********************************/
/*
*	Puts the MCU, SIM7000, and display to sleep.  In this mode nothing is
*	working including the RTC.
*/
void LTESensor::DeepSleep(void)
{
	if (mSleepLevel == eEnteringDeepSleep)
	{
		/*
		*	Release the serial pins (otherwise pinMode and digitalWrite have no
		*	effect.)
		*/
		Serial.end();
		// Set both serial pins low so power doesn't backfeed to the serial board.
		pinMode(Config::kRxPin, INPUT);
		digitalWrite(Config::kRxPin, LOW);
		pinMode(Config::kTxPin, INPUT);
		digitalWrite(Config::kTxPin, LOW);
		mSleepLevel = eDeepSleep;
	}
	ATmega644RTC::RTCDisable();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	cli();
	sleep_enable();
	sleep_bod_disable();
	sei();
	
	sleep_cpu();		// Halts MCU. 
	sleep_disable();	// MCU wakes up here after interrupt

	sei();
	set_sleep_mode(SLEEP_MODE_IDLE);
	UnixTime::SetTime(0);
	ATmega644RTC::RTCEnable();
}

#ifdef SUPPORT_PERIODIC_SLEEP
// If used, GoToPeriodicSleep/PeriodicSleep should be combined with
// GoToDeepSleep/DeepSleep.
#error Watchdog implementation not finished
/***************************** GoToPeriodicSleep ******************************/
/*
*	Puts the [MCU,] SIM7000, and display to sleep.  In this mode nothing is
*	working including the RTC.  Deep sleep doesn't occur till the SIM7000 goes
*	to sleep (asynchronous.)
*/
void LTESensor::GoToPeriodicSleep(void)
{
	mPrevTimeIsValid = true;
	mTimeIsValid = false;
	PutDisplayToSleep();
	SIM7000::Sleep();
	mSleepLevel = eEnteringPeriodicSleep;
}

/******************************* PeriodicSleep ********************************/
/*
*	Puts the MCU, SIM7000, and display to sleep.  In this mode only the WTC is
*	working.
*/
void LTESensor::PeriodicSleep(void)
{
	if (mSleepLevel == eEnteringPeriodicSleep)
	{
		/*
		*	Release the serial pins (otherwise pinMode and digitalWrite have no
		*	effect.)
		*/
		Serial.end();
		// Set both serial pins low so power doesn't backfeed to the serial board.
		pinMode(Config::kRxPin, INPUT);
		digitalWrite(Config::kRxPin, LOW);
		pinMode(Config::kTxPin, INPUT);
		digitalWrite(Config::kTxPin, LOW);
		mSleepLevel = ePeriodicSleep;
	}
	ATmega644RTC::RTCDisable();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	cli();
	sleep_enable();
	sleep_bod_disable();
	sei();
	
	/*
	*	mRunStartTime is used to measure the amount of time the MCU is running
	*	before going back to sleep.  This time is then added to mCheckTempTime
	*	and mCheckForSMSTime.
	*/
	if (mRunStartTime)
	{
		uint32_t	elapsedWDTicks = millis() - mRunStartTime;
		mCheckForSMSTime += elapsedWDTicks;
		mCheckTempTime += elapsedWDTicks;
	}
	sWatchdogTick = false;
	WDTCSR |= _BV(WDIE);
	wdt_enable(WDTO_8S);// << This sets the delay time
	sleep_cpu();		// Halts MCU. 
	sleep_disable();	// MCU wakes up here after interrupt
	wdt_disable();
	mCheckTempTime+=8000;	// Each watchdog timeout is about 8s or 8000ms.
	mCheckForSMSTime+=8000;
	mRunStartTime = millis();
	
	sei();
	set_sleep_mode(SLEEP_MODE_IDLE);
	UnixTime::SetTime(0);
	ATmega644RTC::RTCEnable();
}
#endif

/******************************** MessageRead *********************************/
void LTESensor::MessageRead(
	const char*			inMessage,
	uint8_t				inMessageLen,
	const TPAddress&	inSender,
	const TPAddress&	inSMSCAddr)
{
	SIM7000::MessageRead(inMessage, inMessageLen, inSender, inSMSCAddr);
	/*
	*	Commands: (Not case sensitive - Setup == SETUP = sEtUp)
	*		- Setup	PIN [HxxF] [LxxF]	Makes inSender the target for
	*					outgoing messages and optionally sets the alarm
	*					temperatures.
	*					If a valid 4 digit PIN is supplied, the device responds
	*					to new target with OK plus text from ? command.
	*					If an invalid PIN is supplied, no response is made.
	*
	*			Ex: "Setup 9999 H30C L0C  Note only integer values are valid.
	*
	*		The following responses are only made after successfully setup,
	*		and only to the sender of the last successful setup.  All other
	*		incoming texts from other senders are ignored.
	*
	*		- On		Will report to target temperature events
	*					Responds to target with OK plus text from ? command.
	*		- Off		Will not report to target temperature events
	*					Responds to target with OK plus text from ? command.
	*		- ?			Will report to target:
	*						- alarm report status ON/OFF
	*						- the alarm temperatures
	*						- the current temperature of each sensor
	*						- the current signal strength in bars, a number from
	*						  1 to 5 (if 0 a message couldn't be sent/received.)
	*						- the battery level as a percentage from 1 to 100.
	*
	*/

	if (mTextMessageProcessingEnabled)
	{
		/*
		*	Extract the first token as the command...
		*/
		char	token[10];
		uint8_t	tokenLen = GetToken(8, inMessage, token);
		uint8_t	tokenIndex = FindTokenP(token, kSMSCommands, sizeof(kSMSCommands)/sizeof(const char*));
		switch (tokenIndex)
		{
			case eSetupCmd:
			{
				const char*	messagePtr = &inMessage[tokenLen];
				SkipWhitespaceOnLine(messagePtr);
				uint16_t	pinRead = 0;
				char	thisChar = GetUInt16Value(messagePtr, pinRead);
				if (pinRead == mPIN)
				{
					/*
					*	US numbers need to start with 1.
					*/
					if (inSender[0] != '1')
					{
						mTargetAddr[0] = '1';
						memcpy(&mTargetAddr[1], inSender, sizeof(TPAddress)-1);
					} else
					{
						memcpy(mTargetAddr, inSender, sizeof(TPAddress));
					}
				
					eeprom_update_block(inSender, (void*)Config::kTargetAddr, sizeof(TPAddress));

					while ((thisChar = SkipWhitespaceOnLine(messagePtr)))
					{
						tokenLen = GetToken(8, messagePtr, token);
						if (token[0] == 'h' ||
							token[0] == 'l')
						{
							int16_t	alarmTemp;
							const char*	tokenPtr = &token[1];
							thisChar = GetInt16Value(tokenPtr, alarmTemp);
							/*
							*	The alarm high/low values are fixed-point with a
							*	1/16 scale. (low 4 bits used for fraction.)  The
							*	value read has no fractional component.  For this
							*	reason it needs to be shifted 4 bits to the left.
							*/
							alarmTemp <<= 4;
							/*
							*	If the value read is fahrenheit OR
							*	its unit is unknown AND the default is fahrenheit THEN
							*	convert it to celsius.
							*/
							if (thisChar == 'f' ||
								(thisChar != 'c' && !mTempIsCelsius))
							{
								/*
								*	Note that conversion of F to C is only accurate
								*	to ±0.0625 °C.  In most cases the converted F
								*	value will be slightly off, but not in any
								*	meaningful way given that the sensor resolution
								*	is also at most ±0.0625 °C.  With the current
								*	settings the sensors are only accurate to ±0.5
								*	°C.  The higher the resolution the longer the
								*	sensor read takes.  To preserve battery life,
								*	the lowest resolution is used.
								*/
								alarmTemp = DS18B20Multidrop::FToC(alarmTemp);
							}
							// The value read isn't sanity checked because the
							// response to this command is to echo the values set
							// back to the sender.
							if (token[0] == 'h')
							{
								EEPROM.put(Config::kAlarmHighAddr, alarmTemp);
								mThermometers->SetAlarmHigh(alarmTemp);
							} else
							{
								EEPROM.put(Config::kAlarmLowAddr, alarmTemp);
								mThermometers->SetAlarmLow(alarmTemp);
							}
							//char alarmTempStr[10];
							//Fixed16ToDec10Str(alarmTemp, alarmTempStr);
							//fprintf(stderr, "Alarm %s = %s\n", token[0] == 'h' ? "high":"low", alarmTempStr);
						}
						messagePtr += tokenLen;
					}
					DoOnOffCmd(true);
				}
				break;
			}
			case eOnCmd:
			case eOffCmd:
				/*
				*	If this is the target sender THEN
				*	make the requested change
				*/
				if (SameAddress(mTargetAddr, inSender))
				{
					DoOnOffCmd(tokenIndex == eOnCmd);
				}
				break;
			case eQueryCmd:
				if (SameAddress(mTargetAddr, inSender))
				{
					QueueSMSReply(eQueryReply);
				}
				break;
		}
	}
}

/******************************** DoOnOffCmd *********************************/
void LTESensor::DoOnOffCmd(
	bool	inAlarmIsOn)
{
	SetAlarm(inAlarmIsOn);
	QueueSMSReply(eQueryReplyWithOK);
}

/******************************* QueueSMSReply ********************************/
/*
*	Bottleneck for SMS replies.  For now there is no queue, only a single reply
*	is supported.  There are assumptions in this class that there is only ever
*	one SMS being sent at a time.  If a queue is implemented these assumptions
*	need to be removed.
*/
bool LTESensor::QueueSMSReply(
	uint8_t	inReply)
{
	bool	queued = mSMSReply == eNoReply;
	if (queued)
	{
		mSMSReply = inReply;
	}
	return(queued);
}

/*************************** ProcessQueuedSMSReply ****************************/
void LTESensor::ProcessQueuedSMSReply(void)
{
	if (mSMSReply != eNoReply &&
		DoQueryCmdReply(mSMSReply == eQueryReplyWithOK))
	{
		mSMSReply = eNoReply;
	}
}
	
/****************************** DoQueryCmdReply *******************************/
/*
*	Alarm is ON, High 90F, Low 40F
*	Sensors: (* = alarm)
*	  0: 90.5F *
*	  1: 80.2F
*	Signal: 3.8 (5 = best)
*	Battery: 84%
*/
bool LTESensor::DoQueryCmdReply(
	bool	inPrependOK)
{
	bool	sent = false;
	if (ClearToSendSMS())
	{
		char replyStr[200];
		char*	replyPtr = replyStr;
		if (inPrependOK)
		{
			replyPtr = strcpy_P(replyStr, PSTR("OK\n")) + 3;
		}
		// Alarm Settings...
		replyPtr = strcpy_P(replyPtr, PSTR("Alarm is O")) + 10;
		if (mAlarmIsOn  && !mWaitingToTurnAlarmOff)
		{
			*(replyPtr++) = 'N';
		} else
		{
			*(replyPtr++) = 'F';
			*(replyPtr++) = 'F';
		}
		replyPtr = strcpy_P(replyPtr, PSTR(", High ")) + 7;
		replyPtr += (DS18B20Multidrop::CreateTempStr(mThermometers->GetAlarmHigh(),
											mTempIsCelsius, true, true, replyPtr) + 3);
		replyPtr = strcpy_P(replyPtr, PSTR(", Low ")) + 6;
		replyPtr += (DS18B20Multidrop::CreateTempStr(mThermometers->GetAlarmLow(),
											mTempIsCelsius, true, true, replyPtr) + 3);
		replyPtr = strcpy_P(replyPtr, PSTR("\nSensors: (* = alarm)")) + 21;
		// Sensors/Thermometers
		uint8_t	count = mThermometers->GetCount();
		if (count > 5)count = 5;
		for (uint8_t i = 0; i < count; i++)
		{
			*(replyPtr++) = '\n';
			replyPtr = CreateIndexedTempStr(i, true, true, replyPtr);
		}
		// Signal Strength
		replyPtr = strcpy_P(replyPtr, PSTR("\nSignal: ")) + 9;
		uint8_t	bars = SIM7000::Bars();
		if (bars > 50) bars = 50;
		replyPtr[0] = (bars/10) + '0';
		replyPtr[1] = '.';
		replyPtr[2] = (bars%10) + '0';
		replyPtr+=3;
		// Battery Level
		replyPtr = strcpy_P(replyPtr, PSTR(" (5 = best)\nBattery: ")) + 21;
		{
			uint8_t	batteryLevel = SIM7000::BatteryLevel();
			if (batteryLevel > 9)
			{
				*(replyPtr++) = (batteryLevel / 10) + '0';
			}
			replyPtr[0] = (batteryLevel % 10) + '0';
			replyPtr[1] = '%';
			replyPtr[2] = 0;
		}
	#if 1
		sent = SendSMS(mTargetAddr, replyStr);
	#else
		sent = true;
		Serial.print(replyStr);
		Serial.print('\n');
	#endif
	}
	return(sent);
}

/**************************** HandleNoSIMCardFound ****************************/
void LTESensor::HandleNoSIMCardFound(void)
{
	QueueMessage(eNoSIMCardDesc,
				eNoMessage, eSettingsMode, eAlarmStateItem);
}

#ifdef SUPPORT_PERIODIC_SLEEP
/******************************** watchdog ************************************/
/*
*	This gets triggered to wake up from power down so the temperature can be
*	checked on a regular basis.
*/
ISR(WDT_vect)
{
	LTESensor::WatchdogTick();
}
#endif

/************************* Pin change interrupt PCI0 **************************/
/*
*
*	Sets a flag to show that buttons have been pressed.
*	This will also wakeup the mcu if it's sleeping.
*/
ISR(PCINT0_vect)
{
	LTESensor::SetButtonPressed((PINA & Config::kPINABtnMask) != Config::kPINABtnMask);
}

/************************* Pin change interrupt PCI2 **************************/
/*
*
*	Sets a flag to show that buttons have been pressed.
*	This will also wakeup the mcu if it's sleeping.
*/
ISR(PCINT2_vect)
{
	LTESensor::SetButtonPressed((PINC & Config::kPINCBtnMask) != Config::kPINCBtnMask);
}

/************************* Pin change interrupt PCI3 **************************/
/*
*
*	Sets a flag to show that buttons have been pressed.
*	This will also wakeup the mcu if it's sleeping.
*/
ISR(PCINT3_vect)
{
	LTESensor::SetButtonPressed((PIND & Config::kPINDBtnMask) != Config::kPINDBtnMask);
}

