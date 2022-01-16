/*
*	SIM7000.h
*	Copyright (c) 2022 Jonathan Mackey
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
#ifndef SIM7000_H
#define SIM7000_H

#include <HardwareSerial.h>
#include "MSPeriod.h"
#include "TPDU.h"

#define SIM7000_RX_BUFFER_SIZE	512
#define SIM7000_TX_BUFFER_SIZE	300	// Should be set to the largest Tx TPDU length

class SIM7000 : public TPDU
{
public:
	enum ESleepState
	{
		eRunning,		//	0
		eWakingUp,		//	1 (Also set when resetting)
		eGoingToSleep,	//	2
		eSleeping		//	3
	};
	enum ECommandState
	{
		eReady,			//	0
		eBusy,			//	1
		eTimeout,		//	2
		eError			//	3
	};
	enum ESMSStatus
	{
		eSMSIdle,
		eSMSSending,
		eSMSWaiting,
		eSMSSent,
		eSMSFailed
	};
							SIM7000(
								HardwareSerial&			inSerial,
								uint8_t					inRxPin,
								uint8_t					inTxPin,
								uint8_t					inPowerPin,
								uint8_t					inResetPin);
	void					begin(void);
	void					SetPassthrough(
								HardwareSerial*			inSerial = nullptr)
								{mPassthrough = inSerial;}
	void					Update(void);
	void					Sleep(void);
	bool					IsReady(void) const
								{return(mCommandState == eReady);}
	bool					IsBusy(void) const
								{return(mCommandState == eBusy);}
	bool					IsTimeout(void) const
								{return(mCommandState == eTimeout);}
	bool					IsSleeping(void) const
								{return(mSleepState == eSleeping);}
	bool					IsError(void) const
								{return(mCommandState == eError);}
	void					ClearError(void);
	void					WakeUp(void);
	void					Reset(void);
	uint8_t					SleepState(void) const
								{return(mSleepState);}
	uint8_t					CommandState(void) const
								{return(mCommandState);}
	uint8_t					Bars(void) const
								{return(mBars);}
	uint8_t					BatteryLevel(void) const
								{return(mBatteryLevel);}
	uint8_t					ConnectionStatus(void) const
								{return(mConnectionStatus);}
	void					CheckLevels(void);
	void					SetCheckLevelsPeriod(
								uint32_t				inPeriod = 0);
	bool					SendSMS(
								const char*				inPhoneNumber,
								const char*				inMessage);
	uint8_t					SMSStatus(void) const
								{return(mSMSStatus);}
	void					ResetSMSStatus(void)
								{mSMSStatus = eSMSIdle;} // Called after handling eSMSSent or eSMSFailed
							// mSMSStatus is set to idle in subclass by calling ResetSMSStatus()
							// If this is not done then no SMS texts can be sent.
	bool					ClearToSendSMS(void) const
								{return(ConnectedAndClearToSend() && mSMSStatus == eSMSIdle);}
	void					TurnOffEchoMode(
								uint8_t					inRetries = 0);
	bool					SendCommand(
								const char*				inCommandStr,
								uint16_t				inCommandHash = 0,
								uint16_t				inCommandTimeout = 1000);
	bool					SendCommand(
								const __FlashStringHelper*	inCommandStr,
								uint16_t				inCommandHash = 0,
								uint16_t				inCommandTimeout = 1000);
	bool					ConnectedAndClearToSend(void) const
								{return(ConnectionStatus() == 1 && ClearToSend());}
	inline bool				ClearToSend(void) const
								{return(!IsBusy() && digitalRead(mRxPin) != 0);}
	void					SetDeleteMessagesAfterRead(
								bool					inDeleteMessagesAfterRead)
								{mDeleteMessagesAfterRead = inDeleteMessagesAfterRead;}
								
//	void					DumpRxBuffer(void) const;
	static const __FlashStringHelper * GetSleepStateStr(
								uint8_t					inState);
	static const __FlashStringHelper * GetCommandStateStr(
								uint8_t					inState);
protected:
	uint8_t			mRxPin;
	uint8_t			mTxPin;
	uint8_t			mPowerPin;
	uint8_t			mResetPin;
	uint8_t			mSleepState;
	uint8_t			mCommandState;
	uint8_t			mRetries;
	uint8_t			mBars;
	uint8_t			mBatteryLevel;
	uint8_t			mPendingMessages[15];
	uint8_t			mPendingMessagesHead;
	uint8_t			mPendingMessagesTail;
	uint8_t			mWaitingToProcessMessage;	// Message index +1
	uint8_t			mWaitingToDeleteMessage;	// Message index +1
	uint8_t			mSMSStatus;
	bool			mTimeIsValid;	// Setting to false managed by subclass.
	bool			mRxPaused;
	bool			mDeleteMessagesAfterRead;	// Set to false to keep processed messages on SIM
	uint8_t			mConnectionStatus;
	uint16_t		mPendingCommandHash;
	uint16_t		mCommandHash;
	char			mTxBuffer[SIM7000_TX_BUFFER_SIZE];
	char			mRxBuffer[SIM7000_RX_BUFFER_SIZE];
	char*			mRxBufferPtr;
	HardwareSerial*	mPassthrough;
	MSPeriod		mPinPeriod;
	MSPeriod		mCommandTimeout;
	MSPeriod		mCheckLevelsPeriod;
	HardwareSerial&	mSerial;
	static const uint32_t	kBaudRate;	// Initial, after power up
	static const uint16_t	kRxBufferSize;
	static const uint16_t	kTxBufferSize;
	
	virtual void			MessageRead(
								const char*				inMessage,
								uint8_t					inMessageLen,
								const TPAddress&		inSender,
								const TPAddress&		inSMSCAddr);
	virtual void			ProcessQueuedSMSReply(void);
	virtual void			HandleNoSIMCardFound(void){}
	void					HandleCommandTimeout(void);
	void					HandleCommandResponse(void);
	void					HandleCommandCompleted(void);
	bool					HandleMultiLineCommand(void);
	void					ParseCommandResponse(void);
	void					ParseOtherCommandResponse(void);
	void					UpdateTime(
								uint32_t				inTime);
	void					HandleCommandFailed(void);
	void					FlushRxBuffer(void);
	void					PauseRx(void);
	void					ResumeRx(void);
	void					SendSMSMessage(void);
};

#endif