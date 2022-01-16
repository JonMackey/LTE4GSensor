/*
*	LTESensor.h, Copyright Jonathan Mackey 2021
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
#ifndef LTESensor_h
#define LTESensor_h

#include "XFont.h"
#include <inttypes.h>
#include <time.h>
#include "MSPeriod.h"
#include "LTESensorConfig.h"
#include "DisplayController.h"
#include "SIM7000.h"
#include "PINEditor.h"

class DS18B20Multidrop;

class LTESensor : public XFont, public SIM7000
{
public:
							LTESensor(void);
	void					begin(
								DS18B20Multidrop*		inThermometers,
								DisplayController*		inDisplay,
								Font*					inNormalFont,
								Font*					inSmallFont);
		
	void					GiveTime(void);
	static void				SetButtonPressed(
								bool					inButtonPressed)
								{sButtonPressed = sButtonPressed || inButtonPressed;}
	static void				WatchdogTick(void)
								{sWatchdogTick = true;}

protected:
	PINEditor				mPINEditor;
	DS18B20Multidrop*		mThermometers;
	Font*					mNormalFont;
	Font*					mSmallFont;
	Rect8_t					mSelectionRect;
#ifdef SUPPORT_PERIODIC_SLEEP
	uint32_t				mCheckTempTime;
	uint32_t				mCheckForSMSTime;
	uint32_t				mRunStartTime;	// ms when the MCU last woke up
#endif
	MSPeriod				mDebouncePeriod;	// For buttons and SD card
	MSPeriod				mSelectionPeriod;	// Selection frame flash rate
	
	TPAddress				mTargetAddr;
	uint16_t				mPIN;	// Initialized from EEPROM 
	uint16_t				mPrevPIN;
	uint8_t					mSleepLevel;
	uint8_t					mMode;
	uint8_t					mPrevMode;
	uint8_t					mCurrentFieldOrItem;
	uint8_t					mSelectionFieldOrItem;
	uint8_t					mStartPinState;
	uint8_t					mPrevBars;
	uint8_t					mPrevConnectionStatus;
	uint8_t					mSMSReply;
	bool					mIgnoreButtonPress;
	bool					mSleepEnabled;
	//bool					mPrevSleepEnabled;
	bool					mPrevFormat24Hour;
	bool					mTempIsCelsius;
	bool					mPrevTempIsCelsius;
	bool					mPrevIsPM;
	bool					mAlarmIsOn;
	bool					mPrevAlarmIsOn;
	bool					mPrevTimeIsValid;
	bool					mWaitingToTurnAlarmOff;
	bool					mTextMessageProcessingEnabled;
	uint8_t					mPrevBatteryLevel;
	uint8_t					mSelectionIndex;
	uint8_t					mError;	// Used by eErrorDesc
	uint8_t					mMessageLine0;
	uint8_t					mMessageLine1;
	uint8_t					mMessageReturnMode;
	uint8_t					mMessageReturnItem;
	static bool				sButtonPressed;
	static bool				sWatchdogTick;

	void					EnableTextMessageProcessing(
								bool					inEnable)
								{mTextMessageProcessingEnabled = inEnable;}
	virtual void			MessageRead(
								const char*				inMessage,
								uint8_t					inMessageLen,
								const TPAddress&		inSender,
								const TPAddress&		inSMSCAddr);
	bool					QueueSMSReply(
								uint8_t					inReply);
	virtual void			ProcessQueuedSMSReply(void);
	virtual void			HandleNoSIMCardFound(void);
	void					DoOnOffCmd(
								bool					inAlarmIsOn);
	bool					DoQueryCmdReply(
								bool					inPrependOK);
	
	void					UpdateActions(void);
	void					UpdateDisplay(void);
	uint8_t					Mode(void) const
								{return(mMode);}
	uint8_t					CurrentFieldOrItem(void) const
								{return(mCurrentFieldOrItem);}
	void					GoToInfoMode(void);
	void					EnterPressed(void);
	void					UpDownButtonPressed(
								bool					inIncrement);
	void					LeftRightButtonPressed(
								bool					inIncrement);

	void					SetAlarm(
								bool					inAlarmIsOn);
	void					CheckAlarms(void);
	void					WakeUpDisplay(void);
	void					WakeUpSIM7000(void);
	void					PutDisplayToSleep(void);
	void					GoToDeepSleep(void);
	void					DeepSleep(void);

	void					ClearLines(
								uint8_t					inFirstLine,
								uint8_t					inNumLines);
#if 0
	void					DrawCenteredList(
								uint8_t					inLine,
								uint8_t					inTextEnum, ...);
#endif
	void					DrawCenteredDescP(
								uint8_t					inLine,
								uint8_t					inTextEnum);
	void					DrawCenteredItemP(
								uint8_t					inLine,
								const char*				inTextPStr,
								uint16_t				inColor);
	void					DrawCenteredItem(
								uint8_t					inLine,
								const char*				inTextStr,
								uint16_t				inColor);
	void					DrawDescP(
								uint8_t					inLine,
								uint8_t					inTextEnum,
								uint8_t					inColumn = Config::kTextInset);
	void					DrawItemP(
								uint8_t					inLine,
								const char*				inTextPStr,
								uint16_t				inColor,
								uint8_t					inColumn = Config::kTextInset,
								bool					inClearTillEOL = false);
	void					DrawItemValueP(
								const char*				inTextPStr,
								uint16_t				inColor);
	void					DrawItem(
								uint8_t					inLine,
								const char*				inTextStr,
								uint16_t				inColor,
								uint8_t					inColumn = Config::kTextInset,
								bool					inClearTillEOL = false);
	void					UpdateSelectionFrame(void);
	void					HideSelectionFrame(void);
	void					ShowSelectionFrame(void);
	void					InitializeSelectionRect(void);
	void					QueueMessage(
								uint8_t					inMessageLine0,
								uint8_t					inMessageLine1,
								uint8_t					inReturnMode,
								uint8_t					inReturnItem);
	void					SendAlarmTextMessage(void);
	char*					CreateIndexedTempStr(
								uint8_t					inIndex,
								bool					inAppendAsteriskIfAlarm,
								bool					inUse7Bit,
								char*					outStr);
	static void				UInt8ToDecStr(
								uint8_t					inNum,
								char*					inBuffer);
	
	enum ESleepLevel
	{
		eAwake,
		eLightSleep,	// Display turned off/sleeping
		eWaitingForPeriodicSleep,	// Waits for the SMS to wakup and check for messages.
	#ifdef SUPPORT_PERIODIC_SLEEP
		eEnteringPeriodicSleep,	// Waits for SIM7000 to sleep before entering periodic sleep
		ePeriodicSleep		// eLightSleep + MCU deep sleep with watchdog and SIM7000 sleeping.
	#endif
		eEnteringDeepSleep,	// Waits for SIM7000 to sleep before entering deep sleep
		eDeepSleep		// ePeriodicSleep + no watchdog.
	};
	enum ESMSReply
	{
		eNoReply,
		eQueryReply,
		eQueryReplyWithOK
	};
	enum EMode
	{
		eSettingsMode,
		eInfoMode,
		eAlarmSettingsMode,
		// The modes below are modal (waiting for input of some sort.)
		// The display will not go to sleep when in a modal mode.
		eSetPINMode,
		eMessageMode,
		eForceRedraw
	};
	enum EInfoItem
	{
		eTimeItem,
		eTempItem0,
		eTempItem1,
		eTempItem2,
		eRSSITimeBatItem
	};
	
	/*
	*	The xxxItem enums below represent the line on which the item will draw.
	*	By moving an enum within the list you're moving where it will be drawn.
	*	They are also used to denote the current or selected item.
	*	Be aware that if the last item is changed the UpDownButtonPressed code 
	*	may need to be updated.
	*/
	enum ESettingsItem
	{
		eAlarmStateItem,
		ePINItem,
		eAlarmSettingsItem,
		eTempFormatItem,
		eTimeFormatItem,
//		eEnableSleepItem,
		eLastSettingsItem = eTimeFormatItem
	};
	enum EAlarmSettingsItem
	{
		eTargetNumberItem,
		eHighAlarmTempItem,
		eLowAlarmTempItem,
		eSendTestSMSItem
	};
	
	enum EMessageItem
	{
		eMessage0Item,
		eMessage1Item,
		eOKItemItem,
	};
	enum ETextDesc
	{
		eTextListEnd,
		// Messages
		eNoMessage,
		eOKItemDesc,
		eSetViaSMSDesc,
		eTestSMSSentDesc,
		eCantSendSMSDesc,
		ekNoSignalDesc,
		eNoSIMCardDesc,
		ekBusyDesc,
		eErrorNumDesc
	};
};

#endif // LTESensor_h
